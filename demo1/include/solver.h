/**
 * @file solver.h
 * @brief CPU-side numerical kernels for the diffusion-convection solver.
 */

#pragma once

#include "types.h"

void initialization(SimFields& fields,
                    double x_ini_posi,
                    const GridInfo& grid);

void full_step_explicit(SimFields& fields,
                        const GridInfo& grid,
                        const PhysicsParams& phys,
                        const AdsorptionZone& zone,
                        double ct, double dt);

void calc_eta(const Field1D& ee, Field1D& ne,
              const Field2D& cc, double dt,
              long nx, double h,
              double xpo_l, double xpo_r,
              double eps, double Da, double K0);

void augment_eta(Field1D& ee, long nx);

void augment_phi(Field2D& cc, const Field1D& ee,
                 long nx, long ny, double h,
                 double Da, double K0,
                 double xpo_l, double xpo_r,
                 double c0);

void update_eta_and_phi_boundaries(Field1D& ee, Field1D& ne,
                                   Field2D& cc,
                                   const GridInfo& grid,
                                   const PhysicsParams& phys,
                                   const AdsorptionZone& zone,
                                   double dt);

void oscillatory(double alpha, double Sc,
                 Field1D& ff, const Field1D& yy,
                 double ct, long ny);

void advection_c(const Field2D& oc, Field2D& adv_c,
                 const Field1D& yy, const Field1D& ff,
                 long nx, long ny, double h,
                 double Pe, double Pe2);

void calc_phi(const Field2D& cc, Field2D& nc,
              const Field2D& adv_c, double dt,
              long nx, long ny, double h);

void calc_phi_with_advection(const Field2D& cc, Field2D& nc,
                             const Field1D& yy, const Field1D& ff,
                             double dt, long nx, long ny, double h,
                             double Pe, double Pe2);

bool has_unstable_values(const Field2D& field, long nx, long ny);

double compute_eta_average(const Field1D& eta,
                           const GridInfo& grid,
                           const AdsorptionZone& zone);

void relax(Field2D& p, const Field2D& oc, const Field2D& f,
           double dt, long nx, long ny,
           int p_relax, double lam, double h);
