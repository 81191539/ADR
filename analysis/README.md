# analysis

This directory contains MATLAB analysis, plotting, and result post-processing helpers.

- `show_phi_eta.m`: loads eta and remarks data, then generates plots.
- `remarks_*.m`: legacy run-summary data used directly by analysis scripts.
- `time_eta_F_data.m`: time-series data used by plotting routines.
- `plot_composite_*.m` and `Plot_evolution.m`: scripts for composite and evolution figures.
- `generate_opt.m`: historical parameter-generation or parameter-mapping script.

These files are not part of the core solver. Changes here normally affect only offline analysis and figure generation.