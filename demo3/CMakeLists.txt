cmake_minimum_required(VERSION 3.21)

project(ADR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_package(OpenMP REQUIRED)

function(adr_enable_native_optimizations target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target_name} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-march=native>)
    endif()
endfunction()

set(ADR_COMMON_SOURCES
    src/util.cpp
    src/solver.cpp
    src/backend.cpp
    src/cpu_backend.cpp
    src/runtime.cpp
    src/checkpoint.cpp
    src/io.cpp)

add_executable(df2d
    ${ADR_COMMON_SOURCES}
    src/main.cpp)

target_include_directories(df2d PRIVATE include)
target_link_libraries(df2d PRIVATE OpenMP::OpenMP_CXX)
adr_enable_native_optimizations(df2d)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    find_package(CUDAToolkit REQUIRED)

    add_executable(df2d_cuda
        ${ADR_COMMON_SOURCES}
        src/main_cuda.cpp
        src/cuda_backend.cu)

    target_include_directories(df2d_cuda PRIVATE include)
    target_compile_definitions(df2d_cuda PRIVATE ADR_ENABLE_CUDA)
    target_link_libraries(df2d_cuda PRIVATE OpenMP::OpenMP_CXX CUDA::cudart)
    adr_enable_native_optimizations(df2d_cuda)
    set_target_properties(df2d_cuda PROPERTIES
        CUDA_STANDARD 17
        CUDA_STANDARD_REQUIRED ON)
else()
    message(STATUS "CUDA compiler not found. Target df2d_cuda will not be generated.")
endif()

if(BUILD_TESTING)
    add_executable(adr_solver_tests
        tests/solver_cpu_test.cpp
        src/solver.cpp
        src/backend.cpp
        src/cpu_backend.cpp
        src/io.cpp)

    target_include_directories(adr_solver_tests PRIVATE include tests)
    target_link_libraries(adr_solver_tests PRIVATE OpenMP::OpenMP_CXX)
    adr_enable_native_optimizations(adr_solver_tests)

    add_test(NAME adr_solver_tests COMMAND adr_solver_tests)
endif()
