%=============================================================
%  DIFFUSION-CONVECTION SIMULATION LOG
%  Case Number: 2
%  Generated:   2025-12-22 16:40:04
%=============================================================

% -------------------- INPUT PARAMETERS --------------------
case_number = 2;

% Geometry and mesh
lam = 0.033333;        % aspect ratio (H/L)
ny  = 50;         % grid points in y-direction
nx  = 1500;         % grid points in x-direction (computed)
h   = 2.000000e-02;   % grid spacing

% Computational domain
xleft  = 0;
xright = 30;
yleft  = 0;
yright = 1;
domain_length = 30;  % L = xright - xleft
domain_height = 1;  % H = yright - yleft

% Flow parameters
Pe    = 1000;      % Peclet number (steady flow)
Pe2   = 1000;      % Peclet number (oscillatory flow)
alpha = 0.01;      % Womersley number
Sc    = 16667;      % Schmidt number

% Reaction parameters
Da  = 100;        % Damkohler number
K0  = 1;        % equilibrium constant
eps = 0.1;        % surface capacity parameter
c0  = 1;        % inlet concentration

% Adsorption zone (dimensionless, then physical)
xpo_l_rel = 0.33333;  % relative position (0-1)
xpo_r_rel = 0.66667;  % relative position (0-1)
xpo_l = 9.9999;      % physical left boundary
xpo_r = 20.0001;      % physical right boundary
adsorption_length = 10.0002;  % length of adsorption zone

% Time integration
endT        = 60;    % target end time
coeff_dt    = 0.0353553;    % dt coefficient (dt = coeff_dt * h^2)
dt_initial  = 4.000000e-05;    % initial time step
total_count = 10;      % planned output count

% Initial condition
x_ini_posi = 5;  % initial concentration front position

% -------------------- THEORETICAL VALUES --------------------
eta_eq = 0.5;     % equilibrium coverage = K0/(1+K0)
u_max_estimate = 1250;  % estimated maximum velocity

% CFL stability estimates
dt_diffusion_limit  = 1.000000e-04;  % h^2/4 (2D explicit)
dt_convection_limit = 1.600000e-05;  % h/u_max
dt_cfl_recommended  = 6.400000e-06;  % 0.4 * min(dt_diff, dt_conv)

% -------------------- MEMORY ESTIMATE --------------------
total_grid_cells = 79659;  % including ghost cells
memory_per_matrix_MB = 0.608;
memory_per_vector_MB = 0.011467;
memory_total_estimate_MB = 1.881;  % 3 matrices + 5 vectors

% -------------------- RUNTIME EVENTS --------------------
nan_events = 3;  % number of NaN occurrences
resumed_from_checkpoint = 0;  % 1=yes, 0=no

% dt adjustment history (iteration, old_dt, new_dt, sim_time)
dt_adjustments = [
         252, 4.000000e-05, 2.828427e-05, 1.008000e-02;  % iter, old, new, time
         258, 2.828427e-05, 2.000000e-05, 7.297342e-03;  % iter, old, new, time
         277, 2.000000e-05, 1.414214e-05, 5.540000e-03;  % iter, old, new, time
];

% -------------------- CONVERGENCE HISTORY --------------------
% Key convergence milestones (iteration, time, eta, rel_error)
convergence_milestones = [
        1000, 1.414214e-02, 0.0004682734671, 9.990635e-01;
        8000, 1.131371e-01, 0.05328376354, 8.934325e-01;
       15000, 2.121320e-01, 0.1038608784, 7.922782e-01;
       24000, 3.394113e-01, 0.1593917188, 6.812166e-01;
       36000, 5.091169e-01, 0.2122787167, 5.754426e-01;
       57000, 8.061017e-01, 0.2631095583, 4.737809e-01;
       75000, 1.060660e+00, 0.3150225089, 3.699550e-01;
      106000, 1.499066e+00, 0.3661154237, 2.677692e-01;
      127000, 1.796051e+00, 0.4169427937, 1.661144e-01;
      152000, 2.149605e+00, 0.4587583852, 8.248323e-02;
      194000, 2.743574e+00, 0.4794795376, 4.104092e-02;
      245000, 3.464823e+00, 0.4897769925, 2.044601e-02;
      269000, 3.804234e+00, 0.4949864947, 1.002701e-02;
      292000, 4.129504e+00, 0.4975352622, 4.929476e-03;
      337000, 4.765900e+00, 0.4987695651, 2.460870e-03;
      384000, 5.430580e+00, 0.4993875597, 1.224881e-03;
];
convergence_milestones_headers = {'iteration', 'sim_time', 'eta_ave', 'rel_error'};

% -------------------- PERFORMANCE STATISTICS --------------------
actual_iterations = 391000;
output_file_count = 1;

% Timing breakdown (seconds)
time_initialization = 0.001;
time_computation    = 119.399;
time_io             = 0.000;
time_total          = 119.400;

time_per_iteration_ms = 0.3054;
iterations_per_second = 3274.7;

% -------------------- FINAL RESULTS --------------------
converged = 1;  % 1=yes, 0=no (reached max_it)
final_sim_time = 5.529575029;
final_eta_ave  = 0.499502877;
final_rel_error = 9.942461e-04;

% Convergence achieved!
% eta_ave reached within 0.1% of eta_eq = 0.5

% -------------------- OUTPUT FILES --------------------
% Data files are in: data_2/
%   cc_N.m  - concentration field at output N
%   ee_N.m  - surface coverage (eta) at output N
% 
% Time series: eta_ave_2.m
%   Format: [time, eta_average, d(eta)/dt]

%=============================================================
%  END OF LOG
%=============================================================
