/*****************************************************************************
 * main.cpp
 *
 * CPU entry point. The legacy executable remains available as df2d.
 *****************************************************************************/

#include "runtime.h"

int main(int argc, char* argv[])
{
    ExecutionConfig exec_config;
    exec_config.backend = ComputeBackend::Cpu;
    exec_config.device_id = 0;
    exec_config.gpu_reduce_stats = false;
    return run_cases_with_args(exec_config, argc, argv);
}
