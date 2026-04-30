/*****************************************************************************
 * cuda_backend.cu
 *
 * CUDA solver backend for the diffusion-convection solver.
 *****************************************************************************/

#include "backend.h"
#include "solver.h"

#include <cuda_runtime.h>

#include <memory>
#include <sstream>
#include <stdexcept>

namespace {

struct DeviceField1D {
    double* data{};
    long    begin{};
    long    end{};
    long    lower{};
    long    upper{};
};

struct DeviceField2D {
    double* data{};
    long    x_begin{};
    long    x_end{};
    long    y_begin{};
    long    y_end{};
    long    x_lower{};
    long    x_upper{};
    long    y_lower{};
    long    y_upper{};
    long    pitch{};
};

struct DeviceState {
    DeviceField2D cc;
    DeviceField2D nc;
    DeviceField2D adv_c;
    DeviceField1D ee;
    DeviceField1D ne;
    DeviceField1D yy;
    DeviceField1D ff;
};

inline void cuda_check(cudaError_t error, const char* call, const char* file, int line)
{
    if (error == cudaSuccess) {
        return;
    }

    std::ostringstream oss;
    oss << "CUDA error at " << file << ":" << line
        << " for " << call << ": " << cudaGetErrorString(error);
    throw std::runtime_error(oss.str());
}

#define CUDA_CHECK(call) cuda_check((call), #call, __FILE__, __LINE__)

DeviceField1D make_device_field(const Field1D& field, double* ptr)
{
    DeviceField1D device_field;
    device_field.data = ptr;
    device_field.begin = field.begin();
    device_field.end = field.end();
    device_field.lower = field.lower_bound();
    device_field.upper = field.upper_bound();
    return device_field;
}

DeviceField2D make_device_field(const Field2D& field, double* ptr)
{
    DeviceField2D device_field;
    device_field.data = ptr;
    device_field.x_begin = field.x_begin();
    device_field.x_end = field.x_end();
    device_field.y_begin = field.y_begin();
    device_field.y_end = field.y_end();
    device_field.x_lower = field.x_lower_bound();
    device_field.x_upper = field.x_upper_bound();
    device_field.y_lower = field.y_lower_bound();
    device_field.y_upper = field.y_upper_bound();
    device_field.pitch = field.pitch();
    return device_field;
}

__device__ inline double& at(DeviceField1D field, long i)
{
    return field.data[i - field.lower];
}

__device__ inline const double& at_const(DeviceField1D field, long i)
{
    return field.data[i - field.lower];
}

__device__ inline double& at(DeviceField2D field, long i, long j)
{
    return field.data[(i - field.x_lower) * field.pitch + (j - field.y_lower)];
}

__device__ inline const double& at_const(DeviceField2D field, long i, long j)
{
    return field.data[(i - field.x_lower) * field.pitch + (j - field.y_lower)];
}

__global__ void calc_eta_kernel(DeviceField1D ee, DeviceField1D ne,
                                DeviceField2D cc, double dt,
                                long nx, double h,
                                double xpo_l, double xpo_r,
                                double eps, double Da, double K0)
{
    const long i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i > nx) {
        return;
    }

    const double x = static_cast<double>(i) * h;
    if (x > xpo_l && x <= xpo_r) {
        const double coeff = eps * Da;
        const double K0_inv = 1.0 / K0;
        at(ne, i) = at_const(ee, i) +
                    dt * coeff *
                        (at_const(cc, i, 0) * (1.0 - at_const(ee, i)) -
                         at_const(ee, i) * K0_inv);
    } else {
        at(ne, i) = 0.0;
    }
}

__global__ void augment_phi_lr_kernel(DeviceField2D cc, long ny, double c0, long nx)
{
    const long j = static_cast<long>(blockIdx.x * blockDim.x + threadIdx.x) + cc.y_lower;
    if (j > cc.y_upper) {
        return;
    }

    at(cc, -1, j) = c0;
    at(cc, 0, j) = c0;
    at(cc, nx + 1, j) = at_const(cc, nx, j);
}

__global__ void augment_phi_vertical_kernel(DeviceField2D cc, DeviceField1D ee,
                                            long nx, long ny, double h,
                                            double Da, double K0,
                                            double xpo_l, double xpo_r)
{
    const long i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i > nx) {
        return;
    }

    at(cc, i, ny + 1) = at_const(cc, i, ny);

    const double x = static_cast<double>(i) * h;
    if (x > xpo_l && x <= xpo_r) {
        const double K0_inv = 1.0 / K0;
        at(cc, i, -1) =
            at_const(cc, i, 0) -
            h * Da * (at_const(cc, i, 0) * (1.0 - at_const(ee, i)) -
                      at_const(ee, i) * K0_inv);
    } else {
        at(cc, i, -1) = at_const(cc, i, 0);
    }
}

__global__ void oscillatory_kernel(double alpha, double Sc,
                                   DeviceField1D ff, DeviceField1D yy,
                                   double ct, long ny)
{
    const long j = blockIdx.x * blockDim.x + threadIdx.x;
    if (j > ny) {
        return;
    }

    const double ca = cos(alpha);
    const double sa = sin(alpha);
    const double ch = cosh(alpha);
    const double sh = sinh(alpha);
    const double c2 = cos(2.0 * alpha);
    const double ch2 = cosh(2.0 * alpha);
    const double a2 = alpha * alpha;
    const double dcc = 1.0 / (c2 + ch2);
    const double da2 = 1.0 / a2;
    const double sat = sin(2.0 * a2 * Sc * ct);
    const double cat = cos(2.0 * a2 * Sc * ct);
    const double c2_ch2_sat = (c2 + ch2) * sat;
    const double two_sa_sh = 2.0 * sa * sh;
    const double two_ca_ch = 2.0 * ca * ch;
    const double coeff = dcc * da2;

    const double arg = 2.0 * alpha * (at_const(yy, j) - 0.5);
    const double say = sin(arg);
    const double cay = cos(arg);
    const double shay = sinh(arg);
    const double chay = cosh(arg);

    at(ff, j) = coeff * (c2_ch2_sat
             + two_sa_sh * cay * cat * chay
             - two_sa_sh * say * sat * shay
             - two_ca_ch * (cay * chay * sat + cat * say * shay));
}

__global__ void advection_kernel(DeviceField2D oc, DeviceField2D adv_c,
                                 DeviceField1D yy, DeviceField1D ff,
                                 long nx, long ny, double h,
                                 double Pe, double Pe2)
{
    const long i = blockIdx.x * blockDim.x + threadIdx.x;
    const long j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i > nx || j > ny) {
        return;
    }

    const double du = Pe * at_const(yy, j) * (1.0 - at_const(yy, j)) + Pe2 * at_const(ff, j);
    const double h_inv = 1.0 / h;
    if (du > 0.0) {
        at(adv_c, i, j) = du * (at_const(oc, i, j) - at_const(oc, i - 1, j)) * h_inv;
    } else {
        at(adv_c, i, j) = du * (at_const(oc, i + 1, j) - at_const(oc, i, j)) * h_inv;
    }
}

__global__ void calc_phi_kernel(DeviceField2D cc, DeviceField2D nc,
                                DeviceField2D adv_c, double dt,
                                long nx, long ny, double h)
{
    const long i = blockIdx.x * blockDim.x + threadIdx.x;
    const long j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i > nx || j > ny) {
        return;
    }

    const double h2_inv = 1.0 / (h * h);
    const double lap =
        ((at_const(cc, i + 1, j) - 2.0 * at_const(cc, i, j) + at_const(cc, i - 1, j)) +
         (at_const(cc, i, j + 1) - 2.0 * at_const(cc, i, j) + at_const(cc, i, j - 1))) * h2_inv;
    const double sr = -at_const(adv_c, i, j);
    at(nc, i, j) = at_const(cc, i, j) + dt * (lap + sr);
}

__global__ void copy_vector_kernel(DeviceField1D dst, DeviceField1D src, long end)
{
    const long i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i > end) {
        return;
    }
    at(dst, i) = at_const(src, i);
}

__global__ void copy_matrix_kernel(DeviceField2D dst, DeviceField2D src, long nx, long ny)
{
    const long i = blockIdx.x * blockDim.x + threadIdx.x;
    const long j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i > nx || j > ny) {
        return;
    }
    at(dst, i, j) = at_const(src, i, j);
}

__global__ void nan_check_kernel(DeviceField2D field, long nx, long ny, int* has_nan)
{
    const long i = blockIdx.x * blockDim.x + threadIdx.x;
    const long j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i > nx || j > ny) {
        return;
    }

    if (isnan(at_const(field, i, j))) {
        atomicExch(has_nan, 1);
    }
}

__global__ void eta_average_kernel(DeviceField1D eta,
                                   double xleft, double h,
                                   double xpo_l, double xpo_r,
                                   long nx,
                                   double* sum,
                                   unsigned long long* count)
{
    const long i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i > nx) {
        return;
    }

    const double x = xleft + static_cast<double>(i) * h;
    if (x > xpo_l && x < xpo_r) {
        atomicAdd(sum, at_const(eta, i));
        atomicAdd(count, 1ULL);
    }
}

class CudaSolverBackend final : public SolverBackend {
public:
    explicit CudaSolverBackend(const ExecutionConfig& config)
        : device_id_(config.device_id)
        , gpu_reduce_stats_(config.gpu_reduce_stats)
    {
        CUDA_CHECK(cudaSetDevice(device_id_));
    }

    ~CudaSolverBackend() override
    {
        release();
    }

    const char* name() const override
    {
        return "CUDA";
    }

    ComputeBackend kind() const override
    {
        return ComputeBackend::Cuda;
    }

    void initialize(SimFields& fields,
                    const GridInfo& grid,
                    double x_ini_posi) override
    {
        ensure_allocated(fields);
        ::initialization(fields, x_ini_posi, grid);
        sync_device(fields);
    }

    void full_step_explicit(SimFields& fields,
                            const GridInfo& grid,
                            const PhysicsParams& phys,
                            const AdsorptionZone& zone,
                            double ct, double dt) override
    {
        ensure_allocated(fields);

        const int threads_1d = 256;
        const dim3 threads_2d(16, 16);

        const int blocks_x = static_cast<int>((grid.nx + threads_1d) / threads_1d);
        const int blocks_y = static_cast<int>((grid.ny + threads_1d) / threads_1d);
        const int blocks_y_all =
            static_cast<int>((fields.cc.y_upper_bound() - fields.cc.y_lower_bound() + threads_1d) /
                             threads_1d);
        const dim3 blocks_2d(
            static_cast<unsigned int>((grid.nx + threads_2d.x) / threads_2d.x),
            static_cast<unsigned int>((grid.ny + threads_2d.y) / threads_2d.y));

        calc_eta_kernel<<<blocks_x, threads_1d>>>(
            device_state_.ee, device_state_.ne, device_state_.cc, dt,
            grid.nx, grid.h, zone.xpo_l, zone.xpo_r,
            phys.eps, phys.Da, phys.K0);
        CUDA_CHECK(cudaGetLastError());

        augment_phi_lr_kernel<<<blocks_y_all, threads_1d>>>(
            device_state_.cc, grid.ny, phys.c0, grid.nx);
        CUDA_CHECK(cudaGetLastError());

        augment_phi_vertical_kernel<<<blocks_x, threads_1d>>>(
            device_state_.cc, device_state_.ee, grid.nx, grid.ny, grid.h,
            phys.Da, phys.K0, zone.xpo_l, zone.xpo_r);
        CUDA_CHECK(cudaGetLastError());

        oscillatory_kernel<<<blocks_y, threads_1d>>>(
            phys.alpha, phys.Sc, device_state_.ff, device_state_.yy, ct, grid.ny);
        CUDA_CHECK(cudaGetLastError());

        advection_kernel<<<blocks_2d, threads_2d>>>(
            device_state_.cc, device_state_.adv_c,
            device_state_.yy, device_state_.ff,
            grid.nx, grid.ny, grid.h, phys.Pe, phys.Pe2);
        CUDA_CHECK(cudaGetLastError());

        calc_phi_kernel<<<blocks_2d, threads_2d>>>(
            device_state_.cc, device_state_.nc, device_state_.adv_c,
            dt, grid.nx, grid.ny, grid.h);
        CUDA_CHECK(cudaGetLastError());

        copy_vector_kernel<<<blocks_x, threads_1d>>>(
            device_state_.ee, device_state_.ne, grid.nx);
        CUDA_CHECK(cudaGetLastError());

        copy_matrix_kernel<<<blocks_2d, threads_2d>>>(
            device_state_.cc, device_state_.nc, grid.nx, grid.ny);
        CUDA_CHECK(cudaGetLastError());
    }

    bool has_nan(SimFields& fields, const GridInfo& grid) override
    {
        ensure_allocated(fields);
        CUDA_CHECK(cudaMemset(d_has_nan_, 0, sizeof(int)));

        const dim3 threads(16, 16);
        const dim3 blocks(
            static_cast<unsigned int>((grid.nx + threads.x) / threads.x),
            static_cast<unsigned int>((grid.ny + threads.y) / threads.y));

        nan_check_kernel<<<blocks, threads>>>(device_state_.nc, grid.nx, grid.ny, d_has_nan_);
        CUDA_CHECK(cudaGetLastError());

        int host_has_nan = 0;
        CUDA_CHECK(cudaMemcpy(&host_has_nan, d_has_nan_, sizeof(int), cudaMemcpyDeviceToHost));
        return host_has_nan != 0;
    }

    double compute_eta_average(SimFields& fields,
                               const GridInfo& grid,
                               const AdsorptionZone& zone) override
    {
        ensure_allocated(fields);

        if (!gpu_reduce_stats_) {
            sync_host(fields);
            return ::compute_eta_average(fields.ne, grid, zone);
        }

        CUDA_CHECK(cudaMemset(d_eta_sum_, 0, sizeof(double)));
        CUDA_CHECK(cudaMemset(d_eta_count_, 0, sizeof(unsigned long long)));

        const int threads = 256;
        const int blocks = static_cast<int>((grid.nx + threads) / threads);
        eta_average_kernel<<<blocks, threads>>>(
            device_state_.ne, grid.xleft, grid.h,
            zone.xpo_l, zone.xpo_r, grid.nx, d_eta_sum_, d_eta_count_);
        CUDA_CHECK(cudaGetLastError());

        double host_sum = 0.0;
        unsigned long long host_count = 0;
        CUDA_CHECK(cudaMemcpy(&host_sum, d_eta_sum_, sizeof(double), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&host_count, d_eta_count_, sizeof(unsigned long long),
                              cudaMemcpyDeviceToHost));

        if (host_count == 0ULL) {
            return 0.0;
        }
        return host_sum / static_cast<double>(host_count);
    }

    void zero_state(SimFields& fields, const GridInfo&) override
    {
        fields.zero_all();
        if (!allocated_) {
            return;
        }

        CUDA_CHECK(cudaMemset(device_state_.cc.data, 0, fields.cc.bytes()));
        CUDA_CHECK(cudaMemset(device_state_.nc.data, 0, fields.nc.bytes()));
        CUDA_CHECK(cudaMemset(device_state_.adv_c.data, 0, fields.adv_c.bytes()));
        CUDA_CHECK(cudaMemset(device_state_.ee.data, 0, fields.ee.bytes()));
        CUDA_CHECK(cudaMemset(device_state_.ne.data, 0, fields.ne.bytes()));
        CUDA_CHECK(cudaMemset(device_state_.yy.data, 0, fields.yy.bytes()));
        CUDA_CHECK(cudaMemset(device_state_.ff.data, 0, fields.ff.bytes()));
    }

    void sync_host(SimFields& fields) override
    {
        ensure_allocated(fields);
        CUDA_CHECK(cudaMemcpy(fields.cc.data(), device_state_.cc.data,
                              fields.cc.bytes(), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(fields.nc.data(), device_state_.nc.data,
                              fields.nc.bytes(), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(fields.adv_c.data(), device_state_.adv_c.data,
                              fields.adv_c.bytes(), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(fields.ee.data(), device_state_.ee.data,
                              fields.ee.bytes(), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(fields.ne.data(), device_state_.ne.data,
                              fields.ne.bytes(), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(fields.yy.data(), device_state_.yy.data,
                              fields.yy.bytes(), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(fields.ff.data(), device_state_.ff.data,
                              fields.ff.bytes(), cudaMemcpyDeviceToHost));
    }

    void sync_device(SimFields& fields) override
    {
        ensure_allocated(fields);
        CUDA_CHECK(cudaMemcpy(device_state_.cc.data, fields.cc.data(),
                              fields.cc.bytes(), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(device_state_.nc.data, fields.nc.data(),
                              fields.nc.bytes(), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(device_state_.adv_c.data, fields.adv_c.data(),
                              fields.adv_c.bytes(), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(device_state_.ee.data, fields.ee.data(),
                              fields.ee.bytes(), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(device_state_.ne.data, fields.ne.data(),
                              fields.ne.bytes(), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(device_state_.yy.data, fields.yy.data(),
                              fields.yy.bytes(), cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(device_state_.ff.data, fields.ff.data(),
                              fields.ff.bytes(), cudaMemcpyHostToDevice));
    }

private:
    void allocate_buffer(double** ptr, std::size_t bytes)
    {
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(ptr), bytes));
    }

    void ensure_allocated(const SimFields& fields)
    {
        const long nx = fields.cc.x_end();
        const long ny = fields.cc.y_end();
        if (allocated_ && nx == nx_ && ny == ny_) {
            return;
        }

        release();

        allocate_buffer(&device_state_.cc.data, fields.cc.bytes());
        allocate_buffer(&device_state_.nc.data, fields.nc.bytes());
        allocate_buffer(&device_state_.adv_c.data, fields.adv_c.bytes());
        allocate_buffer(&device_state_.ee.data, fields.ee.bytes());
        allocate_buffer(&device_state_.ne.data, fields.ne.bytes());
        allocate_buffer(&device_state_.yy.data, fields.yy.bytes());
        allocate_buffer(&device_state_.ff.data, fields.ff.bytes());
        allocate_buffer(&d_eta_sum_, sizeof(double));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&d_eta_count_),
                              sizeof(unsigned long long)));
        CUDA_CHECK(cudaMalloc(reinterpret_cast<void**>(&d_has_nan_), sizeof(int)));

        device_state_.cc = make_device_field(fields.cc, device_state_.cc.data);
        device_state_.nc = make_device_field(fields.nc, device_state_.nc.data);
        device_state_.adv_c = make_device_field(fields.adv_c, device_state_.adv_c.data);
        device_state_.ee = make_device_field(fields.ee, device_state_.ee.data);
        device_state_.ne = make_device_field(fields.ne, device_state_.ne.data);
        device_state_.yy = make_device_field(fields.yy, device_state_.yy.data);
        device_state_.ff = make_device_field(fields.ff, device_state_.ff.data);

        nx_ = nx;
        ny_ = ny;
        allocated_ = true;
    }

    void release()
    {
        if (device_state_.cc.data != nullptr) {
            cudaFree(device_state_.cc.data);
            device_state_.cc.data = nullptr;
        }
        if (device_state_.nc.data != nullptr) {
            cudaFree(device_state_.nc.data);
            device_state_.nc.data = nullptr;
        }
        if (device_state_.adv_c.data != nullptr) {
            cudaFree(device_state_.adv_c.data);
            device_state_.adv_c.data = nullptr;
        }
        if (device_state_.ee.data != nullptr) {
            cudaFree(device_state_.ee.data);
            device_state_.ee.data = nullptr;
        }
        if (device_state_.ne.data != nullptr) {
            cudaFree(device_state_.ne.data);
            device_state_.ne.data = nullptr;
        }
        if (device_state_.yy.data != nullptr) {
            cudaFree(device_state_.yy.data);
            device_state_.yy.data = nullptr;
        }
        if (device_state_.ff.data != nullptr) {
            cudaFree(device_state_.ff.data);
            device_state_.ff.data = nullptr;
        }
        if (d_eta_sum_ != nullptr) {
            cudaFree(d_eta_sum_);
            d_eta_sum_ = nullptr;
        }
        if (d_eta_count_ != nullptr) {
            cudaFree(d_eta_count_);
            d_eta_count_ = nullptr;
        }
        if (d_has_nan_ != nullptr) {
            cudaFree(d_has_nan_);
            d_has_nan_ = nullptr;
        }

        allocated_ = false;
        nx_ = -1;
        ny_ = -1;
    }

    int device_id_{0};
    bool gpu_reduce_stats_{true};
    bool allocated_{false};
    long nx_{-1};
    long ny_{-1};
    DeviceState device_state_{};
    double* d_eta_sum_{nullptr};
    unsigned long long* d_eta_count_{nullptr};
    int* d_has_nan_{nullptr};
};

}  // namespace

std::unique_ptr<SolverBackend> create_cuda_backend(const ExecutionConfig& config)
{
    return std::make_unique<CudaSolverBackend>(config);
}
