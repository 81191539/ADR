/*****************************************************************************
 * main_cuda.cpp
 *
 * CUDA entry point. Built separately as df2d_cuda.
 *****************************************************************************/

#include "runtime.h"

int main(int argc, char* argv[])
{
    ExecutionConfig exec_config;
    exec_config.backend = ComputeBackend::Cuda;
    exec_config.device_id = 0;
    exec_config.gpu_reduce_stats = true;
    return run_cases_with_args(exec_config, argc, argv);
}
