/*****************************************************************************
 *
 * 2D diffuse-convection equation with 1st, 2nd & 3rd boundary conditions
 * Explicit scheme
 * model reference: Effect of fluidic transport on the reaction kinetics in lectin microarrays,
 *                Analytica Chimica Acta 701 (2011) 6-14.
 *非对称，上表面不吸，输出吸附速率，输出tec
 *自动计算目录下所有case，自动调节dt
 *****************************************************************************/

#include <algorithm>
#include <cmath>  // std::isnan, std::tanh
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <string>
#include <vector>

#include <dirent.h>
#include <omp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "diffuse.h"  // 若有其他数学/数值例程，可继续保留
#include "util.h"     // 提供 dmatrix/dvector 及其 free_* 原型

#include <string>

#include <filesystem>
namespace fs = std::filesystem;

//=============================================================================
//                         用户配置区域 (USER CONFIGURATION)
//                    修改以下参数后重新编译即可生效
//=============================================================================

namespace config
{

//-------------------------------------------------------------------------
// Case 选择配置
//-------------------------------------------------------------------------

// 运行模式：
//   true  = 只运行 CASE_LIST 中指定的 case
//   false = 自动扫描目录下所有 input_parameter_X.txt 文件
constexpr bool USE_CASE_LIST = false;

// 指定要运行的 case 编号列表（仅当 USE_CASE_LIST = true 时生效）
// 例如：只运行 case 1, 3, 5
inline std::vector<int> get_case_list()
{
    return {1, 3, 5};
}

//-------------------------------------------------------------------------
// 重启控制配置
//-------------------------------------------------------------------------

// 强制重启模式：
//   true  = 忽略所有检查点，所有 case 从头开始计算
//   false = 正常模式，如果存在检查点则恢复（可通过 force_restart_X.txt 单独控制）
constexpr bool FORCE_RESTART_ALL = true;

//-------------------------------------------------------------------------
// 并行计算配置
//-------------------------------------------------------------------------

// OpenMP 线程数：
//   0  = 自动检测（使用系统最大可用线程数）
//   >0 = 使用指定的线程数
constexpr int NUM_THREADS = 0;

//-------------------------------------------------------------------------
// 收敛判断配置
//-------------------------------------------------------------------------

// 收敛阈值：当 |eta_ave - eta_eq| / eta_eq < CONVERGENCE_THRESHOLD 时判定为收敛
constexpr double CONVERGENCE_THRESHOLD = 1e-3;

//-------------------------------------------------------------------------
// 检查点配置
//-------------------------------------------------------------------------

// 检查点保存间隔（迭代步数）
// 较小的值：更频繁保存，更安全，但有轻微性能开销
// 较大的值：保存频率低，性能更好，但中断后可能损失更多进度
constexpr long CHECKPOINT_INTERVAL = 50000;

//-------------------------------------------------------------------------
// 统计与输出配置
//-------------------------------------------------------------------------

// 统计间隔：每隔多少步计算一次区域平均 eta 并更新进度条
constexpr long STATS_INTERVAL = 1000;

// 进度条宽度（字符数）
constexpr int PROGRESS_BAR_WIDTH = 25;

}  // namespace config

//=============================================================================
//                       用户配置区域结束
//=============================================================================

//=========================== 常量定义 ============================//
namespace phys
{
constexpr double PI = 3.14159265358979323846;
constexpr double SQRT2 = std::sqrt(2.0);
constexpr double ATANH_0_9 = std::atanh(0.9);  // ≈1.4722
}  // namespace phys
namespace sim
{
constexpr double DT_SHRINK = 1.0 / phys::SQRT2;  // dt 缩放因子 (~0.7071)
constexpr double EPS_EPOCH = 1e-3;               // 小偏移量 (原 0.001)
constexpr int MAX_ADJUST = 1000;                 // 最多自适应次数
constexpr double SC_DEFAULT = 16667.0;           // 默认 Schmidt 数
}  // namespace sim
//================================================================//

//======================== 参数结构体 ========================//

// 网格参数
struct GridInfo
{
    long nx;        // x方向网格数
    long ny;        // y方向网格数
    double h;       // 网格间距
    double xleft;   // x左边界
    double xright;  // x右边界
    double yleft;   // y下边界
    double yright;  // y上边界
};

// 物理参数
struct PhysicsParams
{
    double Pe;     // Peclet数（稳态流动）
    double Pe2;    // Peclet数（振荡流动）
    double eps;    // 表面反应参数 epsilon
    double Da;     // Damköhler数
    double K0;     // 平衡常数
    double c0;     // 入口浓度
    double alpha;  // Womersley数
    double Sc;     // Schmidt数
};

// 吸附区域边界
struct AdsorptionZone
{
    double xpo_l;  // 吸附区左边界
    double xpo_r;  // 吸附区右边界
};

// 仿真状态（场变量指针）
struct SimFields
{
    double **cc;     // 当前浓度场
    double **nc;     // 下一步浓度场
    double **adv_c;  // 对流项
    double *ee;      // 当前表面覆盖率
    double *ne;      // 下一步表面覆盖率
    double *yy;      // y坐标数组
    double *ff;      // 振荡速度分布
    double *xx;      // x坐标数组
};

// 日志记录结构体
struct RunLog
{
    // dt 调整记录
    struct DtAdjustment
    {
        long iteration;   // 发生调整的迭代步
        double old_dt;    // 调整前的 dt
        double new_dt;    // 调整后的 dt
        double sim_time;  // 发生时的模拟时间
    };
    std::vector<DtAdjustment> dt_history;

    // 收敛历史（记录关键节点）
    struct ConvergencePoint
    {
        long iteration;
        double sim_time;
        double eta_ave;
        double rel_err;
    };
    std::vector<ConvergencePoint> convergence_history;

    // 计时信息
    double time_init;     // 初始化耗时
    double time_compute;  // 计算耗时
    double time_io;       // IO 耗时
    double time_total;    // 总耗时

    // 运行统计
    long actual_iterations;  // 实际迭代次数
    int nan_events;          // NaN 发生次数
    int output_count;        // 输出文件数量
    bool converged;          // 是否收敛
    double final_eta;        // 最终 eta 值
    double final_rel_err;    // 最终相对误差
    double final_sim_time;   // 最终模拟时间

    bool resumed_from_checkpoint;  // 是否从检查点恢复
    long resumed_at_iteration;     // 恢复时的迭代步（如果恢复的话）

    // 更新构造函数
    RunLog()
        : time_init(0), time_compute(0), time_io(0), time_total(0), actual_iterations(0),
          nan_events(0), output_count(0), converged(false), final_eta(0), final_rel_err(0),
          final_sim_time(0), resumed_from_checkpoint(false), resumed_at_iteration(0)
    {
    }
};

// 检查点数据结构
struct Checkpoint
{
    // 时间循环状态
    long iteration;      // 当前迭代次数
    double sim_time;     // 当前模拟时间
    double dt;           // 当前时间步长
    int output_count;    // 已输出文件数量
    int dt_adjustments;  // dt 调整次数

    // 统计量
    double eta_ave;  // 当前平均 eta
    double old_eta;  // 上一次的 eta（用于计算变化率）

    // 文件标识
    int case_number;  // case 编号
    double coeff_dt;  // 当前 coeff_dt（可能已被调整）

    // 魔数和版本，用于验证文件有效性
    static constexpr uint32_t MAGIC = 0x43484B50;  // "CHKP"
    static constexpr uint32_t VERSION = 1;
};

//======================== 函数前向声明 ========================//
Params read_parameter(int case_number);
void initialization(double **cc, double *xx, double *yy, double x_ini_posi, long nx, long ny,
                    double h, double xleft, double yleft);
void case_calculation(int case_number, Params p);
void full_step_explicit(SimFields &fields, const GridInfo &grid, const PhysicsParams &phys,
                        const AdsorptionZone &zone, double ct, double dt);
void print_data(double **phi, const double *eta, const double *xx, int count, const char *buf,
                long nx, long ny);

void calc_eta(const double *ee, double *ne, double **cc, double dt, long nx, double h, double xpo_l,
              double xpo_r, double eps, double Da, double K0);
void augment_phi(double **cc, const double *ee, long nx, long ny, double h, double Da, double K0,
                 double xpo_l, double xpo_r, double c0);
void oscillatory(double alpha, double Sc, double *ff, const double *yy, double ct, long ny);
void advection_c(double **oc, double **adv_c, const double *yy, const double *ff, long nx, long ny,
                 double h, double Pe, double Pe2);
void calc_phi(double **cc, double **nc, double **adv_c, double dt, long nx, long ny, double h);
void write_detailed_log(const char *fname_log, int case_number, const Params &p,
                        const GridInfo &grid, const PhysicsParams &phys, const AdsorptionZone &zone,
                        const RunLog &log, double dt_initial);

// 检查点相关函数
bool save_checkpoint(const char *filename, const Checkpoint &chkpt, double **cc, double *ee,
                     long nx, long ny);
bool load_checkpoint(const char *filename, Checkpoint &chkpt, double **cc, double *ee, long nx,
                     long ny);
bool checkpoint_exists(int case_number);
std::string get_checkpoint_filename(int case_number);
void delete_checkpoint(int case_number);
bool check_force_restart(int case_number);  // 新增

// ---------- 参数读取 --------------------------------------------------
Params read_parameter(int case_number)
{
    Params p;
    int unused{};
    char fname[64];
    std::snprintf(fname, sizeof(fname), "input_parameter_%d.txt", case_number);

    std::ifstream in(fname);
    if (!in)
    {
        throw std::runtime_error(std::string("Cannot open ") + fname);
    }

    in >> unused >> p.lam >> p.Pe >> p.Pe2 >> p.eps >> p.Da >> p.K0 >> p.ny >> p.xpo_l >> p.xpo_r >>
        p.endT >> p.total_count >> p.coeff_dt >> p.x_ini_posi >> p.alpha;
    return p;
}

// ---------- 初始化 ----------------------------------------------------
void initialization(double **cc, double *xx, double *yy, double x_ini_posi, long nx, long ny,
                    double h, double xleft, double yleft)
{
    // 坐标
    xx[0] = xleft;
    for (long i = 0; i <= nx + 1; ++i)
        xx[i + 1] = xx[i] + h;
    xx[-1] = xleft - h;

    yy[0] = yleft;
    for (long j = 0; j <= ny + 1; ++j)
        yy[j + 1] = yy[j] + h;
    yy[-1] = yleft - h;

    // 初始浓度场
    const double dis = 2.0 * h / (2.0 * phys::SQRT2 * phys::ATANH_0_9);
    for (long i = 0; i <= nx + 1; ++i)
    {
        const double x = static_cast<double>(i) * h;
        for (long j = 0; j <= ny + 1; ++j)
        {
            cc[i][j] = 0.5 * (-std::tanh((x - x_ini_posi) / (phys::SQRT2 * dis)) + 1.0);
        }
    }
}

// ---------- case 计算 --------------------------------------------------
void case_calculation(int case_number, Params p)
{
    // --- 网格尺寸及派生量 ---
    const long nx = static_cast<long>(p.ny * (1.0 / p.lam));
    const double yleft = 0.0, yright = 1.0;
    const double xleft = 0.0, xright = (static_cast<double>(nx) / p.ny) * yright;
    const double h = (yright - yleft) / p.ny;
    double dt = p.coeff_dt * h * h;
    double dt_initial = dt;  // 保存初始 dt 用于日志
    double start = omp_get_wtime();
    double time_init_start = start;  // 初始化开始时间

    // 初始化日志记录器
    RunLog run_log;

    // Peclet/其他直接用 p.Pe 等
    double xl = p.xpo_l * xright;
    double xr = p.xpo_r * xright;

    long max_it = static_cast<long>(p.endT / dt + sim::EPS_EPOCH);
    long ns = static_cast<long>(max_it / p.total_count + sim::EPS_EPOCH);

    // ---------- 分配矩阵/向量 ----------
    auto cc = dmatrix(-1, nx + 1, -1, p.ny + 1);
    auto nc = dmatrix(-1, nx + 1, -1, p.ny + 1);
    auto adv_c = dmatrix(-1, nx + 1, -1, p.ny + 1);
    auto ee = dvector(-1, nx + 1);
    auto ne = dvector(-1, nx + 1);
    auto xx = dvector(-1, nx + 1);
    auto yy = dvector(-1, p.ny + 1);
    auto ff = dvector(-1, p.ny + 1);

    zero_matrix(cc, -1, nx + 1, -1, p.ny + 1);
    zero_matrix(nc, -1, nx + 1, -1, p.ny + 1);
    zero_matrix(adv_c, -1, nx + 1, -1, p.ny + 1);
    zero_vector(ee, -1, nx + 1);
    zero_vector(ne, -1, nx + 1);
    zero_vector(xx, -1, nx + 1);
    zero_vector(yy, -1, p.ny + 1);
    zero_vector(ff, -1, p.ny + 1);

    // ---------- 初始化参数结构体 ----------
    GridInfo grid;
    grid.nx = nx;
    grid.ny = p.ny;
    grid.h = h;
    grid.xleft = xleft;
    grid.xright = xright;
    grid.yleft = yleft;
    grid.yright = yright;

    PhysicsParams phys;
    phys.Pe = p.Pe;
    phys.Pe2 = p.Pe2;
    phys.eps = p.eps;
    phys.Da = p.Da;
    phys.K0 = p.K0;
    phys.c0 = 1.0;  // 入口浓度
    phys.alpha = p.alpha;
    phys.Sc = sim::SC_DEFAULT;

    AdsorptionZone zone;
    zone.xpo_l = xl;
    zone.xpo_r = xr;

    SimFields fields;
    fields.cc = cc;
    fields.nc = nc;
    fields.adv_c = adv_c;
    fields.ee = ee;
    fields.ne = ne;
    fields.yy = yy;
    fields.ff = ff;
    fields.xx = xx;

    initialization(cc, xx, yy, p.x_ini_posi, nx, p.ny, h, xleft, yleft);

    // 记录初始化耗时
    run_log.time_init = omp_get_wtime() - time_init_start;
    double time_compute_start = omp_get_wtime();

    // ---------- 输出文件名 ----------
    char fname_data[64], fname_eta[64], fname_log[64];
    std::snprintf(fname_data, sizeof(fname_data), "data_%d", case_number);
    std::snprintf(fname_eta, sizeof(fname_eta), "eta_ave_%d.m", case_number);
    std::snprintf(fname_log, sizeof(fname_log), "remarks_%d.m", case_number);

    // ---------- 日志初始化 ----------
    {
        std::FILE *fp = std::fopen(fname_log, "w");
        std::fprintf(fp, "case_number  = %d\n", case_number);
        std::fprintf(fp, "nx = %ld; ny = %ld; dt = %g;\n", nx, p.ny, dt);
        std::fprintf(fp, "endT = %g; Pe = %g; alpha = %g;\n", p.endT, p.Pe, p.alpha);
        std::fclose(fp);
    }
    {
        std::FILE *fp = std::fopen(fname_eta, "w");
        std::fprintf(fp, " %f  %f %f\n", 0.0, 0.0, 0.0);
        std::fclose(fp);
    }

    // ---------- 时间循环 ----------
    double ct = 0.0;
    int count = 1;
    double eta_ave = 0.0, old_eta = 0.0, eta_ave_dt = 0.0;
    int dt_adjustments = 0;
    // 检查点相关
    std::string chkpt_filename = get_checkpoint_filename(case_number);
    long start_iteration = 1;

    // ---------- 检查是否强制重新开始 ----------
    bool force_restart = config::FORCE_RESTART_ALL || check_force_restart(case_number);
    if (force_restart)
    {
#pragma omp critical
        {
            std::printf("[Case %d] Force restart requested, ignoring checkpoint\n", case_number);
        }
    }

    // ---------- 检查是否存在检查点，尝试恢复 ----------
    if (!force_restart && checkpoint_exists(case_number))
    {
        Checkpoint chkpt;
        if (load_checkpoint(chkpt_filename.c_str(), chkpt, fields.cc, fields.ee, nx, p.ny))
        {
            // 恢复状态
            ct = chkpt.sim_time;
            dt = chkpt.dt;
            count = chkpt.output_count;
            dt_adjustments = chkpt.dt_adjustments;
            eta_ave = chkpt.eta_ave;
            old_eta = chkpt.old_eta;
            p.coeff_dt = chkpt.coeff_dt;
            start_iteration = chkpt.iteration + 1;
            run_log.resumed_from_checkpoint = true;

            // 重新计算 max_it 和 ns（因为 dt 可能已变化）
            max_it = static_cast<long>(p.endT / dt + sim::EPS_EPOCH);
            ns = static_cast<long>(max_it / p.total_count + sim::EPS_EPOCH);

            // 记录到日志
            run_log.resumed_at_iteration = chkpt.iteration;

#pragma omp critical
            {
                std::printf("[Case %d] Resumed from checkpoint at iteration %ld, time=%.4f\n",
                            case_number, chkpt.iteration, ct);
            }
        }
        else
        {
#pragma omp critical
            {
                std::printf("[Case %d] Warning: Failed to load checkpoint, starting fresh\n",
                            case_number);
            }
        }
    }

    // ---------- 时间循环 ----------

    for (long it = start_iteration; it <= max_it; ++it)
    {
        ct += dt;

        full_step_explicit(fields, grid, phys, zone, ct, dt);

        // --- NaN 检测 ---
        bool has_nan = false;
#pragma omp parallel for collapse(2) reduction(|| : has_nan)
        for (long i = 0; i <= nx; ++i)
        {
            for (long j = 0; j <= p.ny; ++j)
            {
                if (std::isnan(fields.nc[i][j]))
                    has_nan = true;
            }
        }
        if (has_nan)
        {
            if (++dt_adjustments > sim::MAX_ADJUST)
                break;

            // 记录 dt 调整事件
            double old_dt = dt;

            p.coeff_dt *= sim::DT_SHRINK;
            dt = p.coeff_dt * grid.h * grid.h;

            // 记录到日志
            run_log.dt_history.push_back({it, old_dt, dt, ct});
            run_log.nan_events++;

            max_it = static_cast<long>(p.endT / dt + sim::EPS_EPOCH);
            ns = static_cast<long>(max_it / p.total_count + sim::EPS_EPOCH);
            zero_matrix(fields.cc, -1, grid.nx + 1, -1, grid.ny + 1);
            zero_matrix(fields.nc, -1, grid.nx + 1, -1, grid.ny + 1);
            zero_matrix(fields.adv_c, -1, grid.nx + 1, -1, grid.ny + 1);
            zero_vector(fields.ee, -1, grid.nx + 1);
            zero_vector(fields.ne, -1, grid.nx + 1);
            initialization(fields.cc, fields.xx, fields.yy, p.x_ini_posi, grid.nx, grid.ny, grid.h,
                           grid.xleft, grid.yleft);
            ct = 0.0;
            count = 1;
            it = 0;
            continue;
        }

        // --- 定期统计区域平均浓度 ---
        if (it % config::STATS_INTERVAL == 0)
        {
            old_eta = eta_ave;
            eta_ave = 0.0;
            int k = 0;
            for (long i = 0; i <= nx; ++i)
            {
                double x = i * h;
                if (x > xl && x < xr)
                {
                    eta_ave += fields.ne[i];
                    ++k;
                }
            }
            if (k)
                eta_ave /= k;
            eta_ave_dt = (eta_ave - old_eta) / dt / static_cast<double>(config::STATS_INTERVAL);
            std::FILE *fp = std::fopen(fname_eta, "a");
            std::fprintf(fp, " %f  %g %g\n", ct, eta_ave, eta_ave_dt);
            std::fclose(fp);

            // --- 实时进度条 ---
            double eta_eq = p.K0 / (1.0 + p.K0);
            double progress = (eta_eq > 1e-10) ? (eta_ave / eta_eq * 100.0) : 0.0;
            if (progress > 100.0)
                progress = 100.0;  // 防止超过100%
            double rel_err = std::fabs(eta_ave - eta_eq) / eta_eq;
            // 记录收敛历史的关键节点（每 10% 进度或误差降低一个数量级）
            bool should_record = false;
            if (run_log.convergence_history.empty())
            {
                should_record = true;  // 记录第一个点
            }
            else
            {
                const auto &last = run_log.convergence_history.back();
                double last_progress = (eta_eq > 1e-10) ? (last.eta_ave / eta_eq * 100.0) : 0.0;
                // 进度增加超过 10% 或误差降低超过 2 倍
                if (progress - last_progress >= 10.0 || last.rel_err / rel_err >= 2.0)
                {
                    should_record = true;
                }
            }
            if (should_record)
            {
                run_log.convergence_history.push_back({it, ct, eta_ave, rel_err});
            }

            double elapsed_now = omp_get_wtime() - start;

            // 构建进度条字符串
            int bar_width = config::PROGRESS_BAR_WIDTH;
            int filled = static_cast<int>(progress / 100.0 * bar_width);

#pragma omp critical
            {
                std::printf("\r[Case %2d] [", case_number);
                for (int b = 0; b < bar_width; ++b)
                {
                    if (b < filled)
                        std::printf("=");
                    else if (b == filled)
                        std::printf(">");
                    else
                        std::printf(" ");
                }
                std::printf("] %5.1f%% | err=%.1e | %.0fs ", progress, rel_err, elapsed_now);
                std::fflush(stdout);
            }

            // --- eta_eq 收敛判断 ---
            if (rel_err < config::CONVERGENCE_THRESHOLD)
            {
                // 更新日志的最终状态
                run_log.converged = true;
                run_log.final_eta = eta_ave;
                run_log.final_rel_err = rel_err;
                run_log.final_sim_time = ct;
                run_log.actual_iterations = it;
                run_log.output_count = count;

                // 收敛时打印换行，结束进度条
                double elapsed_now = omp_get_wtime() - start;
#pragma omp critical
                {
                    std::printf("\n[Case %d] Converged! eta=%.6f, time=%.4f, elapsed=%.1fs\n",
                                case_number, eta_ave, ct, elapsed_now);
                }

                print_data(fields.cc, fields.ee, fields.xx, count + 1000, fname_data, nx, p.ny);

                std::FILE *fp_log = std::fopen(fname_log, "a");
                std::fprintf(fp_log, "final_time_%d = %f;\n", count + 1000, ct);
                std::fprintf(fp_log, "converged_eta_eq = %g;\n", eta_eq);
                std::fprintf(fp_log, "converged_eta_ave = %g;\n", eta_ave);
                std::fclose(fp_log);

                break;
            }
        }

        // --- 数据输出 ---
        if (it % ns == 0)
        {
            double io_start = omp_get_wtime();
            print_data(fields.cc, fields.ee, fields.xx, count, fname_data, nx, p.ny);
            run_log.time_io += omp_get_wtime() - io_start;

            std::FILE *fp = std::fopen(fname_log, "a");
            std::fprintf(fp, "time_%d = %f;\n", count, ct);
            std::fclose(fp);
            ++count;
        }
        // --- 定期保存检查点 ---
        if (it % config::CHECKPOINT_INTERVAL == 0)
        {
            Checkpoint chkpt;
            chkpt.iteration = it;
            chkpt.sim_time = ct;
            chkpt.dt = dt;
            chkpt.output_count = count;
            chkpt.dt_adjustments = dt_adjustments;
            chkpt.eta_ave = eta_ave;
            chkpt.old_eta = old_eta;
            chkpt.case_number = case_number;
            chkpt.coeff_dt = p.coeff_dt;

            if (save_checkpoint(chkpt_filename.c_str(), chkpt, fields.cc, fields.ee, nx, p.ny))
            {
                // 可选：输出保存成功信息（通常静默保存）
                // std::printf("[Case %d] Checkpoint saved at iteration %ld\n", case_number, it);
            }
        }
    }

    // 如果未收敛，记录最终状态
    if (!run_log.converged)
    {
        run_log.final_eta = eta_ave;
        double eta_eq = p.K0 / (1.0 + p.K0);
        run_log.final_rel_err = std::fabs(eta_ave - eta_eq) / eta_eq;
        run_log.final_sim_time = ct;
        run_log.actual_iterations = max_it;
        run_log.output_count = count - 1;
    }

    // 记录计算耗时
    run_log.time_compute = omp_get_wtime() - time_compute_start - run_log.time_io;

// 如果是正常结束（未收敛），打印换行结束进度条
#pragma omp critical
    {
        std::printf("\n");
    }

    // ---------- CPU 时间统计 ----------
    double end = omp_get_wtime();
    run_log.time_total = end - start;

    // 写入详细日志
    write_detailed_log(fname_log, case_number, p, grid, phys, zone, run_log, dt_initial);

#pragma omp critical
    {
        std::printf("[Case %d] Completed in %.1fs (converged=%s)\n", case_number,
                    run_log.time_total, run_log.converged ? "yes" : "no");
    }

    // ---------- 释放内存 ----------
    free_dmatrix(cc, -1, nx + 1, -1, p.ny + 1);
    free_dmatrix(nc, -1, nx + 1, -1, p.ny + 1);
    free_dmatrix(adv_c, -1, nx + 1, -1, p.ny + 1);
    free_dvector(ee, -1, nx + 1);
    free_dvector(ne, -1, nx + 1);
    free_dvector(xx, -1, nx + 1);
    free_dvector(yy, -1, p.ny + 1);
    free_dvector(ff, -1, p.ny + 1);
}

//========== main =======================================================
int main()
{
    // ---------- 设置线程数 ----------
    if (config::NUM_THREADS > 0)
    {
        omp_set_num_threads(config::NUM_THREADS);
    }
    else
    {
        omp_set_num_threads(std::max(1, omp_get_max_threads()));
    }

    std::cout << "Using " << omp_get_max_threads() << " thread(s)." << std::endl;

    // ---------- 获取 case 列表 ----------
    std::vector<int> case_numbers;

    if (config::USE_CASE_LIST)
    {
        // 使用用户指定的 case 列表
        case_numbers = config::get_case_list();
        std::cout << "Running user-specified cases: ";
        for (size_t i = 0; i < case_numbers.size(); ++i)
        {
            std::cout << case_numbers[i];
            if (i < case_numbers.size() - 1)
                std::cout << ", ";
        }
        std::cout << std::endl;
    }
    else
    {
        // 自动扫描目录下所有 input_parameter_X.txt 文件
        std::regex pattern(R"(^input_parameter_(\d+)\.txt$)");
        std::smatch matches;

        for (const auto &p : fs::directory_iterator(fs::current_path()))
        {
            if (!p.is_regular_file())
                continue;
            const std::string fname = p.path().filename().string();
            if (std::regex_match(fname, matches, pattern))
                case_numbers.push_back(std::stoi(matches[1]));
        }

        if (case_numbers.empty())
        {
            std::cerr << "No input_parameter_X.txt found.\n";
            return EXIT_FAILURE;
        }

        std::sort(case_numbers.begin(), case_numbers.end());
        std::cout << "Found " << case_numbers.size() << " case(s) in directory." << std::endl;
    }

    // ---------- 显示配置信息 ----------
    if (config::FORCE_RESTART_ALL)
    {
        std::cout << "Force restart mode: ALL cases will start from beginning." << std::endl;
    }

// ---------- 并行执行 ----------
#pragma omp parallel for schedule(dynamic)
    for (std::size_t idx = 0; idx < case_numbers.size(); ++idx)
    {
        int case_no = case_numbers[idx];
        Params p;
        try
        {
            p = read_parameter(case_no);
        }
        catch (const std::exception &e)
        {
#pragma omp critical
            std::cerr << "[Case " << case_no << "] " << e.what() << std::endl;
            continue;
        }

#pragma omp critical
        std::cout << "[Thread " << omp_get_thread_num() << "] Running case " << case_no << " ("
                  << idx + 1 << "/" << case_numbers.size() << ")" << std::endl;

        case_calculation(case_no, p);
    }

    return EXIT_SUCCESS;
}

void full_step_explicit(SimFields &fields, const GridInfo &grid, const PhysicsParams &phys,
                        const AdsorptionZone &zone, double ct, double dt)
{
    calc_eta(fields.ee, fields.ne, fields.cc, dt, grid.nx, grid.h, zone.xpo_l, zone.xpo_r, phys.eps,
             phys.Da, phys.K0);

    augment_phi(fields.cc, fields.ee, grid.nx, grid.ny, grid.h, phys.Da, phys.K0, zone.xpo_l,
                zone.xpo_r, phys.c0);

    oscillatory(phys.alpha, phys.Sc, fields.ff, fields.yy, ct, grid.ny);

    advection_c(fields.cc, fields.adv_c, fields.yy, fields.ff, grid.nx, grid.ny, grid.h, phys.Pe,
                phys.Pe2);

    calc_phi(fields.cc, fields.nc, fields.adv_c, dt, grid.nx, grid.ny, grid.h);

    vec_copy(fields.ee, fields.ne, 0, grid.nx);
    mat_copy(fields.cc, fields.nc, 0, grid.nx, 0, grid.ny);
}

void augment_eta(double *ee, long nx)
{
    ee[-1] = ee[0];
    ee[nx + 1] = ee[nx];
}

void oscillatory(double alpha, double Sc, double *ff, const double *yy, double ct, long ny)
{
    // 与 j 无关的量，提到循环外
    const double ca = cos(alpha), sa = sin(alpha);
    const double ch = cosh(alpha), sh = sinh(alpha);

    const double c2 = cos(2.0 * alpha);
    const double ch2 = cosh(2.0 * alpha);

    const double a2 = alpha * alpha;
    const double dcc = 1.0 / (c2 + ch2);
    const double da2 = 1.0 / a2;

    // sat, cat 与 j 无关，只依赖时间 ct
    const double sat = sin(2.0 * a2 * Sc * ct);
    const double cat = cos(2.0 * a2 * Sc * ct);

    // 预计算常用组合
    const double c2_ch2_sat = (c2 + ch2) * sat;
    const double two_sa_sh = 2.0 * sa * sh;
    const double two_ca_ch = 2.0 * ca * ch;
    const double coeff = dcc * da2;

#pragma omp parallel for schedule(static)
    for (long j = 0; j <= ny; ++j)
    {
        const double arg = 2.0 * alpha * (yy[j] - 0.5);
        const double say = sin(arg);
        const double cay = cos(arg);
        const double shay = sinh(arg);
        const double chay = cosh(arg);

        ff[j] = coeff * (c2_ch2_sat + two_sa_sh * cay * cat * chay - two_sa_sh * say * sat * shay -
                         two_ca_ch * (cay * chay * sat + cat * say * shay));
    }
}

void advection_c(double **oc, double **adv_c, const double *yy, const double *ff, long nx, long ny,
                 double h, double Pe, double Pe2)
{
    const double h_inv = 1.0 / h;  // 提取到循环外

#pragma omp parallel for collapse(2) schedule(static)
    for (long i = 0; i <= nx; ++i)
    {
        for (long j = 0; j <= ny; ++j)
        {
            double du = Pe * yy[j] * (1.0 - yy[j]) + Pe2 * ff[j];
            adv_c[i][j] = (du > 0.0) ? du * (oc[i][j] - oc[i - 1][j]) * h_inv
                                     : du * (oc[i + 1][j] - oc[i][j]) * h_inv;
        }
    }
}

void augment_phi(double **cc, const double *ee, long nx, long ny, double h, double Da, double K0,
                 double xpo_l, double xpo_r, double c0)
{
    // 第一个循环：设置左右边界（循环较短，不并行）
    for (long j = -1; j <= ny + 1; ++j)
    {
        cc[-1][j] = cc[0][j] = c0;
        cc[nx + 1][j] = cc[nx][j];
    }

    // 第二个循环：设置上下边界（可并行）
    const double K0_inv = 1.0 / K0;
    const double h_Da = h * Da;

#pragma omp parallel for schedule(static)
    for (long i = 0; i <= nx; ++i)
    {
        cc[i][ny + 1] = cc[i][ny];

        double x = static_cast<double>(i) * h;
        if (x > xpo_l && x <= xpo_r)
        {
            cc[i][-1] = cc[i][0] - h_Da * (cc[i][0] * (1.0 - ee[i]) - ee[i] * K0_inv);
        }
        else
        {
            cc[i][-1] = cc[i][0];
        }
    }
}

void calc_eta(const double *ee, double *ne, double **cc, double dt, long nx, double h, double xpo_l,
              double xpo_r, double eps, double Da, double K0)
{
    const double K0_inv = 1.0 / K0;  // 提取到循环外
    const double coeff = eps * Da;

#pragma omp parallel for schedule(static)
    for (long i = 0; i <= nx; ++i)
    {
        double x = static_cast<double>(i) * h;
        if (x > xpo_l && x <= xpo_r)
        {
            ne[i] = ee[i] + dt * coeff * (cc[i][0] * (1.0 - ee[i]) - ee[i] * K0_inv);
        }
        else
        {
            ne[i] = 0.0;
        }
    }
}

void calc_phi(double **cc, double **nc, double **adv_c, double dt, long nx, long ny, double h)
{
    const double h2_inv = 1.0 / (h * h);  // 提取到循环外避免重复计算

#pragma omp parallel for collapse(2) schedule(static)
    for (long i = 0; i <= nx; ++i)
    {
        for (long j = 0; j <= ny; ++j)
        {
            double lap = ((cc[i + 1][j] - 2.0 * cc[i][j] + cc[i - 1][j]) +
                          (cc[i][j + 1] - 2.0 * cc[i][j] + cc[i][j - 1])) *
                         h2_inv;
            double sr = -adv_c[i][j];
            nc[i][j] = cc[i][j] + dt * (lap + sr);
        }
    }
}

void relax(double **p, double **oc, double **f, double dt, long nx, long ny, int p_relax,
           double lam, double h)
{
    double lam2 = lam * lam, h2 = h * h;

    for (int iter = 1; iter <= p_relax; ++iter)
        for (long i = 0; i <= nx; ++i)
            for (long j = 0; j <= ny; ++j)
            {
                double coef = 1.0 / dt + 2.0 * (1.0 + lam2) / h2;
                double lap =
                    (lam2 * (p[i + 1][j] + p[i - 1][j]) + (p[i][j + 1] + p[i][j - 1])) / h2;
                double src = oc[i][j] / dt - f[i][j];
                p[i][j] = (lap + src) / coef;
            }
}

void print_data(double **phi, const double *eta, const double *xx, int count, const char *buf,
                long nx, long ny)
{
    // 保证目标子目录存在
    if (!fs::exists(buf))
        fs::create_directories(buf);

    char buffer_phi[64], buffer_eta[64];
    std::snprintf(buffer_phi, sizeof(buffer_phi), "./%s/cc_%d.m", buf, count);
    std::snprintf(buffer_eta, sizeof(buffer_eta), "./%s/ee_%d.m", buf, count);

    FILE *fphi = fopen(buffer_phi, "w");
    for (long i = 0; i <= nx; ++i)
    {
        for (long j = 0; j <= ny; ++j)
            fprintf(fphi, " %16.14f ", phi[i][j]);
        fprintf(fphi, "\n");
    }
    fclose(fphi);

    FILE *feta = fopen(buffer_eta, "w");
    for (long i = 0; i <= nx; ++i)
        fprintf(feta, " %16.14f  %16.14f\n", xx[i], eta[i]);
    fclose(feta);
}

void print_tecplot_data(double **cc, int count, const char *buf, long nx, long ny, double h,
                        double xleft, double yleft)
{
    // 保证目标子目录存在
    if (!fs::exists(buf))
        fs::create_directories(buf);

    char mid[64];
    std::snprintf(mid, sizeof(mid), "./%s/cc_%d.dat", buf, count);
    FILE *out = fopen(mid, "w");
    if (!out)
    {
        perror("open tecplot");
        return;
    }

    fprintf(out, " \"IJK - Ordered Data\"\n");
    fprintf(out, "VARIABLES = \"x\",\"y\",\"cc\"\n");
    fprintf(out, "ZONE T = \"immerseBoundary\"\n");
    fprintf(out, "STRANDID = 0, SOLUTIONTIME = 0\n");
    fprintf(out, "I = %ld, J = %ld, K = 1, ZONETYPE = Ordered\n", nx + 1, ny + 1);
    fprintf(out, "DATAPACKING = BLOCK\n");
    fprintf(out, "DT = ( SINGLE SINGLE SINGLE )\n");

    // x
    for (long j = 0; j <= ny; ++j)
    {
        for (long i = 0; i <= nx; ++i)
            fprintf(out, " %8.5e ", xleft + i * h);
        fprintf(out, "\n");
    }
    // y
    for (long j = 0; j <= ny; ++j)
    {
        for (long i = 0; i <= nx; ++i)
            fprintf(out, " %8.5e ", yleft + j * h);
        fprintf(out, "\n");
    }
    // cc
    for (long j = 0; j <= ny; ++j)
    {
        for (long i = 0; i <= nx; ++i)
            fprintf(out, " %8.5e ", cc[i][j]);
        fprintf(out, "\n");
    }
    fclose(out);
}

void ensure_dir(const std::string &dir)
{
    if (!fs::exists(dir))
        fs::create_directories(dir);  // 等效 mkdir -p
}

// ---------- 详细日志输出 --------------------------------------------------
void write_detailed_log(const char *fname_log, int case_number, const Params &p,
                        const GridInfo &grid, const PhysicsParams &phys, const AdsorptionZone &zone,
                        const RunLog &log, double dt_initial)
{
    std::FILE *fp = std::fopen(fname_log, "w");
    if (!fp)
    {
        std::perror("Cannot open log file");
        return;
    }

    // ==================== 标题和时间戳 ====================
    std::time_t now = std::time(nullptr);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::fprintf(fp, "%%=============================================================\n");
    std::fprintf(fp, "%%  DIFFUSION-CONVECTION SIMULATION LOG\n");
    std::fprintf(fp, "%%  Case Number: %d\n", case_number);
    std::fprintf(fp, "%%  Generated:   %s\n", time_buf);
    std::fprintf(fp, "%%=============================================================\n\n");

    // ==================== 输入参数 ====================
    std::fprintf(fp, "%% -------------------- INPUT PARAMETERS --------------------\n");
    std::fprintf(fp, "case_number = %d;\n\n", case_number);

    std::fprintf(fp, "%% Geometry and mesh\n");
    std::fprintf(fp, "lam = %.6g;        %% aspect ratio (H/L)\n", p.lam);
    std::fprintf(fp, "ny  = %ld;         %% grid points in y-direction\n", p.ny);
    std::fprintf(fp, "nx  = %ld;         %% grid points in x-direction (computed)\n", grid.nx);
    std::fprintf(fp, "h   = %.6e;   %% grid spacing\n", grid.h);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "%% Computational domain\n");
    std::fprintf(fp, "xleft  = %.6g;\n", grid.xleft);
    std::fprintf(fp, "xright = %.6g;\n", grid.xright);
    std::fprintf(fp, "yleft  = %.6g;\n", grid.yleft);
    std::fprintf(fp, "yright = %.6g;\n", grid.yright);
    std::fprintf(fp, "domain_length = %.6g;  %% L = xright - xleft\n", grid.xright - grid.xleft);
    std::fprintf(fp, "domain_height = %.6g;  %% H = yright - yleft\n", grid.yright - grid.yleft);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "%% Flow parameters\n");
    std::fprintf(fp, "Pe    = %.6g;      %% Peclet number (steady flow)\n", phys.Pe);
    std::fprintf(fp, "Pe2   = %.6g;      %% Peclet number (oscillatory flow)\n", phys.Pe2);
    std::fprintf(fp, "alpha = %.6g;      %% Womersley number\n", phys.alpha);
    std::fprintf(fp, "Sc    = %.6g;      %% Schmidt number\n", phys.Sc);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "%% Reaction parameters\n");
    std::fprintf(fp, "Da  = %.6g;        %% Damkohler number\n", phys.Da);
    std::fprintf(fp, "K0  = %.6g;        %% equilibrium constant\n", phys.K0);
    std::fprintf(fp, "eps = %.6g;        %% surface capacity parameter\n", phys.eps);
    std::fprintf(fp, "c0  = %.6g;        %% inlet concentration\n", phys.c0);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "%% Adsorption zone (dimensionless, then physical)\n");
    std::fprintf(fp, "xpo_l_rel = %.6g;  %% relative position (0-1)\n", p.xpo_l);
    std::fprintf(fp, "xpo_r_rel = %.6g;  %% relative position (0-1)\n", p.xpo_r);
    std::fprintf(fp, "xpo_l = %.6g;      %% physical left boundary\n", zone.xpo_l);
    std::fprintf(fp, "xpo_r = %.6g;      %% physical right boundary\n", zone.xpo_r);
    std::fprintf(fp, "adsorption_length = %.6g;  %% length of adsorption zone\n",
                 zone.xpo_r - zone.xpo_l);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "%% Time integration\n");
    std::fprintf(fp, "endT        = %.6g;    %% target end time\n", p.endT);
    std::fprintf(fp, "coeff_dt    = %.6g;    %% dt coefficient (dt = coeff_dt * h^2)\n",
                 p.coeff_dt);
    std::fprintf(fp, "dt_initial  = %.6e;    %% initial time step\n", dt_initial);
    std::fprintf(fp, "total_count = %ld;      %% planned output count\n", p.total_count);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "%% Initial condition\n");
    std::fprintf(fp, "x_ini_posi = %.6g;  %% initial concentration front position\n", p.x_ini_posi);
    std::fprintf(fp, "\n");

    // ==================== 理论计算值 ====================
    std::fprintf(fp, "%% -------------------- THEORETICAL VALUES --------------------\n");
    double eta_eq = phys.K0 / (1.0 + phys.K0);
    double u_max = std::abs(phys.Pe) * 0.25 + std::abs(phys.Pe2);
    double dt_diff = 0.25 * grid.h * grid.h;
    double dt_conv = (u_max > 1e-10) ? (grid.h / u_max) : 1e10;

    std::fprintf(fp, "eta_eq = %.10g;     %% equilibrium coverage = K0/(1+K0)\n", eta_eq);
    std::fprintf(fp, "u_max_estimate = %.6g;  %% estimated maximum velocity\n", u_max);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "%% CFL stability estimates\n");
    std::fprintf(fp, "dt_diffusion_limit  = %.6e;  %% h^2/4 (2D explicit)\n", dt_diff);
    std::fprintf(fp, "dt_convection_limit = %.6e;  %% h/u_max\n", dt_conv);
    std::fprintf(fp, "dt_cfl_recommended  = %.6e;  %% 0.4 * min(dt_diff, dt_conv)\n",
                 0.4 * std::min(dt_diff, dt_conv));
    std::fprintf(fp, "\n");

    // ==================== 内存估算 ====================
    std::fprintf(fp, "%% -------------------- MEMORY ESTIMATE --------------------\n");
    long total_cells = (grid.nx + 3) * (grid.ny + 3);                      // 包含 ghost cells
    double mem_matrix = total_cells * sizeof(double) / (1024.0 * 1024.0);  // MB
    double mem_vector = (grid.nx + 3) * sizeof(double) / (1024.0 * 1024.0);
    double mem_total = 3 * mem_matrix + 5 * mem_vector;  // cc, nc, adv_c + ee, ne, xx, yy, ff

    std::fprintf(fp, "total_grid_cells = %ld;  %% including ghost cells\n", total_cells);
    std::fprintf(fp, "memory_per_matrix_MB = %.3f;\n", mem_matrix);
    std::fprintf(fp, "memory_per_vector_MB = %.6f;\n", mem_vector);
    std::fprintf(fp, "memory_total_estimate_MB = %.3f;  %% 3 matrices + 5 vectors\n", mem_total);
    std::fprintf(fp, "\n");

    // ==================== 运行时事件 ====================
    std::fprintf(fp, "%% -------------------- RUNTIME EVENTS --------------------\n");
    std::fprintf(fp, "nan_events = %d;  %% number of NaN occurrences\n", log.nan_events);
    std::fprintf(fp, "resumed_from_checkpoint = %d;  %% 1=yes, 0=no\n",
                 log.resumed_from_checkpoint ? 1 : 0);
    if (log.resumed_from_checkpoint)
    {
        std::fprintf(fp, "resumed_at_iteration = %ld;\n", log.resumed_at_iteration);
    }
    std::fprintf(fp, "\n");

    if (!log.dt_history.empty())
    {
        std::fprintf(fp, "%% dt adjustment history (iteration, old_dt, new_dt, sim_time)\n");
        std::fprintf(fp, "dt_adjustments = [\n");
        for (const auto &adj : log.dt_history)
        {
            std::fprintf(fp, "    %8ld, %.6e, %.6e, %.6e;  %% iter, old, new, time\n",
                         adj.iteration, adj.old_dt, adj.new_dt, adj.sim_time);
        }
        std::fprintf(fp, "];\n\n");
    }
    else
    {
        std::fprintf(fp, "%% No dt adjustments were needed (stable throughout)\n\n");
    }

    // ==================== 收敛历史 ====================
    std::fprintf(fp, "%% -------------------- CONVERGENCE HISTORY --------------------\n");
    if (!log.convergence_history.empty())
    {
        std::fprintf(fp, "%% Key convergence milestones (iteration, time, eta, rel_error)\n");
        std::fprintf(fp, "convergence_milestones = [\n");
        for (const auto &pt : log.convergence_history)
        {
            std::fprintf(fp, "    %8ld, %.6e, %.10g, %.6e;\n", pt.iteration, pt.sim_time,
                         pt.eta_ave, pt.rel_err);
        }
        std::fprintf(fp, "];\n");
        std::fprintf(fp, "convergence_milestones_headers = {'iteration', 'sim_time', 'eta_ave', "
                         "'rel_error'};\n\n");
    }

    // ==================== 性能统计 ====================
    std::fprintf(fp, "%% -------------------- PERFORMANCE STATISTICS --------------------\n");
    std::fprintf(fp, "actual_iterations = %ld;\n", log.actual_iterations);
    std::fprintf(fp, "output_file_count = %d;\n", log.output_count);
    std::fprintf(fp, "\n");

    std::fprintf(fp, "%% Timing breakdown (seconds)\n");
    std::fprintf(fp, "time_initialization = %.3f;\n", log.time_init);
    std::fprintf(fp, "time_computation    = %.3f;\n", log.time_compute);
    std::fprintf(fp, "time_io             = %.3f;\n", log.time_io);
    std::fprintf(fp, "time_total          = %.3f;\n", log.time_total);
    std::fprintf(fp, "\n");

    if (log.actual_iterations > 0)
    {
        double time_per_iter = log.time_compute / log.actual_iterations * 1000.0;  // ms
        double iters_per_sec = log.actual_iterations / log.time_compute;
        std::fprintf(fp, "time_per_iteration_ms = %.4f;\n", time_per_iter);
        std::fprintf(fp, "iterations_per_second = %.1f;\n", iters_per_sec);
        std::fprintf(fp, "\n");
    }

    // ==================== 最终结果 ====================
    std::fprintf(fp, "%% -------------------- FINAL RESULTS --------------------\n");
    std::fprintf(fp, "converged = %d;  %% 1=yes, 0=no (reached max_it)\n", log.converged ? 1 : 0);
    std::fprintf(fp, "final_sim_time = %.10g;\n", log.final_sim_time);
    std::fprintf(fp, "final_eta_ave  = %.10g;\n", log.final_eta);
    std::fprintf(fp, "final_rel_error = %.6e;\n", log.final_rel_err);
    std::fprintf(fp, "\n");

    if (log.converged)
    {
        std::fprintf(fp, "%% Convergence achieved!\n");
        std::fprintf(fp, "%% eta_ave reached within 0.1%% of eta_eq = %.10g\n", eta_eq);
    }
    else
    {
        std::fprintf(fp, "%% WARNING: Simulation ended without convergence\n");
        std::fprintf(fp, "%% Consider: (1) increase endT, (2) check parameters\n");
    }
    std::fprintf(fp, "\n");

    // ==================== 输出文件列表 ====================
    std::fprintf(fp, "%% -------------------- OUTPUT FILES --------------------\n");
    std::fprintf(fp, "%% Data files are in: data_%d/\n", case_number);
    std::fprintf(fp, "%%   cc_N.m  - concentration field at output N\n");
    std::fprintf(fp, "%%   ee_N.m  - surface coverage (eta) at output N\n");
    std::fprintf(fp, "%% \n");
    std::fprintf(fp, "%% Time series: eta_ave_%d.m\n", case_number);
    std::fprintf(fp, "%%   Format: [time, eta_average, d(eta)/dt]\n");
    std::fprintf(fp, "\n");

    // ==================== 尾部 ====================
    std::fprintf(fp, "%%=============================================================\n");
    std::fprintf(fp, "%%  END OF LOG\n");
    std::fprintf(fp, "%%=============================================================\n");

    std::fclose(fp);
}

// ---------- 检查点文件名生成 --------------------------------------------------
std::string get_checkpoint_filename(int case_number)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "checkpoint_%d.bin", case_number);
    return std::string(buf);
}

// ---------- 检查检查点是否存在 --------------------------------------------------
bool checkpoint_exists(int case_number)
{
    std::string filename = get_checkpoint_filename(case_number);
    return fs::exists(filename);
}

// ---------- 删除检查点文件 --------------------------------------------------
void delete_checkpoint(int case_number)
{
    std::string filename = get_checkpoint_filename(case_number);
    if (fs::exists(filename))
    {
        fs::remove(filename);
    }
}

// ---------- 检查是否强制重新开始 --------------------------------------------------
bool check_force_restart(int case_number)
{
    char filename[64];
    std::snprintf(filename, sizeof(filename), "force_restart_%d.txt", case_number);

    if (fs::exists(filename))
    {
        // 删除标志文件（只触发一次）
        fs::remove(filename);
        return true;
    }
    return false;
}

// ---------- 保存检查点 --------------------------------------------------
bool save_checkpoint(const char *filename, const Checkpoint &chkpt, double **cc, double *ee,
                     long nx, long ny)
{
    std::FILE *fp = std::fopen(filename, "wb");
    if (!fp)
    {
        std::perror("Cannot create checkpoint file");
        return false;
    }

// 辅助宏：检查写入是否成功
#define CHECK_WRITE(call, expected)                                                                \
    if ((call) != (expected))                                                                      \
    {                                                                                              \
        std::fprintf(stderr, "Checkpoint write error at line %d\n", __LINE__);                     \
        std::fclose(fp);                                                                           \
        return false;                                                                              \
    }

    // 写入魔数和版本
    uint32_t magic = Checkpoint::MAGIC;
    uint32_t version = Checkpoint::VERSION;
    CHECK_WRITE(std::fwrite(&magic, sizeof(magic), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&version, sizeof(version), 1, fp), 1);

    // 写入网格尺寸（用于验证）
    CHECK_WRITE(std::fwrite(&nx, sizeof(nx), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&ny, sizeof(ny), 1, fp), 1);

    // 写入检查点元数据
    CHECK_WRITE(std::fwrite(&chkpt.iteration, sizeof(chkpt.iteration), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&chkpt.sim_time, sizeof(chkpt.sim_time), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&chkpt.dt, sizeof(chkpt.dt), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&chkpt.output_count, sizeof(chkpt.output_count), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&chkpt.dt_adjustments, sizeof(chkpt.dt_adjustments), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&chkpt.eta_ave, sizeof(chkpt.eta_ave), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&chkpt.old_eta, sizeof(chkpt.old_eta), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&chkpt.case_number, sizeof(chkpt.case_number), 1, fp), 1);
    CHECK_WRITE(std::fwrite(&chkpt.coeff_dt, sizeof(chkpt.coeff_dt), 1, fp), 1);

    // 写入浓度场 cc（只写入有效区域 [0, nx] x [0, ny]）
    for (long i = 0; i <= nx; ++i)
    {
        CHECK_WRITE(std::fwrite(&cc[i][0], sizeof(double), ny + 1, fp),
                    static_cast<size_t>(ny + 1));
    }

    // 写入表面覆盖率 ee（只写入有效区域 [0, nx]）
    CHECK_WRITE(std::fwrite(&ee[0], sizeof(double), nx + 1, fp), static_cast<size_t>(nx + 1));

#undef CHECK_WRITE

    std::fclose(fp);
    return true;
}

// ---------- 加载检查点 --------------------------------------------------
bool load_checkpoint(const char *filename, Checkpoint &chkpt, double **cc, double *ee, long nx,
                     long ny)
{
    std::FILE *fp = std::fopen(filename, "rb");
    if (!fp)
    {
        std::perror("Cannot open checkpoint file");
        return false;
    }

// 辅助宏：检查读取是否成功
#define CHECK_READ(call, expected)                                                                 \
    if ((call) != (expected))                                                                      \
    {                                                                                              \
        std::fprintf(stderr, "Checkpoint read error at line %d\n", __LINE__);                      \
        std::fclose(fp);                                                                           \
        return false;                                                                              \
    }

    // 读取并验证魔数和版本
    uint32_t magic, version;
    CHECK_READ(std::fread(&magic, sizeof(magic), 1, fp), 1);
    CHECK_READ(std::fread(&version, sizeof(version), 1, fp), 1);

    if (magic != Checkpoint::MAGIC)
    {
        std::fprintf(stderr, "Invalid checkpoint file (bad magic number)\n");
        std::fclose(fp);
        return false;
    }
    if (version != Checkpoint::VERSION)
    {
        std::fprintf(stderr, "Incompatible checkpoint version (expected %u, got %u)\n",
                     Checkpoint::VERSION, version);
        std::fclose(fp);
        return false;
    }

    // 读取并验证网格尺寸
    long file_nx, file_ny;
    CHECK_READ(std::fread(&file_nx, sizeof(file_nx), 1, fp), 1);
    CHECK_READ(std::fread(&file_ny, sizeof(file_ny), 1, fp), 1);

    if (file_nx != nx || file_ny != ny)
    {
        std::fprintf(stderr, "Grid size mismatch: file has (%ld, %ld), expected (%ld, %ld)\n",
                     file_nx, file_ny, nx, ny);
        std::fclose(fp);
        return false;
    }

    // 读取检查点元数据
    CHECK_READ(std::fread(&chkpt.iteration, sizeof(chkpt.iteration), 1, fp), 1);
    CHECK_READ(std::fread(&chkpt.sim_time, sizeof(chkpt.sim_time), 1, fp), 1);
    CHECK_READ(std::fread(&chkpt.dt, sizeof(chkpt.dt), 1, fp), 1);
    CHECK_READ(std::fread(&chkpt.output_count, sizeof(chkpt.output_count), 1, fp), 1);
    CHECK_READ(std::fread(&chkpt.dt_adjustments, sizeof(chkpt.dt_adjustments), 1, fp), 1);
    CHECK_READ(std::fread(&chkpt.eta_ave, sizeof(chkpt.eta_ave), 1, fp), 1);
    CHECK_READ(std::fread(&chkpt.old_eta, sizeof(chkpt.old_eta), 1, fp), 1);
    CHECK_READ(std::fread(&chkpt.case_number, sizeof(chkpt.case_number), 1, fp), 1);
    CHECK_READ(std::fread(&chkpt.coeff_dt, sizeof(chkpt.coeff_dt), 1, fp), 1);

    // 读取浓度场 cc
    for (long i = 0; i <= nx; ++i)
    {
        CHECK_READ(std::fread(&cc[i][0], sizeof(double), ny + 1, fp), static_cast<size_t>(ny + 1));
    }

    // 读取表面覆盖率 ee
    CHECK_READ(std::fread(&ee[0], sizeof(double), nx + 1, fp), static_cast<size_t>(nx + 1));

#undef CHECK_READ

    std::fclose(fp);
    return true;
}