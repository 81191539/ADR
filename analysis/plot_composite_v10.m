%% plot_composite_v10.m
% Dense-output composite plot for concentration fields.
% Reads output/data_<case>_dense/times.m and cc_<dense_index>.m directly.

clc; clear; close all;

%% ======================== User configuration ========================
case_number = 2;

show_colorbar = 1;          % 0 = no colorbar, 1 = shared bottom colorbar
x_shift       = -10;        % displayed x coordinate = physical x + x_shift

save_output   = 1;          % 0 = display only, 1 = save figure
output_format = 'pdf';      % 'pdf' or 'eps'
output_name   = '';         % empty = automatic name
fig_w_cm      = 8;          % content width in cm
fig_h_cm      = 6;          % content height in cm

contour_levels = 0.1:0.1:0.9;

snapshot_start_index = 4;   % 1-based index in drawable dense snapshots
snapshot_count       = 8;  % number of snapshots to draw

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

required = {'xright', 'yright', 'nx', 'ny', 'Pe', 'Pe2', 'xleft', ...
            'xpo_l', 'xpo_r'};
for k = 1:numel(required)
    assert(exist(required{k}, 'var') == 1, ...
        'Parameter file is missing variable "%s".', required{k});
end

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
available_ids         = available_ids(selected_range);
available_iterations = available_iterations(selected_range);
available_times      = available_times(selected_range);

if snapshot_start_index == 1 && snapshot_count == total_available
    selection_suffix = '';
else
    selection_suffix = sprintf('_snapshots%d-%d', ...
                               snapshot_start_index, snapshot_end_index);
end

n_rows = numel(available_ids);

%% ======================== Geometry decoration ========================
margin = 0.1;
rx1 = [xleft - margin, xpo_l];
rx2 = [xpo_l,          xpo_r];
rx3 = [xpo_r,          xright + margin];
ry_bot = [-0.20, -0.01];
ry_top = [yright + 0.01, yright + 0.20];

color_inert  = [0.7, 0.7, 0.7];
color_adsorb = [0.55, 0.27, 0.07];

draw_rect = @(ax, rx, ry, fc) ...
    patch(ax, [rx(1) rx(1) rx(2) rx(2)], ...
              [ry(1) ry(2) ry(2) ry(1)], fc, ...
              'EdgeColor', 'none');

%% ======================== Create composite figure ========================
label_margin_cm = 1.5;
top_margin_cm   = 0.1;
right_margin_cm = 0.5;

xtick_h_cm  = 0.4;
xlabel_h_cm = 0.5;
if show_colorbar
    clabel_h_cm  = 0.4;
    cbar_h_cm    = 0.4;
    cbar_tick_cm = 0.4;
    cbar_pad_cm  = 0.2;
else
    clabel_h_cm  = 0;
    cbar_h_cm    = 0;
    cbar_tick_cm = 0;
    cbar_pad_cm  = 0.15;
end
bot_margin_cm = xtick_h_cm + xlabel_h_cm + clabel_h_cm + ...
                cbar_h_cm + cbar_tick_cm + cbar_pad_cm;

total_w_cm = label_margin_cm + fig_w_cm + right_margin_cm;
total_h_cm = top_margin_cm + fig_h_cm + bot_margin_cm;

fig = figure('Units', 'centimeters', ...
             'Position', [2, 2, total_w_cm, total_h_cm], ...
             'PaperUnits', 'centimeters', ...
             'PaperSize', [total_w_cm, total_h_cm], ...
             'PaperPosition', [0, 0, total_w_cm, total_h_cm], ...
             'PaperPositionMode', 'auto', ...
             'Renderer', 'painters', ...
             'Colormap', jet, ...
             'Color', 'w');
set(fig, 'DefaultAxesFontName', 'Times New Roman', ...
         'DefaultAxesFontSize', 10, ...
         'DefaultTextFontName', 'Times New Roman', ...
         'DefaultTextFontSize', 10);

content_left_n = label_margin_cm / total_w_cm;
content_bot_n  = bot_margin_cm / total_h_cm;
content_w_n    = fig_w_cm / total_w_cm;
content_h_n    = fig_h_cm / total_h_cm;
row_h_n        = content_h_n / n_rows;

x_vec = linspace(0, xright, nx + 1);
y_vec = linspace(0, yright, ny + 1);
last_row_pos = [];

%% ======================== Draw rows ========================
fprintf('\nDrawing dense composite (%d rows)...\n', n_rows);

for r = 1:n_rows
    dense_id = available_ids(r);
    t_now = available_times(r);
    iteration = available_iterations(r);
    row_bot = content_bot_n + content_h_n - r * row_h_n;

    if abs(t_now) < eps
        label_str = '{\itt}* = 0';
    else
        label_str = sprintf('{\\itt}* = %.6g', t_now);
    end
    annotation(fig, 'textbox', ...
               [0.0, row_bot, content_left_n - 0.005, row_h_n], ...
               'String', label_str, ...
               'Interpreter', 'tex', 'FontName', 'Times New Roman', 'FontSize', 10, ...
               'EdgeColor', 'none', ...
               'HorizontalAlignment', 'right', ...
               'VerticalAlignment', 'middle');

    cc_file = fullfile(dense_dir, sprintf('cc_%d.m', dense_id));
    cc_data = load(cc_file);

    assert(size(cc_data, 1) == nx + 1 && size(cc_data, 2) == ny + 1, ...
        'Dense %d: cc_data size is [%d x %d], expected [%d x %d].', ...
        dense_id, size(cc_data, 1), size(cc_data, 2), nx + 1, ny + 1);

    ax = axes('Parent', fig, ...
              'Position', [content_left_n, row_bot, content_w_n, row_h_n]);
    hold(ax, 'on');

    draw_rect(ax, rx1, ry_bot, color_inert);
    draw_rect(ax, rx2, ry_bot, color_adsorb);
    draw_rect(ax, rx3, ry_bot, color_inert);
    draw_rect(ax, [rx1(1), rx3(2)], ry_top, color_inert);

    imagesc(ax, [0, xright], [0, yright], cc_data.');
    set(ax, 'YDir', 'normal');
    caxis(ax, [0, 1]);

    [~, hC] = contour(ax, x_vec, y_vec, cc_data.', contour_levels, ...
                      'LineColor', 'k', ...
                      'LineWidth', 0.4);
    uistack(hC, 'top');

    if r == n_rows
        tick_step = 5;
        tick_phys = (xleft:tick_step:xright);
        tick_labels = arrayfun(@(v) sprintf('%g', v + x_shift), tick_phys, ...
                               'UniformOutput', false);
        set(ax, 'XAxisLocation', 'bottom', 'YTick', [], ...
                'XTick', tick_phys, 'XTickLabel', tick_labels, ...
                'FontName', 'Times New Roman', 'FontSize', 10, ...
                'TickDir', 'out', 'Box', 'off');
        axis(ax, 'image');
    else
        axis(ax, 'off');
        axis(ax, 'image');
    end

    fprintf('  row %d/%d: dense %d, iteration %d, t* = %.12g\n', ...
        r, n_rows, dense_id, iteration, t_now);

    last_row_pos = get(ax, 'Position');
end

%% ======================== Bottom labels and colorbar ========================
y_content_bot = content_bot_n;
y_xlabel      = y_content_bot - (xtick_h_cm + xlabel_h_cm) / total_h_cm;
y_clabel      = y_xlabel - clabel_h_cm / total_h_cm;
y_cbar        = y_clabel - cbar_h_cm / total_h_cm;

annotation(fig, 'textbox', ...
           [content_left_n, y_xlabel, content_w_n, xlabel_h_cm / total_h_cm], ...
           'String', '{\itx}', 'Interpreter', 'tex', ...
           'FontName', 'Times New Roman', 'FontSize', 10, ...
           'EdgeColor', 'none', ...
           'HorizontalAlignment', 'center', 'VerticalAlignment', 'top');

if show_colorbar
    annotation(fig, 'textbox', ...
               [content_left_n, y_clabel, content_w_n, clabel_h_cm / total_h_cm], ...
               'String', '{\itc}*', 'Interpreter', 'tex', ...
               'FontName', 'Times New Roman', 'FontSize', 10, ...
               'EdgeColor', 'none', ...
               'HorizontalAlignment', 'center', 'VerticalAlignment', 'top');

    ax_cb = axes('Parent', fig, 'Position', last_row_pos, 'Visible', 'off');
    caxis(ax_cb, [0, 1]);
    cb = colorbar(ax_cb, 'horiz');
    cb_w = content_w_n * 0.8;
    cb_left = content_left_n + (content_w_n - cb_w) / 2;
    cb_h_n  = cbar_h_cm / total_h_cm * 0.6;
    set(cb, 'Position', [cb_left, y_cbar, cb_w, cb_h_n], ...
            'FontName', 'Times New Roman', 'FontSize', 10, ...
            'Ticks', [0, 0.25, 0.5, 0.75, 1], ...
            'Box', 'off', 'TickLength', 0.02);
end

drawnow;

%% ======================== Save output ========================
if save_output
    if isempty(output_name)
        output_name = sprintf('dense_composite_case%d_Pe1_%g_Pe2_%g%s.%s', ...
                              case_number, Pe, Pe2, selection_suffix, output_format);
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
                print(fig, output_path, '-depsc', '-painters');
            otherwise
                error('Unsupported output_format: %s', output_format);
        end
    end
    fprintf('\nDense composite figure saved: %s\n', output_path);
end

fprintf('Done.\n');
