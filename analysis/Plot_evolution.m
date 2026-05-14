%% plot_evolution.m
%  全自动可视化脚本 —— 从 remarks_<case>.m 读取所有参数，自动检测帧数，
%  自动标注 Pe, Pe2, λ, 时刻 t，支持可选视频输出。
%
%  数据来源约定：
%    remarks_<case>.m   → 运行后产生所有参数变量（Pe, Pe2, lam, endT, total_count, ...）
%    eta_ave_<case>.m   → 密集时间序列 [time, eta_ave, d(eta)/dt]，由求解器逐步输出
%    data_<case>/cc_N.m → 第 N 帧浓度场快照
%    data_<case>/ee_N.m → 第 N 帧表面覆盖率剖面 [x, eta(x)]
%
%  帧编号 N = 1 ~ total_count，输出间隔 dt_output = endT / total_count。
%  若提前收敛，后面的帧文件不存在，脚本会自动检测并只处理已有帧。
%
% =========================================================================

clc; clear; close all;

%% ======================== 用户可配置区 ========================
case_number   = 1;          % 算例编号
Include_movie = 0;          % 0 = 仅绘图, 1 = 同时输出 MPEG-4 视频
video_quality   = 95;
video_framerate = 5;

% 帧选择（留空 [] 表示使用全部可用帧）
frame_range = [];           % 例如 [1, 300] 或 [10, 50]，留空自动检测
frame_step  = 10;            % 每隔多少帧画一张（1 = 每帧都画）

% 矢量图输出（保存到 figure_<case_number>/ 文件夹）
save_figures  = 1;          % 0 = 不保存, 1 = 保存为矢量图
fig_format    = 'pdf';      % 'pdf' 或 'eps'（两者均为矢量格式）

%% ======================== 加载算例参数 ========================
param_file = sprintf('remarks_%d.m', case_number);
assert(isfile(param_file), '参数文件 "%s" 不存在。', param_file);
run(param_file);

% 校验关键变量（remarks 文件运行后应在工作空间中产生这些变量）
required = {'xright','yright','nx','ny','Pe','Pe2','lam','xleft', ...
            'endT','total_count','dt_initial','xpo_l','xpo_r'};
for k = 1:numel(required)
    assert(exist(required{k}, 'var') == 1, ...
        '参数文件缺少变量 "%s"。', required{k});
end

%% ======================== 构造网格 ========================
x = linspace(0, xright, nx+1);
y = linspace(0, yright, ny+1);
[y2, x2] = ndgrid(x, y);   % 用于 mesh() 绘制浓度场

%% ======================== 自动检测可用帧范围 ========================
data_dir = sprintf('data_%d', case_number);
assert(isfolder(data_dir), '数据目录 "%s" 不存在。', data_dir);

% 扫描目录，找出所有 cc_N.m 文件的编号
cc_files = dir(fullfile(data_dir, 'cc_*.m'));
frame_ids_all = zeros(numel(cc_files), 1);
for k = 1:numel(cc_files)
    tokens = regexp(cc_files(k).name, '^cc_(\d+)\.m$', 'tokens');
    if ~isempty(tokens)
        frame_ids_all(k) = str2double(tokens{1}{1});
    end
end
frame_ids_all = sort(frame_ids_all(frame_ids_all > 0));

if isempty(frame_ids_all)
    error('在 "%s" 中没有找到任何 cc_N.m 文件。', data_dir);
end

N_max = max(frame_ids_all);
fprintf('检测到 %d 个浓度场文件，帧编号范围 [%d, %d]。\n', ...
        numel(frame_ids_all), min(frame_ids_all), N_max);

% 应用用户的帧选择
if isempty(frame_range)
    BN = min(frame_ids_all);
    NN = N_max;
else
    BN = frame_range(1);
    NN = min(frame_range(2), N_max);
end
frames_to_plot = BN:frame_step:NN;

% 进一步过滤：只保留实际存在的帧
frames_to_plot = intersect(frames_to_plot, frame_ids_all);
fprintf('将绘制 %d 帧（从 %d 到 %d，步长 %d）。\n', ...
        numel(frames_to_plot), frames_to_plot(1), frames_to_plot(end), frame_step);

%% ======================== 计算每帧对应的物理时间 ========================
% 求解器每隔 dt_output = endT / total_count 输出一帧
% 帧 N 对应的近似物理时间为 t_N = N * dt_output
dt_output = endT / total_count;

%% ======================== 加载密集时间序列 ========================
eta_file = sprintf('eta_ave_%d.m', case_number);
assert(isfile(eta_file), '时间序列文件 "%s" 不存在。', eta_file);
eta_raw = dlmread(eta_file);

time_full     = eta_raw(:, 1);   % 完整的密集时间轴
eta_ave_full  = eta_raw(:, 2);   % 空间平均 eta
deta_dt_full  = eta_raw(:, 3);   % d(eta)/dt

fprintf('已加载 "%s"：%d 行，时间范围 [%g, %g]。\n', ...
        eta_file, length(time_full), time_full(1), time_full(end));

%% ======================== 几何装饰参数 ========================
% 浓度场图中的底部和顶部装饰带，用于标识吸附区域
% 三段：左侧惰性区 | 中间吸附区（绿色）| 右侧惰性区
margin = 0.1;   % 装饰带超出域边界的余量
rx1 = [xleft - margin,       xpo_l];          % 左惰性区
rx2 = [xpo_l,                xpo_r];          % 吸附区
rx3 = [xpo_r,                xright + margin]; % 右惰性区
ry_bot = [-0.20, -0.01];     % 底部装饰带 y 范围
ry_top = [yright+0.01, yright+0.20];  % 顶部装饰带 y 范围

color_inert  = [0.7, 0.7, 0.7];
color_adsorb = [0.0, 0.4, 0.0];
lw_border    = 1.25;

% 绘制矩形的辅助函数
draw_rect = @(ax, rx, ry, fc) ...
    patch(ax, [rx(1) rx(1) rx(2) rx(2)], ...
              [ry(1) ry(2) ry(2) ry(1)], fc, ...
              'EdgeColor', 'k', 'LineWidth', lw_border);

%% ======================== 时间序列图的坐标轴范围 ========================
% 从 remarks 文件中获取理论平衡值，用于设定 y 轴上限
if exist('eta_eq', 'var')
    eta_ylim = [0, eta_eq * 1.2];   % 留 20% 余量
else
    eta_ylim = [0, 0.6];
end
time_xlim = [0, endT];

% deta/dt 的 y 轴范围：从数据自动估算（支持负值）
deta_valid = deta_dt_full(~isinf(deta_dt_full) & ~isnan(deta_dt_full));
if isempty(deta_valid)
    deta_ylim = [-0.06, 0.06];
else
    deta_lo = min(deta_valid);
    deta_hi = max(deta_valid);
    deta_margin = max(abs([deta_lo, deta_hi])) * 0.15;  % 上下各留 15% 余量
    if deta_margin == 0, deta_margin = 0.01; end         % 防止全零数据
    deta_ylim = [deta_lo - deta_margin, deta_hi + deta_margin];
end

%% ======================== 准备视频输出 ========================
vid_writer = [];
if Include_movie
    vid_name = sprintf('evolution_case%d_Pe%g_lam%g', case_number, Pe, lam);
    try
        vid_writer = VideoWriter(vid_name, 'MPEG-4');
        vid_writer.Quality   = video_quality;
        vid_writer.FrameRate = video_framerate;
        open(vid_writer);
        fprintf('视频输出 → %s.mp4\n', vid_name);
    catch ME
        warning('无法创建视频：%s\n继续运行但不录制。', ME.message);
        vid_writer = [];
    end
end

%% ======================== 准备矢量图输出文件夹 ========================
fig_dir = '';
if save_figures
    fig_dir = sprintf('figure_%d', case_number);
    if ~isfolder(fig_dir)
        mkdir(fig_dir);
    end
    fprintf('矢量图输出 → %s/ (格式: .%s)\n', fig_dir, fig_format);
end

%% ======================== 主循环 ========================
fprintf('\n开始绘制...\n');
t_start = tic;

% 在循环外创建唯一的 figure 窗口，循环内只做 clf 清空重绘，
% 这样窗口始终存在，既不闪退也不会产生几百个 figure 对象。
fig = figure('PaperSize', [20.98, 29.68], ...
             'Position', [100, 50, 900, 720], ...
             'Name', sprintf('Case %d Evolution', case_number));

for idx = 1:numel(frames_to_plot)
    it = frames_to_plot(idx);

    %% --- 当前帧的物理时间 ---
    t_now = it * dt_output;

    %% --- 加载浓度场 ---
    cc_file = fullfile(data_dir, sprintf('cc_%d.m', it));
    if ~isfile(cc_file)
        warning('帧 %d："%s" 不存在，跳过。', it, cc_file);
        continue
    end
    cc_data = load(cc_file);

    %% --- 加载 eta 剖面 ---
    ee_file = fullfile(data_dir, sprintf('ee_%d.m', it));
    if ~isfile(ee_file)
        warning('帧 %d："%s" 不存在，跳过。', it, ee_file);
        continue
    end
    ee_data = load(ee_file);

    %% --- 从密集时间序列中截取到当前时刻 ---
    mask = time_full <= t_now + dt_output * 0.5;  % 允许半个输出步长的容差
    t_slice    = time_full(mask);
    eta_slice  = eta_ave_full(mask);
    deta_slice = deta_dt_full(mask);

    %% --- 清空当前 figure 并重新绘制（复用同一个窗口） ---
    clf(fig);

    % =================== 面板 1：浓度场 c*(x, y) ===================
    ax1 = subplot(3, 2, [1 2], 'Parent', fig);
    hold(ax1, 'on');

    % 装饰带：底部三段 + 顶部通栏
    draw_rect(ax1, rx1, ry_bot, color_inert);
    draw_rect(ax1, rx2, ry_bot, color_adsorb);
    draw_rect(ax1, rx3, ry_bot, color_inert);
    draw_rect(ax1, [rx1(1), rx3(2)], ry_top, color_inert);

    % 浓度场伪彩色
    mesh(ax1, y2, x2, cc_data);
    view(ax1, 0, 90);
    colormap(ax1, jet);
    caxis(ax1, [0, 1]);

    % 标题：自动标注所有关键参数
    title_str = sprintf('$Pe = %g$,  $Pe_2 = %g$,  $\\lambda = %g$', Pe, Pe2, lam);
    title(ax1, title_str, 'Interpreter', 'latex', 'FontSize', 18);

    % 时间标注
    text(ax1, xleft - 0.8, yright + 0.55, '$t^* = $', ...
         'Interpreter', 'latex', 'FontSize', 11);
    text(ax1, xleft + 0.4, yright + 0.53, sprintf('$%.4f$', t_now), ...
         'Interpreter', 'latex', 'FontSize', 10);
    % 浓度标签
    text(ax1, xleft - 0.85, -0.9, '$c^*$', ...
         'Interpreter', 'latex', 'FontSize', 12);

    set(ax1, 'FontSize', 8);
    yticks(ax1, 0:0.5:yright);
    xticks(ax1, 0:ceil(xright/16):xright);

    ch = colorbar(ax1, 'horiz');
    set(ch, 'Position', [0.13, 0.72, 0.775, 0.025], 'FontSize', 10);
    axis(ax1, 'off');
    axis(ax1, 'image');

    % =================== 面板 2：eta(x) 剖面 ===================
    ax2 = subplot(3, 2, [3 4], 'Parent', fig);
    plot(ax2, ee_data(:,1), ee_data(:,2), 'r-', 'LineWidth', 1.5);
    set(ax2, 'FontSize', 10);
    xlabel(ax2, '$x$', 'Interpreter', 'latex', 'FontSize', 13);
    ylabel(ax2, '$\eta$', 'Interpreter', 'latex', 'FontSize', 13);
    axis(ax2, [xleft, xright, eta_ylim]);
    daspect(ax2, [(xright - xleft) / diff(eta_ylim) / 3, 1, 1]);
    grid(ax2, 'on');

    % =================== 面板 3：平均 eta 随时间 ===================
    ax3 = subplot(3, 2, 5, 'Parent', fig);
    plot(ax3, t_slice, eta_slice, 'r-', 'LineWidth', 1.5);
    hold(ax3, 'on');
    % 标记当前时刻
    if ~isempty(eta_slice)
        plot(ax3, t_slice(end), eta_slice(end), 'ko', ...
             'MarkerSize', 5, 'MarkerFaceColor', 'k');
    end
    % 如果有平衡值，画虚线参考
    if exist('eta_eq', 'var')
        yline(ax3, eta_eq, 'b--', 'LineWidth', 0.8);
    end
    set(ax3, 'FontSize', 10);
    ylabel(ax3, '$\bar{\eta}$', 'Interpreter', 'latex', 'FontSize', 13);
    xlabel(ax3, '$t^*$', 'Interpreter', 'latex', 'FontSize', 13);
    grid(ax3, 'on');
    axis(ax3, [time_xlim, eta_ylim]);

    % =================== 面板 4：d(eta)/dt 随时间 ===================
    ax4 = subplot(3, 2, 6, 'Parent', fig);
    plot(ax4, t_slice, deta_slice, 'r-', 'LineWidth', 1.5);
    hold(ax4, 'on');
    if ~isempty(deta_slice)
        plot(ax4, t_slice(end), deta_slice(end), 'ko', ...
             'MarkerSize', 5, 'MarkerFaceColor', 'k');
    end
    set(ax4, 'FontSize', 10);
    ylabel(ax4, '$\mathrm{d}\bar{\eta}/\mathrm{d}t^*$', ...
           'Interpreter', 'latex', 'FontSize', 11);
    xlabel(ax4, '$t^*$', 'Interpreter', 'latex', 'FontSize', 12);
    grid(ax4, 'on');
    axis(ax4, [time_xlim, deta_ylim]);

    % =================== 帧信息水印（调试用） ===================
    annotation(fig, 'textbox', [0.01, 0.01, 0.3, 0.03], ...
               'String', sprintf('Case %d | Frame %d/%d', case_number, it, N_max), ...
               'FontSize', 7, 'EdgeColor', 'none', 'Color', [0.5, 0.5, 0.5]);

    drawnow;

    %% --- 写入视频帧 ---
    if ~isempty(vid_writer)
        try
            frame = getframe(fig);
            writeVideo(vid_writer, frame);
        catch ME
            warning('写入第 %d 帧失败：%s', it, ME.message);
        end
    end

    %% --- 保存矢量图 ---
    if save_figures
        fig_path = fullfile(fig_dir, sprintf('frame_%04d.%s', it, fig_format));
        try
            exportgraphics(fig, fig_path, 'ContentType', 'image'); %vector
        catch
            % MATLAB R2019b 以前没有 exportgraphics，退回到 print
            switch fig_format
                case 'pdf'
                    print(fig, fig_path, '-dpdf', '-bestfit');
                case 'eps'
                    print(fig, fig_path, '-depsc', '-painters');
                otherwise
                    print(fig, fig_path, '-dpdf', '-bestfit');
            end
        end
    end

    %% --- 进度报告 ---
    if mod(idx, 50) == 0 || idx == numel(frames_to_plot)
        elapsed = toc(t_start);
        fprintf('  已完成 %d/%d 帧（%.1f 秒）\n', ...
                idx, numel(frames_to_plot), elapsed);
    end
end

%% ======================== 收尾 ========================
if ~isempty(vid_writer)
    close(vid_writer);
    fprintf('视频已保存。\n');
end

fprintf('\n全部完成。共处理 %d 帧，耗时 %.1f 秒。\n', ...
        numel(frames_to_plot), toc(t_start));