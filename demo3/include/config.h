/**
 * @file config.h
 * @brief 用户配置文件 - 所有可调参数集中在此处
 * 
 * 修改后需要重新编译：cmake --build build --config Release --target df2d
 */

#pragma once

#include <vector>
#include <string>

/**
 * @namespace config
 * @brief 用户配置命名空间
 */
namespace config {

    /**
     * @defgroup case_config Case 选择配置
     * @{
     */
    
    /** 
     * @brief 运行模式
     * - true  = 只运行 get_case_list() 中指定的 case
     * - false = 自动扫描目录下所有 input_parameter_X.txt 文件（默认）
     */
    constexpr bool USE_CASE_LIST = true;
    
    /**
     * @brief 获取要运行的 case 编号列表
     * @return case 编号的向量
     * @note 仅当 USE_CASE_LIST = true 时生效
     */
    inline std::vector<int> get_case_list() {
        return {5};
    }
    /** @} */

    /**
     * @defgroup restart_config 重启控制配置
     * @{
     */
    
    /**
     * @brief 强制重启模式
     * - true  = 忽略所有检查点，所有 case 从头开始计算
     * - false = 正常模式，存在检查点则恢复（默认）
     */
    constexpr bool FORCE_RESTART_ALL = false;
    /** @} */

    /**
     * @defgroup parallel_config 并行计算配置
     * @{
     */
    
    /**
     * @brief OpenMP 线程数
     * - 0  = 自动检测，使用系统最大可用线程数（默认）
     * - >0 = 使用指定的线程数
     */
    constexpr int NUM_THREADS = 17;
    /** @} */

    /**
     * @defgroup convergence_config 收敛判断配置
     * @{
     */
    
    /**
     * @brief 收敛阈值
     * 
     * 当 |eta_ave - eta_eq| / eta_eq < CONVERGENCE_THRESHOLD 时判定为收敛
     * - 默认值 1e-2 表示相对误差小于 1%
     * - 更严格的判断可设为 1e-3 或 1e-4
     */
    constexpr double CONVERGENCE_THRESHOLD = 1e-3;
    /** @} */

    /**
     * @defgroup checkpoint_config 检查点配置
     * @{
     */
    
    /**
     * @brief 检查点保存间隔（迭代步数）
     * - 较小的值（如 10000）：更频繁保存，中断后损失少
     * - 较大的值（如 100000）：保存频率低，性能更好
     */
    constexpr long CHECKPOINT_INTERVAL = 50000;
    /** @} */

    /**
     * @defgroup output_config 统计与输出配置
     * @{
     */
    
    /** @brief 统计间隔：每隔多少步计算一次区域平均 eta */
    constexpr long STATS_INTERVAL = 1000;

    /** @brief Stability check interval; output and checkpoint writes always force a check. */
    constexpr long STABILITY_CHECK_INTERVAL = 100;
    
    /** @brief 进度条宽度（字符数） */
    constexpr int PROGRESS_BAR_WIDTH = 25;
    
    /** @brief 输出 MATLAB 格式 (.m 文件) */
    constexpr bool OUTPUT_MATLAB = true;
    
    /** @brief 输出 Tecplot 格式 (.dat 文件) */
    constexpr bool OUTPUT_TECPLOT = false;
    /** @} */

    /**
     * @defgroup dense_dump_config Dense single-period MATLAB dump config
     * @{
     */

    /** @brief Enable dense cc/ee dumps over one flow period after a global trigger time. */
    constexpr bool ENABLE_DENSE_DUMP = true;

    /** @brief Global simulation time that starts the one-period dense sampling window. */
    constexpr double T_DENSE_DUMP_START = 1.0;

    /** @brief Number of snapshots across one period, including both endpoints. */
    constexpr int DENSE_DUMP_COUNT = 8;
    /** @} */

    /**
     * @defgroup dir_config 目录配置
     * @{
     */
    
    /** @brief 输入文件目录（设为 "" 表示当前目录） */
    inline std::string INPUT_DIR = "input";
    
    /** @brief 输出根目录 */
    inline std::string OUTPUT_DIR = "output";
    
    /** @brief 检查点目录 */
    inline std::string CHECKPOINT_DIR = "output";
    /** @} */

}  // namespace config
