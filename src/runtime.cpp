/*****************************************************************************
 * runtime.cpp
 *
 * Shared runtime for CPU and CUDA executables.
 *****************************************************************************/

#include "runtime.h"

#include "backend.h"
#include "checkpoint.h"
#include "config.h"
#include "file_utils.h"
#include "io.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#include <omp.h>

namespace fs = std::filesystem;

namespace {

void case_calculation(int case_number, Params p,
                      int case_index, int total_cases,
                      const ExecutionConfig& exec_config)
{
    const double start = omp_get_wtime();
    const double time_init_start = start;

    RunLog run_log;
    auto backend = create_backend(exec_config);

    const long nx = static_cast<long>(p.ny * (1.0 / p.lam));
    const double yleft = 0.0;
    const double yright = 1.0;
    const double xleft = 0.0;
    const double xright = (static_cast<double>(nx) / p.ny) * yright;
    const double h = (yright - yleft) / p.ny;
    double dt = p.coeff_dt * h * h;
    const double dt_initial = dt;

    const double xl = p.xpo_l * xright;
    const double xr = p.xpo_r * xright;

    long max_it = static_cast<long>(p.endT / dt + sim::EPS_EPOCH);
    long ns = static_cast<long>(max_it / p.total_count + sim::EPS_EPOCH);
    if (ns <= 0) {
        ns = 1;
    }

    GridInfo grid;
    grid.nx = nx;
    grid.ny = p.ny;
    grid.h = h;
    grid.xleft = xleft;
    grid.xright = xright;
    grid.yleft = yleft;
    grid.yright = yright;

    PhysicsParams phys;
    phys.lam = p.lam;
    phys.Pe = p.Pe;
    phys.Pe2 = p.Pe2;
    phys.eps = p.eps;
    phys.Da = p.Da;
    phys.K0 = p.K0;
    phys.c0 = 1.0;
    phys.alpha = p.alpha;
    phys.Sc = sim::SC_DEFAULT;

    AdsorptionZone zone;
    zone.xpo_l = xl;
    zone.xpo_r = xr;

    SimFields fields;
    fields.resize(nx, p.ny);
    fields.zero_all();
    backend->initialize(fields, grid, p.x_ini_posi);

    run_log.time_init = omp_get_wtime() - time_init_start;
    const double time_compute_start = omp_get_wtime();

    std::string output_prefix = config::OUTPUT_DIR.empty() ? "" : config::OUTPUT_DIR + "/";

    char fname_data[128];
    char fname_eta[128];
    char fname_log[128];
    std::snprintf(fname_data, sizeof(fname_data), "data_%d", case_number);
    std::snprintf(fname_eta, sizeof(fname_eta), "%seta_ave_%d.m", output_prefix.c_str(), case_number);
    std::snprintf(fname_log, sizeof(fname_log), "remarks_%d.m", case_number);

    if (!config::OUTPUT_DIR.empty()) {
        ensure_dir(config::OUTPUT_DIR);
    }

    try {
        SafeFile fp(fname_eta, "w");
        fp.printf(" %f  %f %f\n", 0.0, 0.0, 0.0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error creating eta file: %s\n", e.what());
    }

    double ct = 0.0;
    int count = 1;
    double eta_ave = 0.0;
    double old_eta = 0.0;
    double eta_ave_dt = 0.0;
    int dt_adjustments = 0;
    long start_iteration = 1;
    long last_completed_iteration = 0;

    if (!config::CHECKPOINT_DIR.empty()) {
        ensure_dir(config::CHECKPOINT_DIR);
    }
    const std::string chkpt_filename = get_checkpoint_filename(case_number);

    const bool force_restart = config::FORCE_RESTART_ALL || check_force_restart(case_number);

    if (!force_restart && checkpoint_exists(case_number)) {
        Checkpoint chkpt;
        if (load_checkpoint(chkpt_filename.c_str(), chkpt, fields.cc, fields.ee, nx, p.ny)) {
            ct = chkpt.sim_time;
            dt = chkpt.dt;
            count = chkpt.output_count;
            dt_adjustments = chkpt.dt_adjustments;
            eta_ave = chkpt.eta_ave;
            old_eta = chkpt.old_eta;
            p.coeff_dt = chkpt.coeff_dt;
            start_iteration = chkpt.iteration + 1;
            last_completed_iteration = chkpt.iteration;

            max_it = static_cast<long>(p.endT / dt + sim::EPS_EPOCH);
            ns = static_cast<long>(max_it / p.total_count + sim::EPS_EPOCH);
            if (ns <= 0) {
                ns = 1;
            }

            backend->sync_device(fields);

            run_log.resumed_from_checkpoint = true;
            run_log.resumed_at_iteration = chkpt.iteration;
        }
    }

    int last_milestone = -1;
    const double eta_eq = p.K0 / (1.0 + p.K0);

    backend->sync_host(fields);
    output_data(fields.cc, fields.ee, fields.xx, 0, fname_data,
                nx, p.ny, h, xleft, yleft);

    for (long it = start_iteration; it <= max_it; ++it) {
        ct += dt;

        backend->full_step_explicit(fields, grid, phys, zone, ct, dt);
        last_completed_iteration = it;

        if (backend->has_nan(fields, grid)) {
            if (++dt_adjustments > sim::MAX_ADJUST) {
                break;
            }

            const double old_dt_value = dt;
            p.coeff_dt *= sim::DT_SHRINK;
            dt = p.coeff_dt * grid.h * grid.h;

            run_log.dt_history.push_back({it, old_dt_value, dt, ct});
            run_log.nan_events++;

            max_it = static_cast<long>(p.endT / dt + sim::EPS_EPOCH);
            ns = static_cast<long>(max_it / p.total_count + sim::EPS_EPOCH);
            if (ns <= 0) {
                ns = 1;
            }

            backend->zero_state(fields, grid);
            backend->initialize(fields, grid, p.x_ini_posi);

            ct = 0.0;
            count = 1;
            eta_ave = 0.0;
            old_eta = 0.0;
            last_completed_iteration = 0;
            last_milestone = -1;
            it = 0;
            continue;
        }

        if (it % config::STATS_INTERVAL == 0) {
            old_eta = eta_ave;
            eta_ave = backend->compute_eta_average(fields, grid, zone);
            eta_ave_dt =
                (eta_ave - old_eta) / dt / static_cast<double>(config::STATS_INTERVAL);

            try {
                SafeFile fp(fname_eta, "a");
                fp.printf(" %f  %g %g\n", ct, eta_ave, eta_ave_dt);
            } catch (...) {
            }

            double progress = (eta_eq > 1e-10) ? (eta_ave / eta_eq * 100.0) : 0.0;
            if (progress > 100.0) {
                progress = 100.0;
            }
            const double rel_err =
                (eta_eq > 1e-10) ? std::fabs(eta_ave - eta_eq) / eta_eq : 1.0;

            bool should_record = false;
            if (run_log.convergence_history.empty()) {
                should_record = true;
            } else {
                const auto& last = run_log.convergence_history.back();
                const double last_progress =
                    (eta_eq > 1e-10) ? (last.eta_ave / eta_eq * 100.0) : 0.0;
                if (progress - last_progress >= 10.0 || last.rel_err / rel_err >= 2.0) {
                    should_record = true;
                }
            }
            if (should_record) {
                run_log.convergence_history.push_back({it, ct, eta_ave, rel_err});
            }

            const int current_milestone = static_cast<int>(progress / 10.0) * 10;
            if (current_milestone > last_milestone && current_milestone <= 100) {
                last_milestone = current_milestone;
                const double elapsed = omp_get_wtime() - start;

                #pragma omp critical
                {
                    std::printf("[Case %d][%s] %3d%% | eta=%.4f | err=%.1e | used %.1fs\n",
                                case_number, backend->name(), current_milestone,
                                eta_ave, rel_err, elapsed);
                    std::fflush(stdout);
                }
            }

            if (config::CONVERGENCE_THRESHOLD > 0 &&
                rel_err < config::CONVERGENCE_THRESHOLD) {
                run_log.converged = true;
                run_log.final_eta = eta_ave;
                run_log.final_rel_err = rel_err;
                run_log.final_sim_time = ct;
                run_log.actual_iterations = it;
                run_log.output_count = count;

                const double elapsed = omp_get_wtime() - start;
                #pragma omp critical
                {
                    int lines_up = total_cases - case_index;
                    if (lines_up < 1) {
                        lines_up = 1;
                    }
                    std::printf("\033[%dA\r\033[2K", lines_up);
                    std::printf("[Case %d][%s] Converged! eta=%.4f | used %.1fs",
                                case_number, backend->name(), eta_ave, elapsed);
                    std::printf("\033[%dB\r", lines_up);
                    std::fflush(stdout);
                }

                const double io_start = omp_get_wtime();
                backend->sync_host(fields);
                output_data(fields.cc, fields.ee, fields.xx, count + 1000, fname_data,
                            nx, p.ny, h, xleft, yleft);
                run_log.time_io += omp_get_wtime() - io_start;

                break;
            }
        }

        if (it % ns == 0) {
            const double io_start = omp_get_wtime();
            backend->sync_host(fields);
            output_data(fields.cc, fields.ee, fields.xx, count, fname_data,
                        nx, p.ny, h, xleft, yleft);
            run_log.time_io += omp_get_wtime() - io_start;
            ++count;
        }

        if (it % config::CHECKPOINT_INTERVAL == 0) {
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

            backend->sync_host(fields);
            save_checkpoint(chkpt_filename.c_str(), chkpt, fields.cc, fields.ee, nx, p.ny);
        }
    }

    if (!run_log.converged) {
        run_log.final_eta = eta_ave;
        run_log.final_rel_err =
            (eta_eq > 1e-10) ? std::fabs(eta_ave - eta_eq) / eta_eq : 1.0;
        run_log.final_sim_time = ct;
        run_log.actual_iterations = last_completed_iteration;
        run_log.output_count = count - 1;

        const double elapsed = omp_get_wtime() - start;
        #pragma omp critical
        {
            int lines_up = total_cases - case_index;
            if (lines_up < 1) {
                lines_up = 1;
            }
            std::printf("\033[%dA\r\033[2K", lines_up);
            std::printf("[Case %d][%s] Finished (not converged) | eta=%.4f | used %.1fs",
                        case_number, backend->name(), eta_ave, elapsed);
            std::printf("\033[%dB\r", lines_up);
            std::fflush(stdout);
        }
    }

    run_log.time_compute = omp_get_wtime() - time_compute_start - run_log.time_io;
    run_log.time_total = omp_get_wtime() - start;

    write_detailed_log(fname_log, case_number, p, grid, phys, zone, run_log, dt_initial);
}

}  // namespace

int run_cases(const ExecutionConfig& exec_config)
{
    if (exec_config.backend == ComputeBackend::Cpu) {
        if (config::NUM_THREADS > 0) {
            omp_set_num_threads(config::NUM_THREADS);
        } else {
            omp_set_num_threads(std::max(1, omp_get_max_threads()));
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  2D Diffusion-Convection Solver" << std::endl;
    std::cout << "  Backend: "
              << (exec_config.backend == ComputeBackend::Cpu ? "CPU/OpenMP" : "CUDA")
              << std::endl;
    if (exec_config.backend == ComputeBackend::Cpu) {
        std::cout << "  OpenMP Threads: " << omp_get_max_threads() << std::endl;
    } else {
        std::cout << "  CUDA Device: " << exec_config.device_id << std::endl;
        std::cout << "  Case Scheduling: serial on single GPU" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    std::vector<int> case_numbers;

    if (config::USE_CASE_LIST) {
        case_numbers = config::get_case_list();
        std::cout << "Running user-specified cases: ";
        for (std::size_t i = 0; i < case_numbers.size(); ++i) {
            std::cout << case_numbers[i];
            if (i + 1 < case_numbers.size()) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
    } else {
        std::regex pattern(R"(^input_parameter_(\d+)\.txt$)");
        std::smatch matches;

        fs::path scan_path = config::INPUT_DIR.empty()
                           ? fs::current_path()
                           : fs::path(config::INPUT_DIR);

        if (!fs::exists(scan_path)) {
            std::cerr << "Error: Input directory '" << scan_path
                      << "' does not exist." << std::endl;
            return EXIT_FAILURE;
        }

        for (const auto& entry : fs::directory_iterator(scan_path)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string fname = entry.path().filename().string();
            if (std::regex_match(fname, matches, pattern)) {
                case_numbers.push_back(std::stoi(matches[1]));
            }
        }

        if (case_numbers.empty()) {
            std::cerr << "Error: No input_parameter_X.txt found." << std::endl;
            return EXIT_FAILURE;
        }

        std::sort(case_numbers.begin(), case_numbers.end());
        std::cout << "Found " << case_numbers.size() << " case(s)." << std::endl;
    }

    if (config::FORCE_RESTART_ALL) {
        std::cout << "Force restart mode: ALL cases will start fresh." << std::endl;
    }

    if (exec_config.backend == ComputeBackend::Cpu) {
        #pragma omp parallel for schedule(dynamic)
        for (long idx = 0; idx < static_cast<long>(case_numbers.size()); ++idx) {
            const int case_no = case_numbers[static_cast<std::size_t>(idx)];
            Params p;

            try {
                p = read_parameter(case_no);
            } catch (const std::exception& e) {
                #pragma omp critical
                {
                    int lines_up = static_cast<int>(case_numbers.size() - idx);
                    std::printf("\033[%dA\r\033[2K", lines_up);
                    std::printf("[Case %d] Error: %s", case_no, e.what());
                    std::printf("\033[%dB\r", lines_up);
                    std::fflush(stdout);
                }
                continue;
            }

            case_calculation(case_no, p,
                             static_cast<int>(idx),
                             static_cast<int>(case_numbers.size()),
                             exec_config);
        }
    } else {
        for (std::size_t idx = 0; idx < case_numbers.size(); ++idx) {
            const int case_no = case_numbers[idx];
            Params p;

            try {
                p = read_parameter(case_no);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[Case %d] Error: %s\n", case_no, e.what());
                continue;
            }

            case_calculation(case_no, p,
                             static_cast<int>(idx),
                             static_cast<int>(case_numbers.size()),
                             exec_config);
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  All cases completed." << std::endl;
    std::cout << "========================================" << std::endl;

    return EXIT_SUCCESS;
}
