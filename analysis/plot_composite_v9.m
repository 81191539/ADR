%% plot_composite.m
%  选取若干固定时刻的浓度场快照，紧凑地纵向排列为一张时序组合图。
%  直接从原始数据重新渲染（非图片拼接），输出为单个矢量图文件。
%
% =========================================================================

clc; clear; close all;

%% ======================== 用户可配置区 ========================
case_number = 7;

% ★ 你想展示哪些时刻（物理时间 t*），脚本自动匹配最近的可用帧
%target_times = [0, 4, 8, 12, 16, 20, 24]; 
%target_times = [0.0000269260, 0.0000538520, 0.0000807780, 0.0001077040, 0.0001346300, 0.0001615560, 0.0001884820]; 
target_times = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7]; 
%target_times = [0.2, 0.4, 0.6, 0.8, 1, 1.2, 1.4]; 
%target_times = [1, 2, 3, 4, 5, 6, 7];

% 显示选项
show_colorbar = 1;          % 0 = 无色标, 1 = 底部显示共享色标
x_shift       = -10;        % x 轴标签偏移（显示坐标 = 物理坐标 + x_shift）

% 输出设置
save_output   = 1;            % 0 = 仅屏幕显示, 1 = 同时保存文件
output_format = 'pdf';        % 'pdf' 或 'eps'
output_name   = '';           % 留空则自动命名
fig_w_cm      = 8;           % 子图内容区宽度 (cm)
fig_h_cm      = 6;            % 子图内容区高度 (cm)  ← 只控制浓度场区域

%% ======================== 加载算例参数 ========================
param_file = sprintf('remarks_%d.m', case_number);
assert(isfile(param_file), '参数文件 "%s" 不存在。', param_file);
run(param_file);

required = {'xright','yright','nx','ny','Pe','Pe2','lam','xleft', ...
            'endT','total_count','xpo_l','xpo_r'};
for k = 1:numel(required)
    assert(exist(required{k}, 'var') == 1, ...
        '参数文件缺少变量 "%s"。', required{k});
end

%% ======================== 扫描可用帧并匹配目标时刻 ========================
data_dir  = sprintf('data_%d', case_number);
dt_output = endT / total_count;

cc_files = dir(fullfile(data_dir, 'cc_*.m'));
frame_ids = zeros(numel(cc_files), 1);
for k = 1:numel(cc_files)
    tokens = regexp(cc_files(k).name, '^cc_(\d+)\.m$', 'tokens');
    if ~isempty(tokens)
        frame_ids(k) = str2double(tokens{1}{1});
    end
end
frame_ids = sort(frame_ids(frame_ids >= 0));
assert(~isempty(frame_ids), '在 "%s" 中没有找到 cc_*.m 文件。', data_dir);

frame_times = frame_ids * dt_output;

% 匹配目标时刻 → 最近可用帧
n_rows = numel(target_times);
matched_frames = zeros(n_rows, 1);
matched_times  = zeros(n_rows, 1);

fprintf('时刻匹配：\n');
for k = 1:n_rows
    [min_dist, idx] = min(abs(frame_times - target_times(k)));
    matched_frames(k) = frame_ids(idx);
    matched_times(k)  = frame_times(idx);
    fprintf('  请求 t* = %-8g → 帧 %d（实际 t* = %g）\n', ...
            target_times(k), matched_frames(k), matched_times(k));
    % 偏差超过半个输出步长时警告
    if min_dist > dt_output * 0.5
        warning('帧匹配偏差较大：请求 t*=%g，最近帧 t*=%g，差 %.3g。', ...
                target_times(k), matched_times(k), min_dist);
    end
end

% 去重
[matched_frames, unique_idx] = unique(matched_frames, 'stable');
matched_times = matched_times(unique_idx);
n_rows = numel(matched_frames);

%% ======================== 几何装饰参数 ========================
margin = 0.1;
rx1 = [xleft - margin,       xpo_l];
rx2 = [xpo_l,                xpo_r];
rx3 = [xpo_r,                xright + margin];
ry_bot = [-0.20, -0.01];
ry_top = [yright+0.01, yright+0.20];

color_inert  = [0.7, 0.7, 0.7];
color_adsorb = [0.55, 0.27, 0.07];

draw_rect = @(ax, rx, ry, fc) ...
    patch(ax, [rx(1) rx(1) rx(2) rx(2)], ...
              [ry(1) ry(2) ry(2) ry(1)], fc, ...
              'EdgeColor', 'none');

%% ======================== 创建组合 figure ========================
% 画布 = 内容区 + 四周附加边距（cm）
% 底部区域从下到上依次为：色标条 → c* 标签 → x 标签 → x 刻度数字 → 内容区
label_margin_cm = 1.5;                             % 左侧时间标签
top_margin_cm   = 0.1;                             % 顶部余量
right_margin_cm = 0.5;

% 底部各层高度（cm），从内容区底边往下数
xtick_h_cm  = 0.4;                                % x 刻度数字
xlabel_h_cm = 0.5;                                % "x" 字母
if show_colorbar
    clabel_h_cm = 0.4;                            % "c*" 字母
    cbar_h_cm   = 0.4;                             % 色标条本身
    cbar_tick_cm = 0.4;                            % 色标刻度数字
    cbar_pad_cm = 0.2;                             % 底部留白
else
    clabel_h_cm = 0;
    cbar_h_cm   = 0;
    cbar_tick_cm = 0;
    cbar_pad_cm = 0.15;
end
bot_margin_cm = xtick_h_cm + xlabel_h_cm + clabel_h_cm + cbar_h_cm + cbar_tick_cm + cbar_pad_cm;

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

% 内容区在画布中的归一化坐标
content_left_n = label_margin_cm / total_w_cm;
content_bot_n  = bot_margin_cm / total_h_cm;
content_w_n    = fig_w_cm / total_w_cm;
content_h_n    = fig_h_cm / total_h_cm;

%% ======================== 逐行绘制 ========================
fprintf('\n开始绘制组合图（%d 行）...\n', n_rows);

row_h_n = content_h_n / n_rows;

for r = 1:n_rows
    it = matched_frames(r);
    t_now = matched_times(r);

    row_bot = content_bot_n + content_h_n - r * row_h_n;

    %% --- 左侧时间标签 ---
    if t_now == 0
        label_str = '{\itt}* = 0';
    else
        label_str = sprintf('%g', t_now);
    end
    annotation(fig, 'textbox', ...
               [0.0, row_bot, content_left_n - 0.005, row_h_n], ...
               'String', label_str, ...
               'Interpreter', 'tex', 'FontName', 'Times New Roman', 'FontSize', 10, ...
               'EdgeColor', 'none', ...
               'HorizontalAlignment', 'right', ...
               'VerticalAlignment', 'middle');

    %% --- 加载浓度场 ---
    cc_file = fullfile(data_dir, sprintf('cc_%d.m', it));
    if ~isfile(cc_file)
        warning('帧 %d："%s" 不存在，跳过。', it, cc_file);
        continue
    end
    cc_data = load(cc_file);

    % 数据维度校验（防止求解器输出转置导致静默画错）
    assert(size(cc_data, 1) == nx+1 && size(cc_data, 2) == ny+1, ...
        '帧 %d：cc_data 尺寸 [%d×%d]，预期 [%d×%d]。', ...
        it, size(cc_data,1), size(cc_data,2), nx+1, ny+1);

    %% --- 绘制浓度场 ---
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

% —— 新增：叠加等高线 ——
x_vec = linspace(0, xright, nx+1);
y_vec = linspace(0, yright, ny+1);
levels = 0.1:0.1:0.9;            % 想画哪些等值线
[~, hC] = contour(ax, x_vec, y_vec, cc_data.', levels, ...
                  'LineColor', 'k', ...        % jet 上黑线清楚；也可用 'w'
                  'LineWidth', 0.4);
% 如需标数值： clabel(C, hC, 'FontSize', 7, 'Color', 'k');
uistack(hC, 'top');              % 保证在 imagesc 之上

    % 最后一行显示 x 刻度（不用 xlabel，手动放置所有标注避免冲突）
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

    fprintf('  行 %d/%d：帧 %d, t* = %g\n', r, n_rows, it, t_now);

    % 记录最后一行的 axes 位置，用于放置底部色标
    last_row_pos = get(ax, 'Position');
end

%% ======================== 底部标注（从内容区底边向下逐层放置） ========================
% 各层的归一化 y 坐标，从内容区底边向下计算
y_content_bot = content_bot_n;                                   % 内容区底边
y_xlabel      = y_content_bot - (xtick_h_cm + xlabel_h_cm) / total_h_cm;  % "x" 字母
y_clabel      = y_xlabel - clabel_h_cm / total_h_cm;             % "c*" 字母
y_cbar        = y_clabel - cbar_h_cm / total_h_cm;               % 色标条
y_cbar_tick   = y_cbar - cbar_tick_cm / total_h_cm;              % 色标刻度数字

% "x" 标签（居中于内容区宽度）
annotation(fig, 'textbox', ...
           [content_left_n, y_xlabel, content_w_n, xlabel_h_cm / total_h_cm], ...
           'String', '{\itx}', 'Interpreter', 'tex', ...
           'FontName', 'Times New Roman', 'FontSize', 10, ...
           'EdgeColor', 'none', ...
           'HorizontalAlignment', 'center', 'VerticalAlignment', 'top');

if show_colorbar
    % "c*" 标签
    annotation(fig, 'textbox', ...
               [content_left_n, y_clabel, content_w_n, clabel_h_cm / total_h_cm], ...
               'String', '{\itc}*', 'Interpreter', 'tex', ...
               'FontName', 'Times New Roman', 'FontSize', 10, ...
               'EdgeColor', 'none', ...
               'HorizontalAlignment', 'center', 'VerticalAlignment', 'top');

    % 色标条
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

%% ======================== 保存输出 ========================
if save_output
    if isempty(output_name)
        output_name = sprintf('composite_case%d_Pe1_%g_Pe2_%g_line.%s', ...
                              case_number, Pe, Pe2, output_format);
    end
    try
        exportgraphics(fig, output_name, 'ContentType', 'vector', ...
                       'Resolution', 300);
    catch
        switch output_format
            case 'pdf'
                print(fig, output_name, '-dpdf', '-bestfit');
            case 'eps'
                print(fig, output_name, '-depsc', '-painters');
        end
    end
    fprintf('\n组合图已保存 → %s\n', output_name);
end

fprintf('完成。\n');