/*****************************************************************************
 * solver.cpp
 *
 * CPU numerical kernels operating on backend-neutral field storage.
 *****************************************************************************/

#include "solver.h"

#include <cmath>

#include <omp.h>

//-----------------------------------------------------------------------------
// Initialize coordinates and the concentration front.
//-----------------------------------------------------------------------------
void initialization(SimFields& fields,
                    double x_ini_posi,
                    const GridInfo& grid)
{
    for (long i = fields.xx.lower_bound(); i <= fields.xx.upper_bound(); ++i) {
        fields.xx(i) = grid.xleft + static_cast<double>(i) * grid.h;
    }

    for (long j = fields.yy.lower_bound(); j <= fields.yy.upper_bound(); ++j) {
        fields.yy(j) = grid.yleft + static_cast<double>(j) * grid.h;
    }

    const double dis = 2.0 * grid.h / (2.0 * phys::SQRT2 * phys::ATANH_0_9);

    #pragma omp parallel for collapse(2) schedule(static)
    for (long i = fields.cc.x_lower_bound(); i <= fields.cc.x_upper_bound(); ++i) {
        for (long j = fields.cc.y_lower_bound(); j <= fields.cc.y_upper_bound(); ++j) {
            const double x = grid.xleft + static_cast<double>(i) * grid.h;
            fields.cc(i, j) =
                0.5 * (-std::tanh((x - x_ini_posi) / (phys::SQRT2 * dis)) + 1.0);
        }
    }
}

//-----------------------------------------------------------------------------
// Full explicit time step.
//-----------------------------------------------------------------------------
void full_step_explicit(SimFields& fields,
                        const GridInfo& grid,
                        const PhysicsParams& phys,
                        const AdsorptionZone& zone,
                        double ct, double dt)
{
    calc_eta(fields.ee, fields.ne, fields.cc, dt,
             grid.nx, grid.h, zone.xpo_l, zone.xpo_r,
             phys.eps, phys.Da, phys.K0);

    augment_phi(fields.cc, fields.ee,
                grid.nx, grid.ny, grid.h,
                phys.Da, phys.K0, zone.xpo_l, zone.xpo_r, phys.c0);

    oscillatory(phys.alpha, phys.Sc, fields.ff, fields.yy, ct, grid.ny);

    advection_c(fields.cc, fields.adv_c, fields.yy, fields.ff,
                grid.nx, grid.ny, grid.h, phys.Pe, phys.Pe2);

    calc_phi(fields.cc, fields.nc, fields.adv_c, dt,
             grid.nx, grid.ny, grid.h);

    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= grid.nx; ++i) {
        fields.ee(i) = fields.ne(i);
    }

    #pragma omp parallel for collapse(2) schedule(static)
    for (long i = 0; i <= grid.nx; ++i) {
        for (long j = 0; j <= grid.ny; ++j) {
            fields.cc(i, j) = fields.nc(i, j);
        }
    }
}

//-----------------------------------------------------------------------------
// Surface coverage update.
//-----------------------------------------------------------------------------
void calc_eta(const Field1D& ee, Field1D& ne,
              const Field2D& cc, double dt,
              long nx, double h,
              double xpo_l, double xpo_r,
              double eps, double Da, double K0)
{
    const double K0_inv = 1.0 / K0;
    const double coeff = eps * Da;

    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= nx; ++i) {
        const double x = static_cast<double>(i) * h;
        if (x > xpo_l && x <= xpo_r) {
            ne(i) = ee(i) + dt * coeff * (cc(i, 0) * (1.0 - ee(i)) - ee(i) * K0_inv);
        } else {
            ne(i) = 0.0;
        }
    }
}

//-----------------------------------------------------------------------------
// Extend eta ghost cells.
//-----------------------------------------------------------------------------
void augment_eta(Field1D& ee, long nx)
{
    ee(-1) = ee(0);
    ee(nx + 1) = ee(nx);
}

//-----------------------------------------------------------------------------
// Boundary conditions for concentration.
//-----------------------------------------------------------------------------
void augment_phi(Field2D& cc, const Field1D& ee,
                 long nx, long ny, double h,
                 double Da, double K0,
                 double xpo_l, double xpo_r,
                 double c0)
{
    for (long j = cc.y_lower_bound(); j <= cc.y_upper_bound(); ++j) {
        cc(-1, j) = c0;
        cc(0, j) = c0;
        cc(nx + 1, j) = cc(nx, j);
    }

    const double K0_inv = 1.0 / K0;
    const double h_Da = h * Da;

    #pragma omp parallel for schedule(static)
    for (long i = 0; i <= nx; ++i) {
        cc(i, ny + 1) = cc(i, ny);

        const double x = static_cast<double>(i) * h;
        if (x > xpo_l && x <= xpo_r) {
            cc(i, -1) = cc(i, 0) -
                        h_Da * (cc(i, 0) * (1.0 - ee(i)) - ee(i) * K0_inv);
        } else {
            cc(i, -1) = cc(i, 0);
        }
    }
}

//-----------------------------------------------------------------------------
// Oscillatory velocity profile.
//-----------------------------------------------------------------------------
void oscillatory(double alpha, double Sc,
                 Field1D& ff, const Field1D& yy,
                 double ct, long ny)
{
    const double ca = std::cos(alpha);
    const double sa = std::sin(alpha);
    const double ch = std::cosh(alpha);
    const double sh = std::sinh(alpha);
    const double c2 = std::cos(2.0 * alpha);
    const double ch2 = std::cosh(2.0 * alpha);
    const double a2 = alpha * alpha;
    const double dcc = 1.0 / (c2 + ch2);
    const double da2 = 1.0 / a2;
    const double sat = std::sin(2.0 * a2 * Sc * ct);
    const double cat = std::cos(2.0 * a2 * Sc * ct);
    const double c2_ch2_sat = (c2 + ch2) * sat;
    const double two_sa_sh = 2.0 * sa * sh;
    const double two_ca_ch = 2.0 * ca * ch;
    const double coeff = dcc * da2;

    #pragma omp parallel for schedule(static)
    for (long j = 0; j <= ny; ++j) {
        const double arg = 2.0 * alpha * (yy(j) - 0.5);
        const double say = std::sin(arg);
        const double cay = std::cos(arg);
        const double shay = std::sinh(arg);
        const double chay = std::cosh(arg);

        ff(j) = coeff * (c2_ch2_sat
               + two_sa_sh * cay * cat * chay
               - two_sa_sh * say * sat * shay
               - two_ca_ch * (cay * chay * sat + cat * say * shay));
    }
}

//-----------------------------------------------------------------------------
// Upwind advection term.
//-----------------------------------------------------------------------------
void advection_c(const Field2D& oc, Field2D& adv_c,
                 const Field1D& yy, const Field1D& ff,
                 long nx, long ny, double h,
                 double Pe, double Pe2)
{
    const double h_inv = 1.0 / h;

    #pragma omp parallel for collapse(2) schedule(static)
    for (long i = 0; i <= nx; ++i) {
        for (long j = 0; j <= ny; ++j) {
            const double du = Pe * yy(j) * (1.0 - yy(j)) + Pe2 * ff(j);
            if (du > 0.0) {
                adv_c(i, j) = du * (oc(i, j) - oc(i - 1, j)) * h_inv;
            } else {
                adv_c(i, j) = du * (oc(i + 1, j) - oc(i, j)) * h_inv;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Explicit FTCS concentration update.
//-----------------------------------------------------------------------------
void calc_phi(const Field2D& cc, Field2D& nc,
              const Field2D& adv_c, double dt,
              long nx, long ny, double h)
{
    const double h2_inv = 1.0 / (h * h);

    #pragma omp parallel for collapse(2) schedule(static)
    for (long i = 0; i <= nx; ++i) {
        for (long j = 0; j <= ny; ++j) {
            const double lap =
                ((cc(i + 1, j) - 2.0 * cc(i, j) + cc(i - 1, j)) +
                 (cc(i, j + 1) - 2.0 * cc(i, j) + cc(i, j - 1))) * h2_inv;
            const double sr = -adv_c(i, j);
            nc(i, j) = cc(i, j) + dt * (lap + sr);
        }
    }
}

//-----------------------------------------------------------------------------
// Check whether a field contains NaNs.
//-----------------------------------------------------------------------------
bool has_nan(const Field2D& field, long nx, long ny)
{
    bool found_nan = false;

    #pragma omp parallel for collapse(2) reduction(||:found_nan)
    for (long i = 0; i <= nx; ++i) {
        for (long j = 0; j <= ny; ++j) {
            if (std::isnan(field(i, j))) {
                found_nan = true;
            }
        }
    }

    return found_nan;
}

//-----------------------------------------------------------------------------
// Average eta over the adsorption zone.
//-----------------------------------------------------------------------------
double compute_eta_average(const Field1D& eta,
                           const GridInfo& grid,
                           const AdsorptionZone& zone)
{
    double eta_ave = 0.0;
    long count = 0;

    for (long i = 0; i <= grid.nx; ++i) {
        const double x = grid.xleft + static_cast<double>(i) * grid.h;
        if (x > zone.xpo_l && x < zone.xpo_r) {
            eta_ave += eta(i);
            ++count;
        }
    }

    return (count > 0) ? eta_ave / static_cast<double>(count) : 0.0;
}

//-----------------------------------------------------------------------------
// Relaxation routine retained for future implicit schemes.
//-----------------------------------------------------------------------------
void relax(Field2D& p, const Field2D& oc, const Field2D& f,
           double dt, long nx, long ny,
           int p_relax, double lam, double h)
{
    const double lam2 = lam * lam;
    const double h2 = h * h;

    for (int iter = 1; iter <= p_relax; ++iter) {
        for (long i = 0; i <= nx; ++i) {
            for (long j = 0; j <= ny; ++j) {
                const double coef = 1.0 / dt + 2.0 * (1.0 + lam2) / h2;
                const double lap =
                    (lam2 * (p(i + 1, j) + p(i - 1, j)) +
                     (p(i, j + 1) + p(i, j - 1))) / h2;
                const double src = oc(i, j) / dt - f(i, j);
                p(i, j) = (lap + src) / coef;
            }
        }
    }
}
