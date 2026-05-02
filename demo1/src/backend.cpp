/*****************************************************************************
 * backend.cpp
 *
 * Solver backend factory.
 *****************************************************************************/

#include "backend.h"

#include <stdexcept>

std::unique_ptr<SolverBackend> create_backend(const ExecutionConfig& config)
{
    switch (config.backend) {
    case ComputeBackend::Cpu:
        return create_cpu_backend();
    case ComputeBackend::Cuda:
#ifdef ADR_ENABLE_CUDA
        return create_cuda_backend(config);
#else
        throw std::runtime_error(
            "CUDA backend requested, but this build does not include CUDA support.");
#endif
    default:
        throw std::runtime_error("Unknown compute backend.");
    }
}
