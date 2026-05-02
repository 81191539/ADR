#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "backend.h"
#include "solver.h"
#include "config.h"
#include "io.h"

#include <filesystem>
#include <fstream>
#include <limits>

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
    zone.i_begin = 3;
    zone.i_end = 6;

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
    zone.i_begin = 3;
    zone.i_end = 6;

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

TEST_CASE("Parameter reader prefers TOML over legacy input", "[io]")
{
    const auto old_input_dir = config::INPUT_DIR;
    const auto old_output_dir = config::OUTPUT_DIR;

    const auto temp_dir = std::filesystem::temp_directory_path() / "adr_solver_tests_input";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);

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
             << "alpha = 0.02\n";
    }

    config::INPUT_DIR = temp_dir.string();
    const Params params = read_parameter(42);
    config::INPUT_DIR = old_input_dir;
    config::OUTPUT_DIR = old_output_dir;
    std::filesystem::remove_all(temp_dir);

    REQUIRE(params.lam == Approx(0.25));
    REQUIRE(params.Pe == Approx(11.0));
    REQUIRE(params.ny == 8);
    REQUIRE(params.total_count == 3);
}
