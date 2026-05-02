/**
 * @file checkpoint.h
 * @brief 检查点功能 - 支持断点续算
 * 
 * 检查点文件格式：
 * - 文件名：checkpoint_X.bin（X 为 case 编号）
 * - 格式：二进制，包含魔数和版本号
 * - 内容：迭代状态 + 浓度场 cc + 表面覆盖率 ee
 */

#pragma once

#include <string>
#include "types.h"

/**
 * @brief 生成检查点文件名
 * @param[in] case_number case 编号
 * @return 检查点文件名，格式为 "checkpoint_X.bin"
 */
std::string get_checkpoint_filename(int case_number);

/**
 * @brief 检查检查点文件是否存在
 * @param[in] case_number case 编号
 * @return true=存在，false=不存在
 */
bool checkpoint_exists(int case_number);

/**
 * @brief 删除检查点文件
 * @param[in] case_number case 编号
 */
void delete_checkpoint(int case_number);

/**
 * @brief 检查是否强制重新开始
 * 
 * 检测是否存在 force_restart_X.txt 文件，如果存在则删除并返回 true
 * 
 * @param[in] case_number case 编号
 * @return true=需要强制重启，false=正常模式
 */
bool check_force_restart(int case_number);

/**
 * @brief 保存检查点
 * 
 * @param[in] filename 检查点文件路径
 * @param[in] chkpt    检查点数据结构
 * @param[in] cc       浓度场矩阵
 * @param[in] ee       表面覆盖率向量
 * @param[in] nx       x 方向网格数
 * @param[in] ny       y 方向网格数
 * @return true=保存成功，false=保存失败
 */
bool save_checkpoint(const char* filename, const Checkpoint& chkpt,
                     const Field2D& cc, const Field1D& ee, long nx, long ny);

/**
 * @brief 加载检查点
 * 
 * 加载前会验证魔数、版本号和网格尺寸
 * 
 * @param[in]  filename 检查点文件路径
 * @param[out] chkpt    检查点数据结构
 * @param[out] cc       浓度场矩阵（需预先分配）
 * @param[out] ee       表面覆盖率向量（需预先分配）
 * @param[in]  nx       期望的 x 方向网格数
 * @param[in]  ny       期望的 y 方向网格数
 * @return true=加载成功，false=加载失败
 */
bool load_checkpoint(const char* filename, Checkpoint& chkpt,
                     Field2D& cc, Field1D& ee, long nx, long ny);
