%% ========================================================
%  数据生成脚本（优化版）
%  功能: 自动生成参数组合并写入 Excel
%  扫描参数: a, Pe1, Pe2（直接扫描，彻底移除 f）
%% ========================================================
clear; clc;

%% ========== 1. 集中参数配置（唯一修改入口）==========
cfg = struct();

% 扫描参数范围（直接定义 Pe2 范围，不再经由 f 推导）
cfg.a_range   = [0.001,  10,    41];   % [min, max, 点数] 对数均匀
cfg.Pe1_range = [1000,   1000,     1];   % Pe1 固定为 10
cfg.Pe2_range = [1e-1,  1e8,  91];   % Pe2 直接扫描，对数均匀

% 固定物理参数
cfg.lamda   = 0.033333;
cfg.epsilon = 0.1;
cfg.Da      = 100;
cfg.K0      = 1;
cfg.ny      = 50;
cfg.x_pol   = 0.333333;
cfg.x_por   = 0.666667;
cfg.N       = 10;
cfg.x_pool  = 5;

% 输出设置
cfg.output_file     = 'generated_extra01_data.xlsx';
cfg.coeff_max       = 0.1;
cfg.dt_limit_denom  = 20 * 16667;

%% ========== 2. 生成参数向量 ==========
a_vec   = logspace(log10(cfg.a_range(1)),   log10(cfg.a_range(2)),   cfg.a_range(3));
Pe1_vec = logspace(log10(cfg.Pe1_range(1)), log10(cfg.Pe1_range(2)), cfg.Pe1_range(3));
Pe2_vec = logspace(log10(cfg.Pe2_range(1)), log10(cfg.Pe2_range(2)), cfg.Pe2_range(3));

%% ========== 3. 向量化生成全组合 ==========
[A, P1, P2] = meshgrid(a_vec, Pe1_vec, Pe2_vec);
a   = A(:);
Pe1 = P1(:);
Pe2 = P2(:);

numRows = numel(a);
ID = (1:numRows)';

%% ========== 4. 固定参数列 ==========
ones_col = ones(numRows, 1);
lamda   = ones_col * cfg.lamda;
epsilon = ones_col * cfg.epsilon;
Da      = ones_col * cfg.Da;
K0      = ones_col * cfg.K0;
ny      = ones_col * cfg.ny;
x_pol   = ones_col * cfg.x_pol;
x_por   = ones_col * cfg.x_por;
N       = ones_col * cfg.N;
x_pool  = ones_col * cfg.x_pool;

%% ========== 5. 派生量计算 ==========
endT = 4 * cfg.N * 0.5 ./ epsilon ./ (cfg.epsilon * Pe1).^(1/3);

h             = 1 ./ ny;
dt_limit      = pi ./ (cfg.dt_limit_denom .* a.^2);
coeff_dynamic = dt_limit ./ h.^2;
coeff         = min(cfg.coeff_max, coeff_dynamic);

%% ========== 6. 数据合并与精度控制 ==========
% f 列已彻底移除
data = round([ID, lamda, Pe1, Pe2, epsilon, Da, K0, ...
              ny, x_pol, x_por, endT, N, coeff, x_pool, a], 6);

%% ========== 7. 数据校验 ==========
validate_data(data, Pe2, endT, coeff, cfg);

%% ========== 8. 写入 Excel ==========
% 列名中无 f
columnNames = {'ID','lamda','Pe1','Pe2','epsilon', ...
               'Da','K0','ny','x_pol','x_por', ...
               'endT','N','coeff','x_pool','a'};

write_excel(cfg.output_file, columnNames, data);

%% ========== 9. 汇总统计输出 ==========
print_summary(data, columnNames, numRows, cfg);


%% ===================== 局部函数 =====================

function validate_data(data, Pe2, endT, coeff, cfg)
    warnings = {};

    if any(Pe2 <= 0)
        warnings{end+1} = sprintf('⚠ Pe2 含 %d 个非正值', sum(Pe2 <= 0));
    end
    if any(endT <= 0)
        warnings{end+1} = sprintf('⚠ endT 含 %d 个非正值', sum(endT <= 0));
    end

    coeff_clipped_ratio = mean(coeff == cfg.coeff_max);
    if coeff_clipped_ratio > 0.9
        warnings{end+1} = sprintf('⚠ %.1f%% 的 coeff 被截断至上限 %.4f，建议检查 dt_limit 参数', ...
            coeff_clipped_ratio*100, cfg.coeff_max);
    end
    if any(~isfinite(data(:)))
        warnings{end+1} = sprintf('⚠ 数据含 %d 个 NaN/Inf，请检查参数范围', sum(~isfinite(data(:))));
    end

    if isempty(warnings)
        fprintf('✅ 数据校验通过，无异常\n');
    else
        for i = 1:numel(warnings)
            fprintf('%s\n', warnings{i});
        end
    end
end


function write_excel(filename, colNames, data)
    try
        writecell([colNames; num2cell(data)], filename);
        fprintf('✅ Excel 已生成：%s\n', filename);
    catch ME
        alt_name = strrep(filename, '.xlsx', ...
            ['_' datestr(now,'yyyymmdd_HHMMSS') '.xlsx']);
        warning('原文件写入失败，改写：%s\n错误：%s', alt_name, ME.message);
        writecell([colNames; num2cell(data)], alt_name);
        fprintf('✅ Excel 已生成（备用路径）：%s\n', alt_name);
    end
end


function print_summary(data, colNames, numRows, cfg)
    fprintf('\n===== 数据摘要 =====\n');
    fprintf('总行数：%d\n', numRows);
    fprintf('参数范围：a=[%.2g,%.2g]  Pe1=[%.2g,%.2g]  Pe2=[%.2g,%.2g]\n', ...
        cfg.a_range(1),   cfg.a_range(2), ...
        cfg.Pe1_range(1), cfg.Pe1_range(2), ...
        cfg.Pe2_range(1), cfg.Pe2_range(2));

    key_cols = {'Pe2','endT','coeff'};
    for kc = key_cols
        idx = find(strcmp(colNames, kc{1}));
        if ~isempty(idx)
            col_data = data(:, idx);
            fprintf('  %-8s min=%-12.4g  max=%-12.4g\n', ...
                kc{1}, min(col_data), max(col_data));
        end
    end
    fprintf('====================\n');
end