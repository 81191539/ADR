/*****************************************************************************
 * io.cpp
 * 
 * 数据输入输出功能实现
 *****************************************************************************/

#include "io.h"
#include "config.h"
#include "file_utils.h"

#include <iostream>
#include <fstream>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <stdexcept>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

//-----------------------------------------------------------------------------
// 获取带输出目录前缀的路径
//-----------------------------------------------------------------------------
static std::string get_output_path(const std::string& filename)
{
    if (config::OUTPUT_DIR.empty()) {
        return filename;
    }
    if (!fs::exists(config::OUTPUT_DIR)) {
        fs::create_directories(config::OUTPUT_DIR);
    }
    return config::OUTPUT_DIR + "/" + filename;
}

//-----------------------------------------------------------------------------
// 读取输入参数
//-----------------------------------------------------------------------------
Params read_parameter(int case_number)
{
    Params p;
    int unused{};

    // 尝试多种文件名格式
    std::vector<std::string> patterns = {
        "input_parameter_%d.txt",      // 1
        "input_parameter_%03d.txt",    // 001
        "input_parameter_%02d.txt",     // 01
        "input_parameter_%04d.txt",     // 0001
        "input_parameter_%05d.txt"     // 00001
    };
    
    std::string filepath;
    std::ifstream in;
    
    for (const auto& pat : patterns) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), pat.c_str(), case_number);
        
        if (config::INPUT_DIR.empty()) {
            filepath = buf;
        } else {
            filepath = config::INPUT_DIR + "/" + buf;
        }
        
        in.open(filepath);
        if (in) break;  // 找到了
        in.clear();     // 清除错误状态
    }
    
    if (!in) {
        throw std::runtime_error("Cannot open input_parameter file for case " + std::to_string(case_number));
    }

    in >> unused
       >> p.lam >> p.Pe >> p.Pe2 >> p.eps >> p.Da >> p.K0 >> p.ny
       >> p.xpo_l >> p.xpo_r >> p.endT >> p.total_count >> p.coeff_dt
       >> p.x_ini_posi >> p.alpha;
    
    return p;
}

//-----------------------------------------------------------------------------
// 确保目录存在
//-----------------------------------------------------------------------------
void ensure_dir(const std::string& dir)
{
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
}

//-----------------------------------------------------------------------------
// 输出浓度场和表面覆盖率数据（MATLAB 格式）
//-----------------------------------------------------------------------------
void print_data(const Field2D& phi, const Field1D& eta, const Field1D& xx,
                int count, const char* buf,
                long nx, long ny)
{
    std::string full_dir = get_output_path(buf);
    
    if (!fs::exists(full_dir)) {
        fs::create_directories(full_dir);
    }

    char buffer_phi[128], buffer_eta[128];
    std::snprintf(buffer_phi, sizeof(buffer_phi), "%s/cc_%d.m", full_dir.c_str(), count);
    std::snprintf(buffer_eta, sizeof(buffer_eta), "%s/ee_%d.m", full_dir.c_str(), count);

    try {
        SafeFile fphi(buffer_phi, "w");
        for (long i = 0; i <= nx; ++i) {
            for (long j = 0; j <= ny; ++j) {
                fphi.printf("%s%16.14f ", " ", phi(i, j));
            }
            fphi.puts("\n");
        }

        SafeFile feta(buffer_eta, "w");
        for (long i = 0; i <= nx; ++i) {
            feta.printf(" %16.14f  %16.14f\n", xx(i), eta(i));
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error in print_data: %s\n", e.what());
    }
}

//-----------------------------------------------------------------------------
// 输出 Tecplot 格式数据
//-----------------------------------------------------------------------------
void print_tecplot_data(const Field2D& cc, int count, const char* buf,
                        long nx, long ny, double h,
                        double xleft, double yleft)
{
    std::string full_dir = get_output_path(buf);
    
    if (!fs::exists(full_dir)) {
        fs::create_directories(full_dir);
    }

    char mid[128];
    std::snprintf(mid, sizeof(mid), "%s/cc_%d.dat", full_dir.c_str(), count);
    
    try {
        SafeFile out(mid, "w");

        out.puts(" \"IJK - Ordered Data\"\n");
        out.puts("VARIABLES = \"x\",\"y\",\"cc\"\n");
        out.puts("ZONE T = \"immerseBoundary\"\n");
        out.puts("STRANDID = 0, SOLUTIONTIME = 0\n");
        out.printf("I = %ld, J = %ld, K = 1, ZONETYPE = Ordered\n", nx + 1, ny + 1);
        out.puts("DATAPACKING = BLOCK\n");
        out.puts("DT = ( SINGLE SINGLE SINGLE )\n");

        for (long j = 0; j <= ny; ++j) {
            for (long i = 0; i <= nx; ++i) {
                out.printf(" %8.5e ", xleft + i * h);
            }
            out.puts("\n");
        }

        for (long j = 0; j <= ny; ++j) {
            for (long i = 0; i <= nx; ++i) {
                out.printf(" %8.5e ", yleft + j * h);
            }
            out.puts("\n");
        }

        for (long j = 0; j <= ny; ++j) {
            for (long i = 0; i <= nx; ++i) {
                out.printf(" %8.5e ", cc(i, j));
            }
            out.puts("\n");
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error in print_tecplot_data: %s\n", e.what());
    }
}

//-----------------------------------------------------------------------------
// 统一数据输出接口
//-----------------------------------------------------------------------------
void output_data(const Field2D& phi, const Field1D& eta, const Field1D& xx,
                 int count, const char* buf,
                 long nx, long ny, double h,
                 double xleft, double yleft)
{
    if (config::OUTPUT_MATLAB) {
        print_data(phi, eta, xx, count, buf, nx, ny);
    }
    
    if (config::OUTPUT_TECPLOT) {
        print_tecplot_data(phi, count, buf, nx, ny, h, xleft, yleft);
    }
}

//-----------------------------------------------------------------------------
// 写入详细运行日志
//-----------------------------------------------------------------------------
void write_detailed_log(const char* fname_log, int case_number,
                        const Params& p, const GridInfo& grid,
                        const PhysicsParams& phys, const AdsorptionZone& zone,
                        const RunLog& log, double dt_initial)
{
    std::string full_path = get_output_path(fname_log);
    
    try {
        SafeFile fp(full_path, "w");
        
        std::time_t now = std::time(nullptr);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        
        fp.puts("%%=============================================================\n");
        fp.puts("%%  DIFFUSION-CONVECTION SIMULATION LOG\n");
        fp.printf("%%  Case Number: %d\n", case_number);
        fp.printf("%%  Generated:   %s\n", time_buf);
        fp.puts("%%=============================================================\n\n");
        
        fp.puts("%% -------------------- INPUT PARAMETERS --------------------\n");
        fp.printf("case_number = %d;\n\n", case_number);
        
        fp.puts("%% Geometry and mesh\n");
        fp.printf("lam = %.6g;        %% aspect ratio (H/L)\n", p.lam);
        fp.printf("ny  = %ld;         %% grid points in y-direction\n", p.ny);
        fp.printf("nx  = %ld;         %% grid points in x-direction (computed)\n", grid.nx);
        fp.printf("h   = %.6e;   %% grid spacing\n", grid.h);
        fp.puts("\n");
        
        fp.puts("%% Computational domain\n");
        fp.printf("xleft  = %.6g;\n", grid.xleft);
        fp.printf("xright = %.6g;\n", grid.xright);
        fp.printf("yleft  = %.6g;\n", grid.yleft);
        fp.printf("yright = %.6g;\n", grid.yright);
        fp.printf("domain_length = %.6g;  %% L = xright - xleft\n", grid.xright - grid.xleft);
        fp.printf("domain_height = %.6g;  %% H = yright - yleft\n", grid.yright - grid.yleft);
        fp.puts("\n");
        
        fp.puts("%% Flow parameters\n");
        fp.printf("Pe    = %.6g;      %% Peclet number (steady flow)\n", phys.Pe);
        fp.printf("Pe2   = %.6g;      %% Peclet number (oscillatory flow)\n", phys.Pe2);
        fp.printf("alpha = %.6g;      %% Womersley number\n", phys.alpha);
        fp.printf("Sc    = %.6g;      %% Schmidt number\n", phys.Sc);
        fp.puts("\n");
        
        fp.puts("%% Reaction parameters\n");
        fp.printf("Da  = %.6g;        %% Damkohler number\n", phys.Da);
        fp.printf("K0  = %.6g;        %% equilibrium constant\n", phys.K0);
        fp.printf("eps = %.6g;        %% surface capacity parameter\n", phys.eps);
        fp.printf("c0  = %.6g;        %% inlet concentration\n", phys.c0);
        fp.puts("\n");
        
        fp.puts("%% Adsorption zone (dimensionless, then physical)\n");
        fp.printf("xpo_l_rel = %.6g;  %% relative position (0-1)\n", p.xpo_l);
        fp.printf("xpo_r_rel = %.6g;  %% relative position (0-1)\n", p.xpo_r);
        fp.printf("xpo_l = %.6g;      %% physical left boundary\n", zone.xpo_l);
        fp.printf("xpo_r = %.6g;      %% physical right boundary\n", zone.xpo_r);
        fp.printf("adsorption_length = %.6g;  %% length of adsorption zone\n", zone.xpo_r - zone.xpo_l);
        fp.puts("\n");
        
        fp.puts("%% Time integration\n");
        fp.printf("endT        = %.6g;    %% target end time\n", p.endT);
        fp.printf("coeff_dt    = %.6g;    %% dt coefficient (dt = coeff_dt * h^2)\n", p.coeff_dt);
        fp.printf("dt_initial  = %.6e;    %% initial time step\n", dt_initial);
        fp.printf("total_count = %ld;      %% planned output count\n", p.total_count);
        fp.puts("\n");
        
        fp.puts("%% Initial condition\n");
        fp.printf("x_ini_posi = %.6g;  %% initial concentration front position\n", p.x_ini_posi);
        fp.puts("\n");
        
        fp.puts("%% -------------------- THEORETICAL VALUES --------------------\n");
        double eta_eq = phys.K0 / (1.0 + phys.K0);
        double u_max = std::abs(phys.Pe) * 0.25 + std::abs(phys.Pe2);
        double dt_diff = 0.25 * grid.h * grid.h;
        double dt_conv = (u_max > 1e-10) ? (grid.h / u_max) : 1e10;
        
        fp.printf("eta_eq = %.10g;     %% equilibrium coverage = K0/(1+K0)\n", eta_eq);
        fp.printf("u_max_estimate = %.6g;  %% estimated maximum velocity\n", u_max);
        fp.puts("\n");
        
        fp.puts("%% CFL stability estimates\n");
        fp.printf("dt_diffusion_limit  = %.6e;  %% h^2/4 (2D explicit)\n", dt_diff);
        fp.printf("dt_convection_limit = %.6e;  %% h/u_max\n", dt_conv);
        fp.printf("dt_cfl_recommended  = %.6e;  %% 0.4 * min(dt_diff, dt_conv)\n", 
                 0.4 * std::min(dt_diff, dt_conv));
        fp.puts("\n");
        
        fp.puts("%% -------------------- MEMORY ESTIMATE --------------------\n");
        long total_cells = (grid.nx + 3) * (grid.ny + 3);
        double mem_matrix = total_cells * sizeof(double) / (1024.0 * 1024.0);
        double mem_vector = (grid.nx + 3) * sizeof(double) / (1024.0 * 1024.0);
        double mem_total = 3 * mem_matrix + 5 * mem_vector;
        
        fp.printf("total_grid_cells = %ld;  %% including ghost cells\n", total_cells);
        fp.printf("memory_per_matrix_MB = %.3f;\n", mem_matrix);
        fp.printf("memory_per_vector_MB = %.6f;\n", mem_vector);
        fp.printf("memory_total_estimate_MB = %.3f;  %% 3 matrices + 5 vectors\n", mem_total);
        fp.puts("\n");
        
        fp.puts("%% -------------------- RUNTIME EVENTS --------------------\n");
        fp.printf("nan_events = %d;  %% number of NaN occurrences\n", log.nan_events);
        fp.printf("resumed_from_checkpoint = %d;  %% 1=yes, 0=no\n", 
                 log.resumed_from_checkpoint ? 1 : 0);
        if (log.resumed_from_checkpoint) {
            fp.printf("resumed_at_iteration = %ld;\n", log.resumed_at_iteration);
        }
        fp.puts("\n");
        
        if (!log.dt_history.empty()) {
            fp.puts("%% dt adjustment history (iteration, old_dt, new_dt, sim_time)\n");
            fp.puts("dt_adjustments = [\n");
            for (const auto& adj : log.dt_history) {
                fp.printf("    %8ld, %.6e, %.6e, %.6e;  %% iter, old, new, time\n",
                        adj.iteration, adj.old_dt, adj.new_dt, adj.sim_time);
            }
            fp.puts("];\n\n");
        } else {
            fp.puts("%% No dt adjustments were needed (stable throughout)\n\n");
        }
        
        fp.puts("%% -------------------- CONVERGENCE HISTORY --------------------\n");
        if (!log.convergence_history.empty()) {
            fp.puts("%% Key convergence milestones (iteration, time, eta, rel_error)\n");
            fp.puts("convergence_milestones = [\n");
            for (const auto& pt : log.convergence_history) {
                fp.printf("    %8ld, %.6e, %.10g, %.6e;\n",
                        pt.iteration, pt.sim_time, pt.eta_ave, pt.rel_err);
            }
            fp.puts("];\n");
            fp.puts("convergence_milestones_headers = {'iteration', 'sim_time', 'eta_ave', 'rel_error'};\n\n");
        }
        
        fp.puts("%% -------------------- PERFORMANCE STATISTICS --------------------\n");
        fp.printf("actual_iterations = %ld;\n", log.actual_iterations);
        fp.printf("output_file_count = %d;\n", log.output_count);
        fp.puts("\n");
        
        fp.puts("%% Timing breakdown (seconds)\n");
        fp.printf("time_initialization = %.3f;\n", log.time_init);
        fp.printf("time_computation    = %.3f;\n", log.time_compute);
        fp.printf("time_io             = %.3f;\n", log.time_io);
        fp.printf("time_total          = %.3f;\n", log.time_total);
        fp.puts("\n");
        
        if (log.actual_iterations > 0 && log.time_compute > 0) {
            double time_per_iter = log.time_compute / log.actual_iterations * 1000.0;
            double iters_per_sec = log.actual_iterations / log.time_compute;
            fp.printf("time_per_iteration_ms = %.4f;\n", time_per_iter);
            fp.printf("iterations_per_second = %.1f;\n", iters_per_sec);
            fp.puts("\n");
        }
        
        fp.puts("%% -------------------- FINAL RESULTS --------------------\n");
        fp.printf("converged = %d;  %% 1=yes, 0=no (reached max_it)\n", log.converged ? 1 : 0);
        fp.printf("final_sim_time = %.10g;\n", log.final_sim_time);
        fp.printf("final_eta_ave  = %.10g;\n", log.final_eta);
        fp.printf("final_rel_error = %.6e;\n", log.final_rel_err);
        fp.puts("\n");
        
        if (log.converged) {
            fp.puts("%% Convergence achieved!\n");
            fp.printf("%% eta_ave reached within 1%% of eta_eq = %.10g\n", eta_eq);
        } else {
            fp.puts("%% WARNING: Simulation ended without convergence\n");
            fp.puts("%% Consider: (1) increase endT, (2) check parameters\n");
        }
        fp.puts("\n");
        
        fp.puts("%% -------------------- OUTPUT FILES --------------------\n");
        fp.printf("%% Data files are in: data_%d/\n", case_number);
        if (config::OUTPUT_MATLAB) {
            fp.puts("%%   cc_N.m  - concentration field (MATLAB format)\n");
            fp.puts("%%   ee_N.m  - surface coverage (MATLAB format)\n");
        }
        if (config::OUTPUT_TECPLOT) {
            fp.puts("%%   cc_N.dat - concentration field (Tecplot format)\n");
        }
        fp.puts("%% \n");
        fp.printf("%% Time series: eta_ave_%d.m\n", case_number);
        fp.puts("%%   Format: [time, eta_average, d(eta)/dt]\n");
        fp.puts("\n");
        
        fp.puts("%%=============================================================\n");
        fp.puts("%%  END OF LOG\n");
        fp.puts("%%=============================================================\n");
        
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error in write_detailed_log: %s\n", e.what());
    }
}
