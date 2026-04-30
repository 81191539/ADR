%=============================================================
%  DIFFUSION-CONVECTION SIMULATION LOG
%  Case Number: 1
%  Generated:   2025-12-22 16:40:00
%=============================================================

% -------------------- INPUT PARAMETERS --------------------
case_number = 1;

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
Pe    = 100;      % Peclet number (steady flow)
Pe2   = 100;      % Peclet number (oscillatory flow)
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
coeff_dt    = 0.1;    % dt coefficient (dt = coeff_dt * h^2)
dt_initial  = 4.000000e-05;    % initial time step
total_count = 10;      % planned output count

% Initial condition
x_ini_posi = 5;  % initial concentration front position

% -------------------- THEORETICAL VALUES --------------------
eta_eq = 0.5;     % equilibrium coverage = K0/(1+K0)
u_max_estimate = 125;  % estimated maximum velocity

% CFL stability estimates
dt_diffusion_limit  = 1.000000e-04;  % h^2/4 (2D explicit)
dt_convection_limit = 1.600000e-04;  % h/u_max
dt_cfl_recommended  = 4.000000e-05;  % 0.4 * min(dt_diff, dt_conv)

% -------------------- MEMORY ESTIMATE --------------------
total_grid_cells = 79659;  % including ghost cells
memory_per_matrix_MB = 0.608;
memory_per_vector_MB = 0.011467;
memory_total_estimate_MB = 1.881;  % 3 matrices + 5 vectors

% -------------------- RUNTIME EVENTS --------------------
nan_events = 0;  % number of NaN occurrences
resumed_from_checkpoint = 0;  % 1=yes, 0=no

% No dt adjustments were needed (stable throughout)

% -------------------- CONVERGENCE HISTORY --------------------
% Key convergence milestones (iteration, time, eta, rel_error)
convergence_milestones = [
        1000, 4.000000e-02, 2.477781816e-08, 1.000000e+00;
        8000, 3.200000e-01, 0.05025514716, 8.994897e-01;
       17000, 6.800000e-01, 0.1019189939, 7.961620e-01;
       36000, 1.440000e+00, 0.1520136023, 6.959728e-01;
       48000, 1.920000e+00, 0.2043522598, 5.912955e-01;
       56000, 2.240000e+00, 0.2587943693, 4.824113e-01;
       75000, 3.000000e+00, 0.3091122728, 3.817755e-01;
       97000, 3.880000e+00, 0.3632460797, 2.735078e-01;
      124000, 4.960000e+00, 0.4140275355, 1.719449e-01;
      156000, 6.240000e+00, 0.4575206268, 8.495875e-02;
      197000, 7.880000e+00, 0.4794618569, 4.107629e-02;
      239000, 9.560000e+00, 0.4898573497, 2.028530e-02;
      281000, 1.124000e+01, 0.494954669, 1.009066e-02;
      311000, 1.244000e+01, 0.4975019348, 4.996130e-03;
      341000, 1.364000e+01, 0.4987579785, 2.484043e-03;
      382000, 1.528000e+01, 0.4993800072, 1.239986e-03;
];
convergence_milestones_headers = {'iteration', 'sim_time', 'eta_ave', 'rel_error'};

% -------------------- PERFORMANCE STATISTICS --------------------
actual_iterations = 389000;
output_file_count = 3;

% Timing breakdown (seconds)
time_initialization = 0.001;
time_computation    = 116.016;
time_io             = 0.100;
time_total          = 116.117;

time_per_iteration_ms = 0.2982;
iterations_per_second = 3353.0;

% -------------------- FINAL RESULTS --------------------
converged = 1;  % 1=yes, 0=no (reached max_it)
final_sim_time = 15.56;
final_eta_ave  = 0.4995136173;
final_rel_error = 9.727655e-04;

% Convergence achieved!
% eta_ave reached within 0.1% of eta_eq = 0.5

% -------------------- OUTPUT FILES --------------------
% Data files are in: data_1/
%   cc_N.m  - concentration field at output N
%   ee_N.m  - surface coverage (eta) at output N
% 
% Time series: eta_ave_1.m
%   Format: [time, eta_average, d(eta)/dt]

%=============================================================
%  END OF LOG
%=============================================================
