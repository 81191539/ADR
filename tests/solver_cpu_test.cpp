#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "backend.h"
#include "solver.h"
#include "config.h"
#include "io.h"
#include "runtime.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <string>
#include <vector>

namespace {

struct ConfigDirsGuard {
    std::string input_dir{config::INPUT_DIR};
    std::string output_dir{config::OUTPUT_DIR};
    std::string checkpoint_dir{config::CHECKPOINT_DIR};

    ~ConfigDirsGuard()
    {
        config::INPUT_DIR = input_dir;
        config::OUTPUT_DIR = output_dir;
        config::CHECKPOINT_DIR = checkpoint_dir;
    }
};

std::vector<char*> make_argv(std::vector<std::string>& args)
{
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args) {
        argv.push_back(arg.data());
    }
    return argv;
}

std::filesystem::path make_test_temp_dir(const std::string& stem)
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      (stem + "_" + std::to_string(suffix));
    std::filesystem::create_directories(path);
    return path;
}

void cleanup_test_temp_dir(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

void require_all_numbers_finite(const std::filesystem::path& path)
{
    std::ifstream in(path);
    REQUIRE(in.good());

    double value = 0.0;
    std::size_t count = 0;
    while (in >> value) {
        REQUIRE(std::isfinite(value));
        ++count;
    }
    REQUIRE(count > 0);
}

void require_eta_file_physical(const std::filesystem::path& path)
{
    std::ifstream in(path);
    REQUIRE(in.good());

    double x = 0.0;
    double eta = 0.0;
    std::size_t count = 0;
    while (in >> x >> eta) {
        REQUIRE(std::isfinite(x));
        REQUIRE(std::isfinite(eta));
        REQUIRE(eta >= -1e-10);
        REQUIRE(eta <= 1.0 + 1e-10);
        ++count;
    }
    REQUIRE(count > 0);
}

std::vector<std::filesystem::path> child_directories(const std::filesystem::path& dir)
{
    std::vector<std::filesystem::path> children;
    if (!std::filesystem::exists(dir)) {
        return children;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_directory()) {
            children.push_back(entry.path());
        }
    }
    std::sort(children.begin(), children.end());
    return children;
}

}  // namespace

TEST_CASE("Field storage supports ghost-cell indexing", "[fields]")
{
    SimFields fields;
    fields.resize(4, 3);
    fields.zero_all();

    fields.cc(-1, -1) = 1.25;
    fields.cc(4, 3) = 2.50;
    fields.ee(-1) = 3.75;
    fields.ee(4) = 4.50;

    REQUIRE(fields.cc(-1, -1) == Approx(1.25));
    REQUIRE(fields.cc(4, 3) == Approx(2.50));
    REQUIRE(fields.ee(-1) == Approx(3.75));
    REQUIRE(fields.ee(4) == Approx(4.50));
}

TEST_CASE("CPU explicit step keeps the state finite", "[solver][cpu]")
{
    GridInfo grid;
    grid.nx = 8;
    grid.ny = 4;
    grid.h = 0.25;
    grid.xleft = 0.0;
    grid.xright = 2.0;
    grid.yleft = 0.0;
    grid.yright = 1.0;

    PhysicsParams phys;
    phys.lam = 0.5;
    phys.Pe = 10.0;
    phys.Pe2 = 10.0;
    phys.eps = 0.1;
    phys.Da = 5.0;
    phys.K0 = 1.0;
    phys.c0 = 1.0;
    phys.alpha = 0.01;
    phys.Sc = sim::SC_DEFAULT;

    AdsorptionZone zone;
    zone.xpo_l = 0.5;
    zone.xpo_r = 1.5;

    SimFields fields;
    fields.resize(grid.nx, grid.ny);
    fields.zero_all();

    initialization(fields, 0.5, grid);
    full_step_explicit(fields, grid, phys, zone, 0.01, 1e-3);

    REQUIRE_FALSE(has_unstable_values(fields.nc, grid.nx, grid.ny));
    REQUIRE(compute_eta_average(fields.ne, grid, zone) >= 0.0);
}

TEST_CASE("CPU backend exposes swapped next state as current state", "[solver][cpu]")
{
    GridInfo grid;
    grid.nx = 8;
    grid.ny = 4;
    grid.h = 0.25;
    grid.xleft = 0.0;
    grid.xright = 2.0;
    grid.yleft = 0.0;
    grid.yright = 1.0;

    PhysicsParams phys;
    phys.lam = 0.5;
    phys.Pe = 10.0;
    phys.Pe2 = 10.0;
    phys.eps = 0.1;
    phys.Da = 5.0;
    phys.K0 = 1.0;
    phys.c0 = 1.0;
    phys.alpha = 0.01;
    phys.Sc = sim::SC_DEFAULT;

    AdsorptionZone zone;
    zone.xpo_l = 0.5;
    zone.xpo_r = 1.5;

    SimFields direct_fields;
    direct_fields.resize(grid.nx, grid.ny);
    direct_fields.zero_all();
    initialization(direct_fields, 0.5, grid);
    full_step_explicit(direct_fields, grid, phys, zone, 0.01, 1e-3);

    SimFields backend_fields;
    backend_fields.resize(grid.nx, grid.ny);
    backend_fields.zero_all();
    initialization(backend_fields, 0.5, grid);

    auto backend = create_cpu_backend();
    backend->full_step_explicit(backend_fields, grid, phys, zone, 0.01, 1e-3);

    for (long i = 0; i <= grid.nx; ++i) {
        REQUIRE(backend_fields.ee(i) == Approx(direct_fields.ne(i)));
        for (long j = 0; j <= grid.ny; ++j) {
            REQUIRE(backend_fields.cc(i, j) == Approx(direct_fields.nc(i, j)));
        }
    }
    REQUIRE_FALSE(backend->has_unstable_values(backend_fields, grid));
    REQUIRE(backend->compute_eta_average(backend_fields, grid, zone) ==
            Approx(compute_eta_average(direct_fields.ne, grid, zone)));
}

TEST_CASE("CPU stability check rejects non-physical concentration values", "[solver][cpu]")
{
    Field2D field;
    field.resize(0, 2, 0, 2);
    field.fill(0.5);

    REQUIRE_FALSE(has_unstable_values(field, 2, 2));

    field(1, 1) = std::numeric_limits<double>::infinity();
    REQUIRE(has_unstable_values(field, 2, 2));

    field(1, 1) = -1e-6;
    REQUIRE(has_unstable_values(field, 2, 2));

    field(1, 1) = 1e13;
    REQUIRE(has_unstable_values(field, 2, 2));
}

TEST_CASE("CPU stability check scans eta over the x dimension", "[solver][cpu]")
{
    GridInfo grid;
    grid.nx = 8;
    grid.ny = 2;

    SimFields fields;
    fields.resize(grid.nx, grid.ny);
    fields.zero_all();
    fields.cc.fill(0.5);
    fields.ee.fill(0.5);

    REQUIRE_FALSE(has_unstable_eta(fields.ee, grid.nx));

    fields.ee(grid.nx) = 1.25;
    REQUIRE(has_unstable_eta(fields.ee, grid.nx));

    auto backend = create_cpu_backend();
    REQUIRE(backend->has_unstable_values(fields, grid));
}

TEST_CASE("Parameter reader prefers TOML over legacy input", "[io]")
{
    const auto old_input_dir = config::INPUT_DIR;
    const auto old_output_dir = config::OUTPUT_DIR;

    const auto temp_dir = make_test_temp_dir("adr_solver_tests_input");

    {
        std::ofstream legacy(temp_dir / "input_parameter_0042.txt");
        legacy << "1 0.5 10 10 0.1 5 1 4 0.25 0.75 1 2 0.1 0.5 0.01\n";
    }
    {
        std::ofstream toml(temp_dir / "input_parameter_0042.toml");
        toml << "lam = 0.25\n"
             << "Pe = 11\n"
             << "Pe2 = 12\n"
             << "eps = 0.2\n"
             << "Da = 6\n"
             << "K0 = 2\n"
             << "ny = 8\n"
             << "xpo_l = 0.2\n"
             << "xpo_r = 0.8\n"
             << "endT = 2\n"
             << "total_count = 3\n"
             << "coeff_dt = 0.2\n"
             << "x_ini_posi = 0.6\n"
             << "alpha = 0.02\n"
             << "Sc = 12345\n"
             << "\n"
             << "[runtime]\n"
             << "enable_dense_dump = false\n";
    }

    config::INPUT_DIR = temp_dir.string();
    const Params params = read_parameter(42);
    config::INPUT_DIR = old_input_dir;
    config::OUTPUT_DIR = old_output_dir;
    cleanup_test_temp_dir(temp_dir);

    REQUIRE(params.lam == Approx(0.25));
    REQUIRE(params.Pe == Approx(11.0));
    REQUIRE(params.ny == 8);
    REQUIRE(params.total_count == 3);
    REQUIRE(params.Sc == Approx(12345.0));
}

TEST_CASE("Optional TOML runtime config applies unless CLI overrides it", "[io][runtime]")
{
    ConfigDirsGuard guard;

    const auto temp_dir = make_test_temp_dir("adr_solver_tests_runtime_toml");

    {
        std::ofstream toml(temp_dir / "input_parameter_0043.toml");
        toml << "lam = 0.25\n"
             << "Pe = 11\n"
             << "Pe2 = 12\n"
             << "eps = 0.2\n"
             << "Da = 6\n"
             << "K0 = 2\n"
             << "ny = 8\n"
             << "xpo_l = 0.2\n"
             << "xpo_r = 0.8\n"
             << "endT = 2\n"
             << "total_count = 3\n"
             << "coeff_dt = 0.2\n"
             << "x_ini_posi = 0.6\n"
             << "alpha = 0.02\n"
             << "\n"
             << "[runtime]\n"
             << "stats_interval = 7\n"
             << "stability_check_interval = 5\n"
             << "checkpoint_interval = 11\n"
             << "enable_dense_dump = false\n"
             << "dense_dump_start = 0\n"
             << "dense_dump_count = 2\n"
             << "convergence_threshold = 0\n"
             << "output_matlab = true\n"
             << "output_tecplot = true\n";
    }

    config::INPUT_DIR = temp_dir.string();

    ExecutionConfig toml_config;
    apply_runtime_config_from_case(43, toml_config);
    REQUIRE(toml_config.stats_interval == 7);
    REQUIRE(toml_config.stability_check_interval == 5);
    REQUIRE(toml_config.checkpoint_interval == 11);
    REQUIRE_FALSE(toml_config.enable_dense_dump);
    REQUIRE(toml_config.dense_dump_start == Approx(0.0));
    REQUIRE(toml_config.dense_dump_count == 2);
    REQUIRE(toml_config.convergence_threshold == Approx(0.0));
    REQUIRE(toml_config.output_matlab);
    REQUIRE(toml_config.output_tecplot);

    ExecutionConfig cli_config;
    cli_config.stats_interval = 99;
    cli_config.runtime_overrides.stats_interval = true;
    cli_config.enable_dense_dump = true;
    cli_config.runtime_overrides.enable_dense_dump = true;
    apply_runtime_config_from_case(43, cli_config);
    REQUIRE(cli_config.stats_interval == 99);
    REQUIRE(cli_config.stability_check_interval == 5);
    REQUIRE(cli_config.enable_dense_dump);

    cleanup_test_temp_dir(temp_dir);
}

TEST_CASE("Runtime rejects invalid interval CLI values before reading cases", "[runtime][cli]")
{
    std::vector<std::string> args = {
        "adr_solver_tests",
        "--case", "777",
        "--stats-interval", "0",
    };
    std::vector<char*> argv = make_argv(args);

    ExecutionConfig exec_config;
    REQUIRE(run_cases_with_args(exec_config,
                                static_cast<int>(argv.size()),
                                argv.data()) != 0);
}

TEST_CASE("Runtime runs a tiny TOML case and writes finite outputs", "[runtime][e2e]")
{
    ConfigDirsGuard guard;

    const auto root = make_test_temp_dir("adr_solver_tests_runtime_e2e");
    const auto input_dir = root / "input";
    const auto output_dir = root / "output";
    const auto checkpoint_dir = root / "checkpoint";

    std::filesystem::create_directories(input_dir);
    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(checkpoint_dir);

    {
        std::ofstream toml(input_dir / "input_parameter_0777.toml");
        toml << "lam = 0.5\n"
             << "Pe = 1\n"
             << "Pe2 = 0.5\n"
             << "eps = 0.1\n"
             << "Da = 1\n"
             << "K0 = 1\n"
             << "ny = 4\n"
             << "xpo_l = 0.25\n"
             << "xpo_r = 0.75\n"
             << "endT = 0.00625\n"
             << "total_count = 2\n"
             << "coeff_dt = 0.01\n"
             << "x_ini_posi = 0.4\n"
             << "alpha = 1\n";
    }

    config::INPUT_DIR = input_dir.string();
    config::OUTPUT_DIR = output_dir.string();
    config::CHECKPOINT_DIR = checkpoint_dir.string();

    std::vector<std::string> args = {
        "adr_solver_tests",
        "--case", "777",
        "--force-restart",
        "--stats-interval", "1",
        "--stability-check-interval", "1",
        "--checkpoint-interval", "10000",
        "--disable-dense-dump",
        "--convergence-threshold", "0",
    };
    std::vector<char*> argv = make_argv(args);

    ExecutionConfig exec_config;
    REQUIRE(run_cases_with_args(exec_config,
                                static_cast<int>(argv.size()),
                                argv.data()) == 0);

    const auto data_dir = output_dir / "data_777";
    REQUIRE(std::filesystem::exists(output_dir / "eta_ave_777.m"));
    REQUIRE(std::filesystem::exists(output_dir / "remarks_777.m"));
    REQUIRE(std::filesystem::exists(data_dir / "cc_0.m"));
    REQUIRE(std::filesystem::exists(data_dir / "ee_0.m"));
    REQUIRE(std::filesystem::exists(data_dir / "cc_2.m"));
    REQUIRE_FALSE(std::filesystem::exists(output_dir / "data_777_dense"));

    require_all_numbers_finite(output_dir / "eta_ave_777.m");
    require_all_numbers_finite(data_dir / "cc_0.m");
    require_all_numbers_finite(data_dir / "cc_2.m");
    require_eta_file_physical(data_dir / "ee_0.m");
    require_eta_file_physical(data_dir / "ee_2.m");

    cleanup_test_temp_dir(root);
}

TEST_CASE("Dense dump writes unique run directories with descriptive filenames", "[runtime][dense]")
{
    ConfigDirsGuard guard;

    const auto root = make_test_temp_dir("adr_solver_tests_dense_runs");
    const auto input_dir = root / "input";
    const auto output_dir = root / "output";
    const auto checkpoint_dir = root / "checkpoint";

    std::filesystem::create_directories(input_dir);
    std::filesystem::create_directories(output_dir);
    std::filesystem::create_directories(checkpoint_dir);

    {
        std::ofstream toml(input_dir / "input_parameter_0778.toml");
        toml << "lam = 0.5\n"
             << "Pe = 1\n"
             << "Pe2 = 0.5\n"
             << "eps = 0.1\n"
             << "Da = 1\n"
             << "K0 = 1\n"
             << "ny = 4\n"
             << "xpo_l = 0.25\n"
             << "xpo_r = 0.75\n"
             << "endT = 0.00625\n"
             << "total_count = 2\n"
             << "coeff_dt = 0.01\n"
             << "x_ini_posi = 0.4\n"
             << "alpha = 1\n";
    }

    config::INPUT_DIR = input_dir.string();
    config::OUTPUT_DIR = output_dir.string();
    config::CHECKPOINT_DIR = checkpoint_dir.string();

    std::vector<std::string> args = {
        "adr_solver_tests",
        "--case", "778",
        "--force-restart",
        "--stats-interval", "1",
        "--stability-check-interval", "1",
        "--checkpoint-interval", "10000",
        "--dense-dump-start", "0",
        "--dense-dump-count", "1",
        "--convergence-threshold", "0",
    };

    for (int run = 0; run < 2; ++run) {
        std::vector<char*> argv = make_argv(args);
        ExecutionConfig exec_config;
        REQUIRE(run_cases_with_args(exec_config,
                                    static_cast<int>(argv.size()),
                                    argv.data()) == 0);
    }

    const auto dense_root = output_dir / "data_778_dense";
    const auto runs = child_directories(dense_root);
    REQUIRE(runs.size() == 2);
    REQUIRE(runs[0].filename().string().rfind("run_", 0) == 0);
    REQUIRE(runs[1].filename().string().rfind("run_", 0) == 0);
    REQUIRE(runs[0] != runs[1]);

    const std::regex cc_name(R"(^cc_0_t[-+0-9.eE]+_it0\.m$)");
    const std::regex ee_name(R"(^ee_0_t[-+0-9.eE]+_it0\.m$)");
    for (const auto& run_dir : runs) {
        std::vector<std::string> names;
        for (const auto& entry : std::filesystem::directory_iterator(run_dir)) {
            if (entry.is_regular_file()) {
                names.push_back(entry.path().filename().string());
            }
        }
        std::sort(names.begin(), names.end());
        REQUIRE(std::find_if(names.begin(), names.end(), [&](const std::string& name) {
                    return std::regex_match(name, cc_name);
                }) != names.end());
        REQUIRE(std::find_if(names.begin(), names.end(), [&](const std::string& name) {
                    return std::regex_match(name, ee_name);
                }) != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "times.m") != names.end());

        std::ifstream times(run_dir / "times.m");
        REQUIRE(times.good());
        std::string line;
        std::getline(times, line);
        REQUIRE(line.find("cc_0_t") != std::string::npos);
        REQUIRE(line.find("ee_0_t") != std::string::npos);
    }

    cleanup_test_temp_dir(root);
}
