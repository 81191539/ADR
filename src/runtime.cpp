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
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <omp.h>

namespace fs = std::filesystem;

namespace {

void print_usage(const char* program)
{
    std::cout << "Usage: " << program << " [--case N | --cases N[,M...]] [--force-restart]\n"
              << "       " << program << " --benchmark-concurrency N --benchmark-cases A,B "
              << "[--benchmark-seconds S] [--benchmark-warmup-seconds S]\n"
              << "\n"
              << "Options:\n"
              << "  --case N          Run one case number.\n"
              << "  --cases A,B,C     Run a comma-separated case list.\n"
              << "  --force-restart   Ignore checkpoint files and start fresh.\n"
              << "  --benchmark-concurrency N  Run CPU case-concurrency benchmark with N workers.\n"
              << "  --benchmark-cases A,B,C     Case parameter files used by benchmark workers.\n"
              << "  --benchmark-seconds S       Measurement duration in seconds.\n"
              << "  --benchmark-warmup-seconds S Warmup duration in seconds.\n"
              << "  -h, --help        Show this help.\n";
}

bool parse_case_number(const std::string& text, int& value, std::string& error)
{
    try {
        std::size_t parsed = 0;
        const long number = std::stol(text, &parsed, 10);
        if (parsed != text.size()) {
            error = "Invalid case number '" + text + "'.";
            return false;
        }
        if (number <= 0 || number > std::numeric_limits<int>::max()) {
            error = "Case number must be a positive integer: '" + text + "'.";
            return false;
        }
        value = static_cast<int>(number);
        return true;
    } catch (const std::exception&) {
        error = "Invalid case number '" + text + "'.";
        return false;
    }
}

bool parse_case_list(const std::string& text,
                     std::vector<int>& case_numbers,
                     std::string& error)
{
    std::stringstream stream(text);
    std::string token;
    std::vector<int> parsed_cases;

    while (std::getline(stream, token, ',')) {
        if (token.empty()) {
            error = "Case list must not contain empty entries.";
            return false;
        }
        int case_number = 0;
        if (!parse_case_number(token, case_number, error)) {
            return false;
        }
        parsed_cases.push_back(case_number);
    }

    if (parsed_cases.empty()) {
        error = "Case list must contain at least one case number.";
        return false;
    }

    case_numbers = parsed_cases;
    return true;
}

bool parse_positive_double(const std::string& text, double& value, std::string& error)
{
    try {
        std::size_t parsed = 0;
        value = std::stod(text, &parsed);
        if (parsed != text.size() || !std::isfinite(value) || value <= 0.0) {
            error = "Expected a positive number: '" + text + "'.";
            return false;
        }
        return true;
    } catch (const std::exception&) {
        error = "Expected a positive number: '" + text + "'.";
        return false;
    }
}

bool parse_runtime_args(int argc, char* argv[],
                        ExecutionConfig& exec_config,
                        bool& show_help,
                        std::string& error)
{
    show_help = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            show_help = true;
            return true;
        }

        if (arg == "--force-restart") {
            exec_config.force_restart = true;
            continue;
        }

        if (arg == "--benchmark-concurrency" ||
            arg == "--benchmark-cases" ||
            arg == "--benchmark-seconds" ||
            arg == "--benchmark-warmup-seconds") {
            if (i + 1 >= argc) {
                error = arg + " requires a value.";
                return false;
            }

            const std::string value = argv[++i];
            if (arg == "--benchmark-concurrency") {
                int concurrency = 0;
                if (!parse_case_number(value, concurrency, error)) {
                    return false;
                }
                exec_config.benchmark_mode = true;
                exec_config.benchmark_concurrency = concurrency;
            } else if (arg == "--benchmark-cases") {
                exec_config.benchmark_mode = true;
                if (!parse_case_list(value, exec_config.benchmark_case_numbers, error)) {
                    return false;
                }
            } else if (arg == "--benchmark-seconds") {
                exec_config.benchmark_mode = true;
                if (!parse_positive_double(value, exec_config.benchmark_seconds, error)) {
                    return false;
                }
            } else {
                exec_config.benchmark_mode = true;
                if (!parse_positive_double(value, exec_config.benchmark_warmup_seconds, error)) {
                    return false;
                }
            }
            continue;
        }

        if (arg == "--case" || arg == "--cases") {
            if (i + 1 >= argc) {
                error = arg + " requires a value.";
                return false;
            }

            const std::string value = argv[++i];
            if (arg == "--case") {
                int case_number = 0;
                if (!parse_case_number(value, case_number, error)) {
                    return false;
                }
                exec_config.case_numbers = {case_number};
            } else if (!parse_case_list(value, exec_config.case_numbers, error)) {
                return false;
            }
            continue;
        }

        const std::string case_prefix = "--case=";
        const std::string cases_prefix = "--cases=";
        const std::string benchmark_concurrency_prefix = "--benchmark-concurrency=";
        const std::string benchmark_cases_prefix = "--benchmark-cases=";
        const std::string benchmark_seconds_prefix = "--benchmark-seconds=";
        const std::string benchmark_warmup_prefix = "--benchmark-warmup-seconds=";
        if (arg.rfind(benchmark_concurrency_prefix, 0) == 0) {
            int concurrency = 0;
            if (!parse_case_number(arg.substr(benchmark_concurrency_prefix.size()),
                                   concurrency,
                                   error)) {
                return false;
            }
            exec_config.benchmark_mode = true;
            exec_config.benchmark_concurrency = concurrency;
            continue;
        }
        if (arg.rfind(benchmark_cases_prefix, 0) == 0) {
            exec_config.benchmark_mode = true;
            if (!parse_case_list(arg.substr(benchmark_cases_prefix.size()),
                                 exec_config.benchmark_case_numbers,
                                 error)) {
                return false;
            }
            continue;
        }
        if (arg.rfind(benchmark_seconds_prefix, 0) == 0) {
            exec_config.benchmark_mode = true;
            if (!parse_positive_double(arg.substr(benchmark_seconds_prefix.size()),
                                       exec_config.benchmark_seconds,
                                       error)) {
                return false;
            }
            continue;
        }
        if (arg.rfind(benchmark_warmup_prefix, 0) == 0) {
            exec_config.benchmark_mode = true;
            if (!parse_positive_double(arg.substr(benchmark_warmup_prefix.size()),
                                       exec_config.benchmark_warmup_seconds,
                                       error)) {
                return false;
            }
            continue;
        }
        if (arg.rfind(case_prefix, 0) == 0) {
            int case_number = 0;
            if (!parse_case_number(arg.substr(case_prefix.size()), case_number, error)) {
                return false;
            }
            exec_config.case_numbers = {case_number};
            continue;
        }
        if (arg.rfind(cases_prefix, 0) == 0) {
            if (!parse_case_list(arg.substr(cases_prefix.size()),
                                 exec_config.case_numbers,
                                 error)) {
                return false;
            }
            continue;
        }

        error = "Unknown argument: " + arg;
        return false;
    }

    return true;
}

struct DenseDumpRecord {
    int    index{};
    long   iteration{};
    double sim_time{};
};

fs::path dense_dump_dir(int case_number)
{
    char dirname[64];
    std::snprintf(dirname, sizeof(dirname), "data_%d_dense", case_number);

    if (config::OUTPUT_DIR.empty()) {
        return fs::path(dirname);
    }
    return fs::path(config::OUTPUT_DIR) / dirname;
}

bool is_dense_dump_file(const fs::path& path)
{
    static const std::regex dense_file_pattern(R"(^((cc|ee)_[0-9]+|times)\.m$)");
    return std::regex_match(path.filename().string(), dense_file_pattern);
}

void prepare_dense_dump_directory(const fs::path& dir)
{
    ensure_dir(dir.string());

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file() || !is_dense_dump_file(entry.path())) {
            continue;
        }

        std::error_code error;
        fs::remove(entry.path(), error);
        if (error) {
            std::fprintf(stderr, "Warning: could not remove dense dump file '%s': %s\n",
                         entry.path().string().c_str(), error.message().c_str());
        }
    }
}

bool dump_dense_snapshot(const fs::path& dir,
                         const Field2D& cc,
                         const Field1D& ee,
                         const Field1D& xx,
                         int index,
                         long nx,
                         long ny)
{
    const fs::path cc_path = dir / ("cc_" + std::to_string(index) + ".m");
    const fs::path ee_path = dir / ("ee_" + std::to_string(index) + ".m");

    try {
        SafeFile fcc(cc_path.string(), "w");
        for (long i = 0; i <= nx; ++i) {
            for (long j = 0; j <= ny; ++j) {
                fcc.printf(" %16.14f ", cc(i, j));
            }
            fcc.puts("\n");
        }

        SafeFile fee(ee_path.string(), "w");
        for (long i = 0; i <= nx; ++i) {
            fee.printf(" %16.14f  %16.14f\n", xx(i), ee(i));
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error writing dense dump snapshot %d: %s\n",
                     index, e.what());
        return false;
    }

    return true;
}

void write_dense_times_index(const fs::path& dir,
                             const std::vector<DenseDumpRecord>& records)
{
    try {
        SafeFile fp((dir / "times.m").string(), "w");
        for (const auto& record : records) {
            fp.printf(" %d %ld %.17g\n",
                      record.index, record.iteration, record.sim_time);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error writing dense dump times index: %s\n", e.what());
    }
}

std::vector<long> make_dense_target_offsets(int count, double period_steps)
{
    std::vector<long> offsets;
    if (count <= 0) {
        return offsets;
    }

    if (count == 1) {
        offsets.push_back(0);
        return offsets;
    }

    const double denominator = static_cast<double>(count - 1);
    for (int k = 0; k < count; ++k) {
        long offset = static_cast<long>(
            std::llround(static_cast<double>(k) * period_steps / denominator));
        if (offset < 0) {
            offset = 0;
        }
        if (offsets.empty() || offsets.back() != offset) {
            offsets.push_back(offset);
        }
    }

    return offsets;
}

struct PreparedCase {
    GridInfo grid;
    PhysicsParams phys;
    AdsorptionZone zone;
    double dt{};
    double x_ini_posi{};
};

PreparedCase prepare_case_for_benchmark(const Params& p)
{
    PreparedCase prepared;
    const long nx = static_cast<long>(p.ny * (1.0 / p.lam));
    const double yleft = 0.0;
    const double yright = 1.0;
    const double xleft = 0.0;
    const double xright = (static_cast<double>(nx) / p.ny) * yright;
    const double h = (yright - yleft) / p.ny;

    prepared.grid.nx = nx;
    prepared.grid.ny = p.ny;
    prepared.grid.h = h;
    prepared.grid.xleft = xleft;
    prepared.grid.xright = xright;
    prepared.grid.yleft = yleft;
    prepared.grid.yright = yright;

    prepared.phys.lam = p.lam;
    prepared.phys.Pe = p.Pe;
    prepared.phys.Pe2 = p.Pe2;
    prepared.phys.eps = p.eps;
    prepared.phys.Da = p.Da;
    prepared.phys.K0 = p.K0;
    prepared.phys.c0 = 1.0;
    prepared.phys.alpha = p.alpha;
    prepared.phys.Sc = sim::SC_DEFAULT;

    prepared.zone.xpo_l = p.xpo_l * xright;
    prepared.zone.xpo_r = p.xpo_r * xright;
    prepared.dt = p.coeff_dt * h * h;
    prepared.x_ini_posi = p.x_ini_posi;
    return prepared;
}

int run_benchmark(const ExecutionConfig& exec_config)
{
    if (exec_config.backend != ComputeBackend::Cpu) {
        std::cerr << "Benchmark mode is only supported by the CPU backend." << std::endl;
        return EXIT_FAILURE;
    }
    if (exec_config.benchmark_case_numbers.empty()) {
        std::cerr << "Benchmark mode requires --benchmark-cases." << std::endl;
        return EXIT_FAILURE;
    }

    const int logical_processors = std::max(1, omp_get_num_procs());
    const int concurrency = std::max(
        1,
        std::min(exec_config.benchmark_concurrency, logical_processors));

    std::vector<PreparedCase> cases;
    cases.reserve(exec_config.benchmark_case_numbers.size());
    for (int case_number : exec_config.benchmark_case_numbers) {
        cases.push_back(prepare_case_for_benchmark(read_parameter(case_number)));
    }

    omp_set_max_active_levels(1);
    double total_iterations = 0.0;
    const double warmup_start = omp_get_wtime();
    const double warmup_end = warmup_start + exec_config.benchmark_warmup_seconds;
    const double measure_end = warmup_end + exec_config.benchmark_seconds;

    #pragma omp parallel num_threads(concurrency) reduction(+:total_iterations)
    {
        const int thread_id = omp_get_thread_num();
        const PreparedCase& prepared =
            cases[static_cast<std::size_t>(thread_id) % cases.size()];
        auto backend = create_cpu_backend();
        SimFields fields;
        fields.resize(prepared.grid.nx, prepared.grid.ny);
        fields.zero_all();
        backend->initialize(fields, prepared.grid, prepared.x_ini_posi);

        long local_iterations = 0;
        double now = omp_get_wtime();
        while (now < measure_end) {
            backend->full_step_explicit(fields, prepared.grid, prepared.phys,
                                        prepared.zone,
                                        static_cast<double>(local_iterations + 1) *
                                            prepared.dt,
                                        prepared.dt);
            ++local_iterations;
            now = omp_get_wtime();
            if (now < warmup_end) {
                local_iterations = 0;
            }
        }
        total_iterations += static_cast<double>(local_iterations);
    }

    const double total_throughput =
        total_iterations / exec_config.benchmark_seconds;
    const double per_worker_throughput = total_throughput / concurrency;

    std::cout << "{\"benchmark\":true"
              << ",\"concurrency\":" << concurrency
              << ",\"logical_processors\":" << logical_processors
              << ",\"sample_cases\":" << cases.size()
              << ",\"warmup_seconds\":" << exec_config.benchmark_warmup_seconds
              << ",\"measurement_seconds\":" << exec_config.benchmark_seconds
              << ",\"total_iterations\":" << static_cast<long long>(total_iterations)
              << ",\"iterations_per_second\":" << total_throughput
              << ",\"iterations_per_second_per_worker\":" << per_worker_throughput
              << "}" << std::endl;

    return EXIT_SUCCESS;
}

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

    const bool dense_dump_enabled =
        config::ENABLE_DENSE_DUMP && config::DENSE_DUMP_COUNT > 0;
    const double dense_period =
        phys::PI / (phys.alpha * phys.alpha * phys.Sc);
    const fs::path dense_dir = dense_dump_dir(case_number);
    std::vector<DenseDumpRecord> dense_times;
    std::vector<long> dense_target_offsets;
    int dense_dump_count = 0;
    bool dense_dump_active = false;
    std::size_t dense_next_target = 0;
    long dense_anchor_iteration = 0;

    auto reset_dense_dump = [&]() {
        dense_times.clear();
        dense_target_offsets.clear();
        dense_dump_count = 0;
        dense_dump_active = false;
        dense_next_target = 0;
        dense_anchor_iteration = 0;
        if (dense_dump_enabled) {
            prepare_dense_dump_directory(dense_dir);
        }
    };

    reset_dense_dump();

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

    auto record_dense_snapshot = [&](long iteration) {
        if (!dense_dump_enabled ||
            dense_dump_count >= config::DENSE_DUMP_COUNT) {
            return;
        }

        const double io_start = omp_get_wtime();
        backend->sync_host(fields);
        if (dump_dense_snapshot(dense_dir, fields.cc, fields.ee, fields.xx,
                                dense_dump_count, nx, p.ny)) {
            dense_times.push_back({dense_dump_count, iteration, ct});
            ++dense_dump_count;
        }
        run_log.time_io += omp_get_wtime() - io_start;
    };

    auto start_dense_window = [&](long iteration) {
        dense_dump_active = true;
        dense_anchor_iteration = iteration;
        dense_next_target = 0;
        dense_target_offsets = make_dense_target_offsets(
            config::DENSE_DUMP_COUNT, dense_period / dt);
    };

    auto record_due_dense_snapshots = [&](long iteration) {
        if (!dense_dump_enabled || !dense_dump_active) {
            return;
        }

        while (dense_next_target < dense_target_offsets.size() &&
               dense_dump_count < config::DENSE_DUMP_COUNT &&
               iteration >= dense_anchor_iteration +
                            dense_target_offsets[dense_next_target]) {
            record_dense_snapshot(iteration);
            ++dense_next_target;
        }
    };

    auto dense_snapshot_due = [&](long iteration) {
        if (!dense_dump_enabled ||
            dense_dump_count >= config::DENSE_DUMP_COUNT) {
            return false;
        }
        if (!dense_dump_active) {
            return ct + sim::EPS_EPOCH >= config::T_DENSE_DUMP_START;
        }
        return dense_next_target < dense_target_offsets.size() &&
               iteration >= dense_anchor_iteration +
                            dense_target_offsets[dense_next_target];
    };

    if (!config::CHECKPOINT_DIR.empty()) {
        ensure_dir(config::CHECKPOINT_DIR);
    }
    const std::string chkpt_filename = get_checkpoint_filename(case_number);

    const bool force_restart = exec_config.force_restart ||
                               config::FORCE_RESTART_ALL ||
                               check_force_restart(case_number);

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
    long last_stability_checked_iteration = -1;
    bool stability_abort = false;

    auto handle_unstable_state = [&](long iteration) {
        last_stability_checked_iteration = -1;
        if (++dt_adjustments > sim::MAX_ADJUST) {
            stability_abort = true;
            return false;
        }

        const double old_dt_value = dt;
        p.coeff_dt *= sim::DT_SHRINK;
        dt = p.coeff_dt * grid.h * grid.h;

        run_log.dt_history.push_back({iteration, old_dt_value, dt, ct});
        run_log.instability_events++;
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
        reset_dense_dump();
        if (config::T_DENSE_DUMP_START <= 0.0) {
            start_dense_window(0);
            record_due_dense_snapshots(0);
        }
        return true;
    };

    auto check_stability = [&](long iteration, bool force) {
        const bool scheduled =
            config::STABILITY_CHECK_INTERVAL <= 1 ||
            iteration % config::STABILITY_CHECK_INTERVAL == 0;
        if (!force && !scheduled) {
            return false;
        }
        if (last_stability_checked_iteration == iteration) {
            return false;
        }

        last_stability_checked_iteration = iteration;
        if (!backend->has_unstable_values(fields, grid)) {
            return false;
        }
        return handle_unstable_state(iteration);
    };

    backend->sync_host(fields);
    output_data(fields.cc, fields.ee, fields.xx, 0, fname_data,
                nx, p.ny, h, xleft, yleft);
    if (dense_dump_enabled &&
        (config::T_DENSE_DUMP_START <= 0.0 ||
         ct + sim::EPS_EPOCH >= config::T_DENSE_DUMP_START)) {
        start_dense_window(last_completed_iteration);
        record_due_dense_snapshots(last_completed_iteration);
    }

    for (long it = start_iteration; it <= max_it; ++it) {
        ct += dt;

        backend->full_step_explicit(fields, grid, phys, zone, ct, dt);
        last_completed_iteration = it;

        if (check_stability(it, false)) {
            it = 0;
            continue;
        }
        if (stability_abort) {
            break;
        }

        if (it % config::STATS_INTERVAL == 0) {
            if (check_stability(it, true)) {
                it = 0;
                continue;
            }
            if (stability_abort) {
                break;
            }

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
            if (check_stability(it, true)) {
                it = 0;
                continue;
            }
            if (stability_abort) {
                break;
            }

            const double io_start = omp_get_wtime();
            backend->sync_host(fields);
            output_data(fields.cc, fields.ee, fields.xx, count, fname_data,
                        nx, p.ny, h, xleft, yleft);
            run_log.time_io += omp_get_wtime() - io_start;
            ++count;
        }

        if (dense_dump_enabled &&
            (!dense_dump_active ||
             dense_next_target < dense_target_offsets.size())) {
            if (dense_snapshot_due(it)) {
                if (check_stability(it, true)) {
                    it = 0;
                    continue;
                }
                if (stability_abort) {
                    break;
                }
            }
            if (!dense_dump_active &&
                ct + sim::EPS_EPOCH >= config::T_DENSE_DUMP_START) {
                start_dense_window(it);
            }
            record_due_dense_snapshots(it);
        }

        if (it % config::CHECKPOINT_INTERVAL == 0) {
            if (check_stability(it, true)) {
                it = 0;
                continue;
            }
            if (stability_abort) {
                break;
            }

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

    if (dense_dump_enabled) {
        write_dense_times_index(dense_dir, dense_times);
    }

    write_detailed_log(fname_log, case_number, p, grid, phys, zone, run_log, dt_initial);
}

}  // namespace

int run_cases_with_args(ExecutionConfig exec_config, int argc, char* argv[])
{
    bool show_help = false;
    std::string error;

    if (!parse_runtime_args(argc, argv, exec_config, show_help, error)) {
        std::cerr << "Error: " << error << std::endl;
        print_usage(argc > 0 ? argv[0] : "df2d");
        return EXIT_FAILURE;
    }

    if (show_help) {
        print_usage(argc > 0 ? argv[0] : "df2d");
        return EXIT_SUCCESS;
    }

    if (exec_config.benchmark_mode) {
        return run_benchmark(exec_config);
    }

    return run_cases(exec_config);
}

int run_cases(const ExecutionConfig& exec_config)
{
    if (exec_config.backend == ComputeBackend::Cpu) {
        if (config::NUM_THREADS > 0) {
            omp_set_num_threads(config::NUM_THREADS);
        } else {
            omp_set_num_threads(std::max(1, omp_get_max_threads()));
        }
        omp_set_max_active_levels(1);
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

    if (!exec_config.case_numbers.empty()) {
        case_numbers = exec_config.case_numbers;
        std::cout << "Running runtime-selected cases: ";
        for (std::size_t i = 0; i < case_numbers.size(); ++i) {
            std::cout << case_numbers[i];
            if (i + 1 < case_numbers.size()) {
                std::cout << ", ";
            }
        }
        std::cout << std::endl;
    } else if (config::USE_CASE_LIST) {
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
        std::regex pattern(R"(^input_parameter_(\d+)\.(toml|txt)$)");
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

        std::sort(case_numbers.begin(), case_numbers.end());
        case_numbers.erase(std::unique(case_numbers.begin(), case_numbers.end()),
                           case_numbers.end());

        if (case_numbers.empty()) {
            std::cerr << "Error: No input_parameter_X.toml or input_parameter_X.txt found."
                      << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "Found " << case_numbers.size() << " case(s)." << std::endl;
    }

    if (exec_config.force_restart || config::FORCE_RESTART_ALL) {
        std::cout << "Force restart mode: ALL cases will start fresh." << std::endl;
    }

    int completed_cases = 0;
    int failed_cases = 0;

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
                #pragma omp atomic
                failed_cases++;
                continue;
            }

            case_calculation(case_no, p,
                             static_cast<int>(idx),
                             static_cast<int>(case_numbers.size()),
                             exec_config);
            #pragma omp atomic
            completed_cases++;
        }
    } else {
        for (std::size_t idx = 0; idx < case_numbers.size(); ++idx) {
            const int case_no = case_numbers[idx];
            Params p;

            try {
                p = read_parameter(case_no);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[Case %d] Error: %s\n", case_no, e.what());
                ++failed_cases;
                continue;
            }

            case_calculation(case_no, p,
                             static_cast<int>(idx),
                             static_cast<int>(case_numbers.size()),
                             exec_config);
            ++completed_cases;
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  All cases completed." << std::endl;
    std::cout << "========================================" << std::endl;

    if (completed_cases == 0 && failed_cases > 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
