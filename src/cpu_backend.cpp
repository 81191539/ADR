/*****************************************************************************
 * cpu_backend.cpp
 *
 * OpenMP-backed CPU implementation of the solver backend interface.
 *****************************************************************************/

#include "backend.h"
#include "solver.h"

#include <memory>
#include <utility>

namespace {

class CpuSolverBackend final : public SolverBackend {
public:
    const char* name() const override
    {
        return "CPU/OpenMP";
    }

    ComputeBackend kind() const override
    {
        return ComputeBackend::Cpu;
    }

    void initialize(SimFields& fields,
                    const GridInfo& grid,
                    double x_ini_posi) override
    {
        initialization(fields, x_ini_posi, grid);
    }

    void full_step_explicit(SimFields& fields,
                            const GridInfo& grid,
                            const PhysicsParams& phys,
                            const AdsorptionZone& zone,
                            double ct, double dt) override
    {
        ::full_step_explicit(fields, grid, phys, zone, ct, dt);
        std::swap(fields.cc, fields.nc);
        std::swap(fields.ee, fields.ne);
    }

    bool has_unstable_values(SimFields& fields, const GridInfo& grid) override
    {
        return ::has_unstable_values(fields.cc, grid.nx, grid.ny) ||
               ::has_unstable_eta(fields.ee, grid.nx);
    }

    double compute_eta_average(SimFields& fields,
                               const GridInfo& grid,
                               const AdsorptionZone& zone) override
    {
        return ::compute_eta_average(fields.ee, grid, zone);
    }

    void zero_state(SimFields& fields, const GridInfo&) override
    {
        fields.zero_all();
    }

    void sync_host(SimFields&) override
    {
    }

    void sync_device(SimFields&) override
    {
    }
};

}  // namespace

std::unique_ptr<SolverBackend> create_cpu_backend()
{
    return std::make_unique<CpuSolverBackend>();
}
