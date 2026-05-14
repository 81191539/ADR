/*****************************************************************************
 * solver.cpp
 *
 * CPU numerical kernels operating on backend-neutral field storage.
 *****************************************************************************/

#include "solver.h"

#include <algorithm>
#include <cmath>

#include <omp.h>

#if defined(_MSC_VER)
#define ADR_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define ADR_RESTRICT __restrict__
#else
#define ADR_RESTRICT
#endif

namespace {

inline void advection_row_kernel(const double* ADR_RESTRICT oc_im,
                                 const double* ADR_RESTRICT oc_i,
                                 const double* ADR_RESTRICT oc_ip,
                                 const double* ADR_RESTRICT yy_data,
                                 const double* ADR_RESTRICT ff_data,
                                 double* ADR_RESTRICT adv_i,
                                 long ny, double h_inv,
                                 double Pe, double Pe2)
{
    #pragma omp simd
    for (long j = 0; j <= ny; ++j) {
        const double y = yy_data[j];
        const double du = Pe * y * (1.0 - y) + Pe2 * ff_data[j];
        const double back_diff = (oc_i[j] - oc_im[j]) * h_inv;
        const double fwd_diff = (oc_ip[j] - oc_i[j]) * h_inv;
        const double pos = du > 0.0 ? du : 0.0;
        const double neg = du < 0.0 ? du : 0.0;
        adv_i[j] = pos * back_diff + neg * fwd_diff;
    }
}

inline double mc_limited_slope(double delta_left, double delta_right)
{
    if (delta_left * delta_right <= 0.0) {
        return 0.0;
    }
    const double centered = 0.5 * (delta_left + delta_right);
    const double limit = std::min(std::fabs(centered),
                                  2.0 * std::min(std::fabs(delta_left),
                                                 std::fabs(delta_right)));
    return std::copysign(limit, centered);
}

inline double mc_limited_slope_from_values(double left, double center, double right)
{
    return mc_limited_slope(center - left, right - center);
}

inline void advection_tvd_mc_row_kernel(const double* ADR_RESTRICT oc_imm,
                                        const double* ADR_RESTRICT oc_im,
                                        const double* ADR_RESTRICT oc_i,
                                        const double* ADR_RESTRICT oc_ip,
                                        const double* ADR_RESTRICT oc_ipp,
                                        const double* ADR_RESTRICT yy_data,
                                        const double* ADR_RESTRICT ff_data,
                                        double* ADR_RESTRICT adv_i,
                                        long i, long nx, long ny,
                                        double h_inv,
                                        double Pe, double Pe2)
{
    #pragma omp simd
    for (long j = 0; j <= ny; ++j) {
        const double y = yy_data[j];
        const double du = Pe * y * (1.0 - y) + Pe2 * ff_data[j];
        double flux_right = 0.0;
        double flux_left = 0.0;

        if (du >= 0.0) {
            double right_value = oc_i[j];
            if (i > 0 && i < nx) {
                right_value += 0.5 * mc_limited_slope_from_values(oc_im[j], oc_i[j], oc_ip[j]);
            }

            double left_value = oc_im[j];
            if (i >= 2) {
                left_value += 0.5 * mc_limited_slope_from_values(oc_imm[j], oc_im[j], oc_i[j]);
            }

            flux_right = du * right_value;
            flux_left = du * left_value;
        } else {
            double right_value = oc_ip[j];
            if (i <= nx - 2) {
                right_value -= 0.5 * mc_limited_slope_from_values(oc_i[j], oc_ip[j], oc_ipp[j]);
            }

            double left_value = oc_i[j];
            if (i > 0 && i < nx) {
                left_value -= 0.5 * mc_limited_slope_from_values(oc_im[j], oc_i[j], oc_ip[j]);
            }

            flux_right = du * right_value;
            flux_left = du * left_value;
        }

        adv_i[j] = (flux_right - flux_left) * h_inv;
    }
}

inline void calc_phi_row_kernel(const double* ADR_RESTRICT cc_im,
                                const double* ADR_RESTRICT cc_i,
                                const double* ADR_RESTRICT cc_ip,
                                const double* ADR_RESTRICT adv_i,
                                double* ADR_RESTRICT nc_i,
                                long ny, double h2_inv,
                                double dt)
{
    #pragma omp simd
    for (long j = 0; j <= ny; ++j) {
        const double lap =
            ((cc_ip[j] - 2.0 * cc_i[j] + cc_im[j]) +
             (cc_i[j + 1] - 2.0 * cc_i[j] + cc_i[j - 1])) * h2_inv;
        nc_i[j] = cc_i[j] + dt * (lap - adv_i[j]);
    }
}

}  // namespace

//-----------------------------------------------------------------------------
// Initialize coordinates and the concentration front.
//-----------------------------------------------------------------------------
void initialization(SimFields& fields,
                    double x_ini_posi,
                    const GridInfo& grid)
{
    for (long i = fields.xx.lower_bound(); i <= fields.xx.upper_bound(); ++i) {
        fields.xx(i) = grid.xleft + static_cast<double>(i) * grid.h;
    }

    for (long j = fields.yy.lower_bound(); j <= fields.yy.upper_bound(); ++j) {
        fields.yy(j) = grid.yleft + static_cast<double>(j) * grid.h;
    }

    const double dis = 2.0 * grid.h / (2.0 * phys::SQRT2 * phys::ATANH_0_9);

    #pragma omp parallel for schedule(static)
    for (long i = fields.cc.x_lower_bound(); i <= fields.cc.x_upper_bound(); ++i) {
        double* cc_i = fields.cc.physical_row_data(i);
        for (long j = fields.cc.y_lower_bound(); j <= fields.cc.y_upper_bound(); ++j) {
            const double x = grid.xleft + static_cast<double>(i) * grid.h;
            cc_i[j] = 0.5 *
                      (-std::tanh((x - x_ini_posi) / (phys::SQRT2 * dis)) + 1.0);
        }
    }
}

//-----------------------------------------------------------------------------
// Full explicit time step.
//-----------------------------------------------------------------------------
void full_step_explicit(SimFields& fields,
                        const GridInfo& grid,
                        const PhysicsParams& phys,
                        const AdsorptionZone& zone,
                        double ct, double dt,
                        AdvectionScheme advection_scheme)
{
    calc_eta(fields.ee, fields.ne, fields.cc, dt,
             grid.nx, grid.h, zone.xpo_l, zone.xpo_r,
             phys.eps, phys.Da, phys.K0);

    augment_phi(fields.cc, fields.ee,
                grid.nx, grid.ny, grid.h,
                phys.Da, phys.K0, zone.xpo_l, zone.xpo_r, phys.c0);

    oscillatory(phys.alpha, phys.Sc, fields.ff, fields.yy, ct, grid.ny);

    advection_c(fields.cc, fields.adv_c, fields.yy, fields.ff,
                grid.nx, grid.ny, grid.h, phys.Pe, phys.Pe2, advection_scheme);

    calc_phi(fields.cc, fields.nc, fields.adv_c, dt,
             grid.nx, grid.ny, grid.h);
}

//-----------------------------------------------------------------------------
// Surface coverage update.
//-----------------------------------------------------------------------------
void calc_eta(const Field1D& ee, Field1D& ne,
              const Field2D& cc, double dt,
              long nx, double h,
              double xpo_l, double xpo_r,
              double eps, double Da, double K0)
{
    const double K0_inv = 1.0 / K0;
    const double coeff = eps * Da;
    const double* ee_data = ee.physical_data();
    double* ne_data = ne.physical_data();

    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= nx; ++i) {
        const double x = static_cast<double>(i) * h;
        if (x > xpo_l && x <= xpo_r) {
            const double eta = ee_data[i];
            ne_data[i] = eta + dt * coeff *
                         (cc.physical_row_data(i)[0] * (1.0 - eta) - eta * K0_inv);
        } else {
            ne_data[i] = 0.0;
        }
    }
}

//-----------------------------------------------------------------------------
// Extend eta ghost cells.
//-----------------------------------------------------------------------------
void augment_eta(Field1D& ee, long nx)
{
    ee(-1) = ee(0);
    ee(nx + 1) = ee(nx);
}

//-----------------------------------------------------------------------------
// Boundary conditions for concentration.
//-----------------------------------------------------------------------------
void augment_phi(Field2D& cc, const Field1D& ee,
                 long nx, long ny, double h,
                 double Da, double K0,
                 double xpo_l, double xpo_r,
                 double c0)
{
    double* left_ghost = cc.physical_row_data(-1);
    double* left = cc.physical_row_data(0);
    const double* right = cc.physical_row_data(nx);
    double* right_ghost = cc.physical_row_data(nx + 1);
    for (long j = cc.y_lower_bound(); j <= cc.y_upper_bound(); ++j) {
        left_ghost[j] = c0;
        left[j] = c0;
        right_ghost[j] = right[j];
    }

    const double K0_inv = 1.0 / K0;
    const double h_Da = h * Da;
    const double* ee_data = ee.physical_data();

    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= nx; ++i) {
        double* cc_i = cc.physical_row_data(i);
        cc_i[ny + 1] = cc_i[ny];

        const double x = static_cast<double>(i) * h;
        if (x > xpo_l && x <= xpo_r) {
            const double eta = ee_data[i];
            cc_i[-1] = cc_i[0] -
                       h_Da * (cc_i[0] * (1.0 - eta) - eta * K0_inv);
        } else {
            cc_i[-1] = cc_i[0];
        }
    }
}

//-----------------------------------------------------------------------------
// Oscillatory velocity profile.
//-----------------------------------------------------------------------------
void oscillatory(double alpha, double Sc,
                 Field1D& ff, const Field1D& yy,
                 double ct, long ny)
{
    const double ca = std::cos(alpha);
    const double sa = std::sin(alpha);
    const double ch = std::cosh(alpha);
    const double sh = std::sinh(alpha);
    const double c2 = std::cos(2.0 * alpha);
    const double ch2 = std::cosh(2.0 * alpha);
    const double a2 = alpha * alpha;
    const double dcc = 1.0 / (c2 + ch2);
    const double da2 = 1.0 / a2;
    const double sat = std::sin(2.0 * a2 * Sc * ct);
    const double cat = std::cos(2.0 * a2 * Sc * ct);
    const double c2_ch2_sat = (c2 + ch2) * sat;
    const double two_sa_sh = 2.0 * sa * sh;
    const double two_ca_ch = 2.0 * ca * ch;
    const double coeff = dcc * da2;
    const double* yy_data = yy.physical_data();
    double* ff_data = ff.physical_data();

    #pragma omp parallel for schedule(static)
    for (long j = 0; j <= ny; ++j) {
        const double arg = 2.0 * alpha * (yy_data[j] - 0.5);
        const double say = std::sin(arg);
        const double cay = std::cos(arg);
        const double shay = std::sinh(arg);
        const double chay = std::cosh(arg);

        ff_data[j] = coeff * (c2_ch2_sat
                   + two_sa_sh * cay * cat * chay
                   - two_sa_sh * say * sat * shay
                   - two_ca_ch * (cay * chay * sat + cat * say * shay));
    }
}

//-----------------------------------------------------------------------------
// Upwind advection term.
//-----------------------------------------------------------------------------
void advection_c(const Field2D& oc, Field2D& adv_c,
                 const Field1D& yy, const Field1D& ff,
                 long nx, long ny, double h,
                 double Pe, double Pe2,
                 AdvectionScheme advection_scheme)
{
    const double h_inv = 1.0 / h;
    const double* yy_data = yy.physical_data();
    const double* ff_data = ff.physical_data();

    if (advection_scheme == AdvectionScheme::Upwind) {
        #pragma omp parallel for schedule(static)
        for (long i = 0; i <= nx; ++i) {
            const double* oc_im = oc.physical_row_data(i - 1);
            const double* oc_i = oc.physical_row_data(i);
            const double* oc_ip = oc.physical_row_data(i + 1);
            double* adv_i = adv_c.physical_row_data(i);
            advection_row_kernel(oc_im, oc_i, oc_ip, yy_data, ff_data, adv_i,
                                 ny, h_inv, Pe, Pe2);
        }
        return;
    }

    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= nx; ++i) {
        const double* oc_imm = i >= 2 ? oc.physical_row_data(i - 2) : nullptr;
        const double* oc_im = oc.physical_row_data(i - 1);
        const double* oc_i = oc.physical_row_data(i);
        const double* oc_ip = oc.physical_row_data(i + 1);
        const double* oc_ipp = i <= nx - 2 ? oc.physical_row_data(i + 2) : nullptr;
        double* adv_i = adv_c.physical_row_data(i);
        advection_tvd_mc_row_kernel(oc_imm, oc_im, oc_i, oc_ip, oc_ipp,
                                    yy_data, ff_data, adv_i,
                                    i, nx, ny, h_inv, Pe, Pe2);
    }
}

//-----------------------------------------------------------------------------
// Explicit FTCS concentration update.
//-----------------------------------------------------------------------------
void calc_phi(const Field2D& cc, Field2D& nc,
              const Field2D& adv_c, double dt,
              long nx, long ny, double h)
{
    const double h2_inv = 1.0 / (h * h);

    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= nx; ++i) {
        const double* cc_im = cc.physical_row_data(i - 1);
        const double* cc_i = cc.physical_row_data(i);
        const double* cc_ip = cc.physical_row_data(i + 1);
        const double* adv_i = adv_c.physical_row_data(i);
        double* nc_i = nc.physical_row_data(i);
        calc_phi_row_kernel(cc_im, cc_i, cc_ip, adv_i, nc_i, ny, h2_inv, dt);
    }
}

//-----------------------------------------------------------------------------
// Check whether a concentration field contains non-physical values.
//-----------------------------------------------------------------------------
bool has_unstable_values(const Field2D& field, long nx, long ny)
{
    constexpr double negative_tolerance = -1e-12;
    constexpr double max_reasonable_magnitude = 1e12;
    bool found_unstable = false;

    #pragma omp parallel for schedule(static) reduction(||:found_unstable)
    for (long i = 0; i <= nx; ++i) {
        const double* field_i = field.physical_row_data(i);
        for (long j = 0; j <= ny; ++j) {
            const double value = field_i[j];
            if (!std::isfinite(value) ||
                value < negative_tolerance ||
                std::fabs(value) > max_reasonable_magnitude) {
                found_unstable = true;
            }
        }
    }

    return found_unstable;
}

//-----------------------------------------------------------------------------
// Check whether eta contains non-physical surface coverage values.
// Eta is stored along x, so the upper index is nx.
//-----------------------------------------------------------------------------
bool has_unstable_eta(const Field1D& eta, long nx)
{
    constexpr double lower_tolerance = -1e-12;
    constexpr double upper_tolerance = 1.0 + 1e-6;
    bool found_unstable = false;

    #pragma omp parallel for schedule(static) reduction(||:found_unstable)
    for (long i = 0; i <= nx; ++i) {
        const double value = eta(i);
        if (!std::isfinite(value) ||
            value < lower_tolerance ||
            value > upper_tolerance) {
            found_unstable = true;
        }
    }

    return found_unstable;
}

//-----------------------------------------------------------------------------
// Average eta over the adsorption zone.
//-----------------------------------------------------------------------------
double compute_eta_average(const Field1D& eta,
                           const GridInfo& grid,
                           const AdsorptionZone& zone)
{
    double eta_ave = 0.0;
    long count = 0;
    const double* eta_data = eta.physical_data();

    for (long i = 0; i <= grid.nx; ++i) {
        const double x = grid.xleft + static_cast<double>(i) * grid.h;
        if (x > zone.xpo_l && x < zone.xpo_r) {
            eta_ave += eta_data[i];
            ++count;
        }
    }

    return (count > 0) ? eta_ave / static_cast<double>(count) : 0.0;
}

//-----------------------------------------------------------------------------
// Relaxation routine retained for future implicit schemes.
//-----------------------------------------------------------------------------
void relax(Field2D& p, const Field2D& oc, const Field2D& f,
           double dt, long nx, long ny,
           int p_relax, double lam, double h)
{
    const double lam2 = lam * lam;
    const double h2 = h * h;

    for (int iter = 1; iter <= p_relax; ++iter) {
        for (long i = 0; i <= nx; ++i) {
            for (long j = 0; j <= ny; ++j) {
                const double coef = 1.0 / dt + 2.0 * (1.0 + lam2) / h2;
                const double lap =
                    (lam2 * (p(i + 1, j) + p(i - 1, j)) +
                     (p(i, j + 1) + p(i, j - 1))) / h2;
                const double src = oc(i, j) / dt - f(i, j);
                p(i, j) = (lap + src) / coef;
            }
        }
    }
}
