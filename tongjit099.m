% 自动获取目录下所有匹配的文件名
files = dir('eta_ave_*.m');

% 提取文件编号
fileNumbers = zeros(length(files), 1);
for k = 1:length(files)
    % 提取数字部分
    name = files(k).name;
    % 匹配 'eta_ave_X.m' 中的X
    token = regexp(name, 'eta_ave_(\d+)\.m', 'tokens');
    if ~isempty(token)
        fileNumbers(k) = str2double(token{1});
    end
end

% 去除可能为0的空值编号，按升序排列
fileNumbers = sort(fileNumbers(fileNumbers > 0));

% 初始化结果存储
results = zeros(length(fileNumbers), 2); % 第一列存编号，第二列存值

for k = 1:length(fileNumbers)
    i = fileNumbers(k);
    fileName = ['eta_ave_', num2str(i), '.m'];
    
    if exist(fileName, 'file')
        data = dlmread(fileName);
        if size(data,2) >= 2
            col1 = data(:, 1);
            col2 = data(:, 2);
            
            % 判断连续两行均 > 0.495
            cond = (col2 > 0.495);
            cond_pair = cond(1:end-1) & cond(2:end);
            idx = find(cond_pair, 1, 'first');
            
            if ~isempty(idx)
                % 记录第一行的 col1 值
                results(k, :) = [i, col1(idx)];
            else
                results(k, :) = [i, NaN];
            end
        else
            fprintf('File %s does not have two columns.\n', fileName);
            results(k, :) = [i, NaN];
        end
    else
        fprintf('File %s does not exist.\n', fileName);
        results(k, :) = [i, NaN];
    end
end

% 输出结果
disp('Results (File Index and Corresponding First Column Value):');
disp(results);

% 保存结果到文件
outputFileName = 'tongji_extracted_099_results.txt';
writematrix(results, outputFileName, 'Delimiter', '\t');
disp(['Results saved to ', outputFileName]);