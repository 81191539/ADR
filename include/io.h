/**
 * @file io.h
 * @brief 数据输入输出功能
 */

#pragma once

#include <string>
#include "types.h"

/**
 * @brief 读取输入参数
 * 
 * 读取文件名格式为 "input_parameter_X.txt"
 * 
 * @param[in] case_number case 编号
 * @return Params 结构体
 * @throws std::runtime_error 如果文件无法打开
 */
Params read_parameter(int case_number);

/**
 * @brief 输出浓度场和表面覆盖率数据（MATLAB 格式）
 * 
 * 输出文件：
 * - {buf}/cc_{count}.m: 浓度场矩阵
 * - {buf}/ee_{count}.m: 表面覆盖率 [x坐标, eta值]
 * 
 * @param[in] phi   浓度场矩阵
 * @param[in] eta   表面覆盖率向量
 * @param[in] xx    x 坐标向量
 * @param[in] count 输出文件编号
 * @param[in] buf   输出目录名
 * @param[in] nx    x 方向网格数
 * @param[in] ny    y 方向网格数
 */
void print_data(const Field2D& phi, const Field1D& eta, const Field1D& xx,
                int count, const char* buf,
                long nx, long ny);

/**
 * @brief 输出 Tecplot 格式数据
 * 
 * @param[in] cc     浓度场矩阵
 * @param[in] count  输出文件编号
 * @param[in] buf    输出目录名
 * @param[in] nx     x 方向网格数
 * @param[in] ny     y 方向网格数
 * @param[in] h      网格间距
 * @param[in] xleft  x 左边界坐标
 * @param[in] yleft  y 下边界坐标
 */
void print_tecplot_data(const Field2D& cc, int count, const char* buf,
                        long nx, long ny, double h,
                        double xleft, double yleft);

/**
 * @brief 统一数据输出接口
 * 
 * 根据 config.h 中的 OUTPUT_MATLAB 和 OUTPUT_TECPLOT 设置自动选择输出格式
 * 
 * @param[in] phi    浓度场矩阵
 * @param[in] eta    表面覆盖率向量
 * @param[in] xx     x 坐标向量
 * @param[in] count  输出文件编号
 * @param[in] buf    输出目录名
 * @param[in] nx     x 方向网格数
 * @param[in] ny     y 方向网格数
 * @param[in] h      网格间距
 * @param[in] xleft  x 左边界坐标
 * @param[in] yleft  y 下边界坐标
 */
void output_data(const Field2D& phi, const Field1D& eta, const Field1D& xx,
                 int count, const char* buf,
                 long nx, long ny, double h,
                 double xleft, double yleft);

/**
 * @brief 写入详细运行日志
 * 
 * 输出格式为 MATLAB 可读的 .m 文件
 * 
 * @param[in] fname_log   日志文件路径
 * @param[in] case_number case 编号
 * @param[in] p           输入参数
 * @param[in] grid        网格信息
 * @param[in] phys        物理参数
 * @param[in] zone        吸附区域
 * @param[in] log         运行日志
 * @param[in] dt_initial  初始时间步长
 */
void write_detailed_log(const char* fname_log, int case_number,
                        const Params& p, const GridInfo& grid,
                        const PhysicsParams& phys, const AdsorptionZone& zone,
                        const RunLog& log, double dt_initial);

/**
 * @brief 确保目录存在
 * 
 * 如果目录不存在，创建该目录（包括必要的父目录）
 * 
 * @param[in] dir 目录路径
 */
void ensure_dir(const std::string& dir);
