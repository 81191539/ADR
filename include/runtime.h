/**
 * @file runtime.h
 * @brief Shared case runner used by CPU and CUDA executables.
 */

#pragma once

#include "types.h"

int run_cases(const ExecutionConfig& exec_config);
