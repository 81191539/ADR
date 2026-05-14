%% plot_centerline_cx_v2.m
% Plot c-x profiles from a user-selected y layer in dense concentration fields.
% The selected layer uses the nearest available y-grid point.

clc; clear; close all;

%% ======================== User configuration ========================
case_number = 2;

snapshot_start_index = 4;   % 1-based index in drawable dense snapshots
snapshot_count       = 8;   % number of consecutive snapshots to draw

sample_y = 0.5;             % requested y position; nearest y-grid layer is used
x_shift  = -10;             % displayed x coordinate = physical x + x_shift

save_output   = 1;          % 0 = display only, 1 = save figure
output_format = 'pdf';      % 'pdf' or 'eps'
output_name   = '';         % empty = automatic name

fig_w_cm = 16;
fig_h_cm = 12;

%% ======================== Locate files ========================
script_dir = fileparts(mfilename('fullpath'));
if isempty(script_dir)
    script_dir = pwd;
end

output_root = fullfile(script_dir, 'output');
dense_dir = fullfile(output_root, sprintf('data_%d_dense', case_number));
times_file = fullfile(dense_dir, 'times.m');

param_file = fullfile(output_root, sprintf('remarks_%d.m', case_number));
if ~isfile(param_file)
    param_file = fullfile(script_dir, sprintf('remarks_%d.m', case_number));
end

assert(isfile(param_file), ...
    'Parameter file not found for case %d. Expected output/remarks_%d.m or remarks_%d.m.', ...
    case_number, case_number, case_number);
assert(isfile(times_file), ...
    'Dense times file not found: %s. Run the solver with dense output first.', ...
    times_file);

%% ======================== Load case parameters ========================
run(param_file);

required = {'xright', 'yright', 'nx', 'ny', 'Pe', 'Pe2'};
for k = 1:numel(required)
    assert(exist(required{k}, 'var') == 1, ...
        'Parameter file is missing variable "%s".', required{k});
end

assert(isscalar(sample_y) && isfinite(sample_y), ...
       'sample_y must be a finite scalar.');
assert(sample_y >= 0 && sample_y <= yright, ...
       'sample_y (%.12g) must be within the physical domain [0, %.12g].', ...
       sample_y, yright);

%% ======================== Load dense time index ========================
times_data = load(times_file);
if isvector(times_data)
    times_data = reshape(times_data, 1, []);
end

assert(~isempty(times_data), 'Dense times file is empty: %s.', times_file);
assert(size(times_data, 2) >= 3, ...
    'Dense times file must have at least 3 columns: [dense_index, iteration, sim_time].');

dense_ids        = round(times_data(:, 1));
dense_iterations = round(times_data(:, 2));
dense_times      = times_data(:, 3);

valid = isfinite(dense_ids) & isfinite(dense_iterations) & isfinite(dense_times);
dense_ids        = dense_ids(valid);
dense_iterations = dense_iterations(valid);
dense_times      = dense_times(valid);

assert(~isempty(dense_ids), 'Dense times file has no valid rows: %s.', times_file);

available_ids = [];
available_iterations = [];
available_times = [];

fprintf('Dense snapshots for case %d:\n', case_number);
for k = 1:numel(dense_ids)
    dense_id = dense_ids(k);
    cc_file = fullfile(dense_dir, sprintf('cc_%d.m', dense_id));

    if ~isfile(cc_file)
        warning('Missing dense concentration file, skipping: %s', cc_file);
        continue;
    end

    available_ids(end + 1, 1) = dense_id; %#ok<SAGROW>
    available_iterations(end + 1, 1) = dense_iterations(k); %#ok<SAGROW>
    available_times(end + 1, 1) = dense_times(k); %#ok<SAGROW>

    fprintf('  dense %d: iteration %d, t* = %.12g\n', ...
        dense_id, dense_iterations(k), dense_times(k));
end

assert(~isempty(available_ids), ...
    'No drawable dense concentration snapshots were found in %s.', dense_dir);

assert(isscalar(snapshot_start_index) && isfinite(snapshot_start_index) && ...
       snapshot_start_index == round(snapshot_start_index) && snapshot_start_index >= 1, ...
       'snapshot_start_index must be a positive integer.');
assert(isscalar(snapshot_count) && isfinite(snapshot_count) && ...
       snapshot_count == round(snapshot_count) && snapshot_count >= 1, ...
       'snapshot_count must be a positive integer.');

total_available = numel(available_ids);
snapshot_end_index = snapshot_start_index + snapshot_count - 1;
assert(snapshot_start_index <= total_available, ...
    ['snapshot_start_index (%d) exceeds the number of drawable dense snapshots (%d). ' ...
     'Use a start index from 1 to %d.'], ...
    snapshot_start_index, total_available, total_available);
assert(snapshot_end_index <= total_available, ...
    ['Requested snapshots %d:%d, but only %d drawable dense snapshots are available. ' ...
     'Reduce snapshot_count or choose an earlier snapshot_start_index.'], ...
    snapshot_start_index, snapshot_end_index, total_available);

selected_range = snapshot_start_index:snapshot_end_index;
selected_ids = available_ids(selected_range);
selected_iterations = available_iterations(selected_range);
selected_times = available_times(selected_range);

%% ======================== Extract y-layer profiles ========================
x_vec = linspace(0, xright, nx + 1);
y_vec = linspace(0, yright, ny + 1);
x_plot = x_vec + x_shift;

[~, y_idx] = min(abs(y_vec - sample_y));
actual_y = y_vec(y_idx);

profiles = nan(numel(x_vec), snapshot_count);
reference_cc = [];

fprintf('\nExtracting profiles at requested y = %.12g, nearest grid y = %.12g (index %d, %d snapshots)...\n', ...
    sample_y, actual_y, y_idx, snapshot_count);

for r = 1:snapshot_count
    dense_id = selected_ids(r);
    cc_file = fullfile(dense_dir, sprintf('cc_%d.m', dense_id));
    cc_data = load(cc_file);

    assert(size(cc_data, 1) == nx + 1 && size(cc_data, 2) == ny + 1, ...
        'Dense %d: cc_data size is [%d x %d], expected [%d x %d].', ...
        dense_id, size(cc_data, 1), size(cc_data, 2), nx + 1, ny + 1);

    if r == 1
        reference_cc = cc_data;
    end

    profiles(:, r) = cc_data(:, y_idx);

    fprintf('  profile %d/%d: dense %d, iteration %d, t* = %.12g\n', ...
        r, snapshot_count, dense_id, selected_iterations(r), selected_times(r));
end

%% ======================== Plot field and y-layer profiles ========================
fig = figure('Units', 'centimeters', ...
             'Position', [2, 2, fig_w_cm, fig_h_cm], ...
             'PaperUnits', 'centimeters', ...
             'PaperSize', [fig_w_cm, fig_h_cm], ...
             'PaperPosition', [0, 0, fig_w_cm, fig_h_cm], ...
             'PaperPositionMode', 'auto', ...
             'Color', 'w');
set(fig, 'DefaultAxesFontName', 'Times New Roman', ...
         'DefaultAxesFontSize', 10, ...
         'DefaultTextFontName', 'Times New Roman', ...
         'DefaultTextFontSize', 10);

layout = tiledlayout(fig, 2, 1, 'TileSpacing', 'compact', 'Padding', 'compact');

ax_field = nexttile(layout, 1);
imagesc(ax_field, [x_plot(1), x_plot(end)], [0, yright], reference_cc.');
set(ax_field, 'YDir', 'normal');
colormap(ax_field, jet);
clim(ax_field, [0, 1]);
hold(ax_field, 'on');
plot(ax_field, [x_plot(1), x_plot(end)], [actual_y, actual_y], ...
     'k-', 'LineWidth', 2.4);
plot(ax_field, [x_plot(1), x_plot(end)], [actual_y, actual_y], ...
     'w-', 'LineWidth', 1.2);
hold(ax_field, 'off');
axis(ax_field, 'tight');
box(ax_field, 'off');
ylabel(ax_field, '{\ity}', 'Interpreter', 'tex');
title(ax_field, sprintf('Reference concentration field, {\\itt}* = %.6g', ...
      selected_times(1)), 'Interpreter', 'tex', 'FontWeight', 'normal');
cb = colorbar(ax_field);
cb.Label.String = '{\itc}*';
cb.Label.Interpreter = 'tex';

ax_profile = nexttile(layout, 2);
colors = lines(snapshot_count);
line_styles = {'-', '--', ':', '-.'};
hold(ax_profile, 'on');
for r = 1:snapshot_count
    style_now = line_styles{mod(r - 1, numel(line_styles)) + 1};
    plot(ax_profile, x_plot, profiles(:, r), ...
         'LineStyle', style_now, ...
         'Color', colors(r, :), ...
         'LineWidth', 1.4, ...
         'DisplayName', sprintf('{\\itt}* = %.6g', selected_times(r)));
end
hold(ax_profile, 'off');
grid(ax_profile, 'on');
box(ax_profile, 'off');
xlim(ax_profile, [x_plot(1), x_plot(end)]);
ylim(ax_profile, [0, 1]);
xlabel(ax_profile, '{\itx}', 'Interpreter', 'tex');
ylabel(ax_profile, sprintf('{\\itc}* at {\\ity} = %.6g', actual_y), ...
       'Interpreter', 'tex');
legend(ax_profile, 'Location', 'eastoutside', 'Interpreter', 'tex', 'Box', 'off');
title(ax_profile, sprintf('Profiles at requested {\\ity} = %.6g, grid {\\ity} = %.6g', ...
      sample_y, actual_y), 'Interpreter', 'tex', 'FontWeight', 'normal');

drawnow;

%% ======================== Save output ========================
if save_output
    if isempty(output_name)
        output_name = sprintf('cx_y%g_case%d_Pe1_%g_Pe2_%g_snapshots%d-%d.%s', ...
                              actual_y, case_number, Pe, Pe2, ...
                              snapshot_start_index, snapshot_end_index, ...
                              output_format);
    end

    output_path = fullfile(script_dir, output_name);
    try
        exportgraphics(fig, output_path, 'ContentType', 'vector', ...
                       'Resolution', 300);
    catch
        switch output_format
            case 'pdf'
                print(fig, output_path, '-dpdf', '-bestfit');
            case 'eps'
                print(fig, output_path, '-depsc', '-vector');
            otherwise
                error('Unsupported output_format: %s', output_format);
        end
    end
    fprintf('\nY-layer profile figure saved: %s\n', output_path);
end

fprintf('Done.\n');
