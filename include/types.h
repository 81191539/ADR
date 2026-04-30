/**
 * @file types.h
 * @brief Core types and field storage for the 2D diffusion-convection solver.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

/**
 * @namespace phys
 * @brief Physical constants.
 */
namespace phys {
    constexpr double PI        = 3.14159265358979323846;
    constexpr double SQRT2     = 1.4142135623730950488;
    constexpr double ATANH_0_9 = 1.4722194895832204;
}

/**
 * @namespace sim
 * @brief Simulation constants.
 */
namespace sim {
    constexpr double DT_SHRINK  = 1.0 / phys::SQRT2;
    constexpr double EPS_EPOCH  = 1e-3;
    constexpr int    MAX_ADJUST = 1000;
    constexpr double SC_DEFAULT = 16667.0;
}

/**
 * @enum ComputeBackend
 * @brief Execution backend selection.
 */
enum class ComputeBackend {
    Cpu,
    Cuda
};

/**
 * @struct ExecutionConfig
 * @brief Runtime execution settings shared by CPU and GPU entry points.
 */
struct ExecutionConfig {
    ComputeBackend backend{ComputeBackend::Cpu};
    int            device_id{0};
    bool           gpu_reduce_stats{true};
};

/**
 * @struct Params
 * @brief Input parameters loaded from input_parameter_X.txt.
 */
struct Params {
    double lam{};
    long   ny{};
    double Pe{};
    double Pe2{};
    double alpha{};
    double eps{};
    double Da{};
    double K0{};
    double xpo_l{};
    double xpo_r{};
    double endT{};
    long   total_count{};
    double coeff_dt{};
    double x_ini_posi{};
    int    id{};
    double Pe1{};
    double f{};
};

/**
 * @struct GridInfo
 * @brief Derived mesh information.
 */
struct GridInfo {
    long   nx{};
    long   ny{};
    double h{};
    double xleft{};
    double xright{};
    double yleft{};
    double yright{};
};

/**
 * @struct PhysicsParams
 * @brief Physical parameters used directly by the PDE.
 */
struct PhysicsParams {
    double lam{};
    double Pe{};
    double Pe2{};
    double eps{};
    double Da{};
    double K0{};
    double c0{};
    double alpha{};
    double Sc{};
};

/**
 * @struct AdsorptionZone
 * @brief Physical adsorption interval.
 */
struct AdsorptionZone {
    double xpo_l{};
    double xpo_r{};
};

/**
 * @class Field1D
 * @brief 1D contiguous storage with explicit ghost cells.
 */
class Field1D {
public:
    Field1D() = default;

    void resize(long begin, long end, long ghost = 1)
    {
        begin_ = begin;
        end_ = end;
        ghost_ = ghost;
        lower_ = begin_ - ghost_;
        upper_ = end_ + ghost_;
        offset_ = -lower_;
        data_.assign(static_cast<std::size_t>(upper_ - lower_ + 1), 0.0);
    }

    void fill(double value)
    {
        std::fill(data_.begin(), data_.end(), value);
    }

    double& operator()(long i)
    {
        return data_[static_cast<std::size_t>(i + offset_)];
    }

    const double& operator()(long i) const
    {
        return data_[static_cast<std::size_t>(i + offset_)];
    }

    long begin() const { return begin_; }
    long end() const { return end_; }
    long lower_bound() const { return lower_; }
    long upper_bound() const { return upper_; }
    long ghost() const { return ghost_; }
    long extent() const { return upper_ - lower_ + 1; }

    double* data() { return data_.data(); }
    const double* data() const { return data_.data(); }

    double* physical_data() { return data_.data() + (begin_ + offset_); }
    const double* physical_data() const { return data_.data() + (begin_ + offset_); }

    std::size_t bytes() const { return data_.size() * sizeof(double); }

private:
    long begin_{0};
    long end_{-1};
    long ghost_{0};
    long lower_{0};
    long upper_{-1};
    long offset_{0};
    std::vector<double> data_;
};

/**
 * @class Field2D
 * @brief 2D contiguous storage with explicit ghost cells.
 */
class Field2D {
public:
    Field2D() = default;

    void resize(long x_begin, long x_end,
                long y_begin, long y_end,
                long ghost_x = 1, long ghost_y = 1)
    {
        x_begin_ = x_begin;
        x_end_ = x_end;
        y_begin_ = y_begin;
        y_end_ = y_end;
        ghost_x_ = ghost_x;
        ghost_y_ = ghost_y;
        x_lower_ = x_begin_ - ghost_x_;
        x_upper_ = x_end_ + ghost_x_;
        y_lower_ = y_begin_ - ghost_y_;
        y_upper_ = y_end_ + ghost_y_;
        pitch_ = y_upper_ - y_lower_ + 1;
        const long total_x = x_upper_ - x_lower_ + 1;
        data_.assign(static_cast<std::size_t>(total_x * pitch_), 0.0);
    }

    void fill(double value)
    {
        std::fill(data_.begin(), data_.end(), value);
    }

    double& operator()(long i, long j)
    {
        return data_[index(i, j)];
    }

    const double& operator()(long i, long j) const
    {
        return data_[index(i, j)];
    }

    long x_begin() const { return x_begin_; }
    long x_end() const { return x_end_; }
    long y_begin() const { return y_begin_; }
    long y_end() const { return y_end_; }
    long x_lower_bound() const { return x_lower_; }
    long x_upper_bound() const { return x_upper_; }
    long y_lower_bound() const { return y_lower_; }
    long y_upper_bound() const { return y_upper_; }
    long pitch() const { return pitch_; }
    long total_x() const { return x_upper_ - x_lower_ + 1; }
    long total_y() const { return y_upper_ - y_lower_ + 1; }

    double* data() { return data_.data(); }
    const double* data() const { return data_.data(); }

    double* row_data(long i)
    {
        return data_.data() + index(i, y_lower_);
    }

    const double* row_data(long i) const
    {
        return data_.data() + index(i, y_lower_);
    }

    double* physical_row_data(long i)
    {
        return data_.data() + index(i, y_begin_);
    }

    const double* physical_row_data(long i) const
    {
        return data_.data() + index(i, y_begin_);
    }

    std::size_t bytes() const { return data_.size() * sizeof(double); }

private:
    std::size_t index(long i, long j) const
    {
        const long x_index = i - x_lower_;
        const long y_index = j - y_lower_;
        return static_cast<std::size_t>(x_index * pitch_ + y_index);
    }

    long x_begin_{0};
    long x_end_{-1};
    long y_begin_{0};
    long y_end_{-1};
    long ghost_x_{0};
    long ghost_y_{0};
    long x_lower_{0};
    long x_upper_{-1};
    long y_lower_{0};
    long y_upper_{-1};
    long pitch_{0};
    std::vector<double> data_;
};

/**
 * @struct SimFields
 * @brief Backend-neutral host representation of simulation fields.
 */
struct SimFields {
    Field2D cc;
    Field2D nc;
    Field2D adv_c;
    Field1D ee;
    Field1D ne;
    Field1D yy;
    Field1D ff;
    Field1D xx;

    void resize(long nx, long ny, long ghost = 1)
    {
        cc.resize(0, nx, 0, ny, ghost, ghost);
        nc.resize(0, nx, 0, ny, ghost, ghost);
        adv_c.resize(0, nx, 0, ny, ghost, ghost);
        ee.resize(0, nx, ghost);
        ne.resize(0, nx, ghost);
        xx.resize(0, nx, ghost);
        yy.resize(0, ny, ghost);
        ff.resize(0, ny, ghost);
    }

    void zero_all()
    {
        cc.fill(0.0);
        nc.fill(0.0);
        adv_c.fill(0.0);
        ee.fill(0.0);
        ne.fill(0.0);
        xx.fill(0.0);
        yy.fill(0.0);
        ff.fill(0.0);
    }
};

/**
 * @struct RunLog
 * @brief Simulation runtime diagnostics.
 */
struct RunLog {
    struct DtAdjustment {
        long   iteration{};
        double old_dt{};
        double new_dt{};
        double sim_time{};
    };

    struct ConvergencePoint {
        long   iteration{};
        double sim_time{};
        double eta_ave{};
        double rel_err{};
    };

    std::vector<DtAdjustment>    dt_history;
    std::vector<ConvergencePoint> convergence_history;

    double time_init{};
    double time_compute{};
    double time_io{};
    double time_total{};

    long   actual_iterations{};
    int    nan_events{};
    int    output_count{};
    bool   converged{};
    double final_eta{};
    double final_rel_err{};
    double final_sim_time{};

    bool resumed_from_checkpoint{};
    long resumed_at_iteration{};
};

/**
 * @struct Checkpoint
 * @brief Binary checkpoint payload.
 */
struct Checkpoint {
    long   iteration{};
    double sim_time{};
    double dt{};
    int    output_count{};
    int    dt_adjustments{};
    double eta_ave{};
    double old_eta{};
    int    case_number{};
    double coeff_dt{};

    static constexpr uint32_t MAGIC   = 0x43484B50;
    static constexpr uint32_t VERSION = 1;
};
