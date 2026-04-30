/**
 * @file backend.h
 * @brief Backend abstraction shared by CPU and CUDA solver implementations.
 */

#pragma once

#include <memory>

#include "types.h"

class SolverBackend {
public:
    virtual ~SolverBackend() = default;

    virtual const char* name() const = 0;
    virtual ComputeBackend kind() const = 0;

    virtual void initialize(SimFields& fields,
                            const GridInfo& grid,
                            double x_ini_posi) = 0;

    virtual void full_step_explicit(SimFields& fields,
                                    const GridInfo& grid,
                                    const PhysicsParams& phys,
                                    const AdsorptionZone& zone,
                                    double ct, double dt) = 0;

    virtual bool has_nan(SimFields& fields, const GridInfo& grid) = 0;

    virtual double compute_eta_average(SimFields& fields,
                                       const GridInfo& grid,
                                       const AdsorptionZone& zone) = 0;

    virtual void zero_state(SimFields& fields, const GridInfo& grid) = 0;
    virtual void sync_host(SimFields& fields) = 0;
    virtual void sync_device(SimFields& fields) = 0;
};

std::unique_ptr<SolverBackend> create_backend(const ExecutionConfig& config);
std::unique_ptr<SolverBackend> create_cpu_backend();

#ifdef ADR_ENABLE_CUDA
std::unique_ptr<SolverBackend> create_cuda_backend(const ExecutionConfig& config);
#endif
