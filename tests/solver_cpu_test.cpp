#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "solver.h"

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

    REQUIRE_FALSE(has_nan(fields.nc, grid.nx, grid.ny));
    REQUIRE(compute_eta_average(fields.ne, grid, zone) >= 0.0);
}
