# ADR 可用性与鲁棒性改进建议

生成日期：2026-04-30

## 项目现状概览

当前项目是一个二维扩散-对流-吸附 PDE 求解器，核心程序使用 C++17 实现，支持 CPU/OpenMP 后端，并在有 CUDA 编译环境时支持 GPU 后端。项目还包含一个本地 Python Web UI，用于编辑 case、编译运行求解器、查看输出结果。

主要数据流如下：

```text
input/input_parameter_XXXX.txt
  -> read_parameter()
  -> run_cases()
  -> case_calculation()
  -> CPU/CUDA backend
  -> full_step_explicit()
  -> output/eta_ave_X.m, output/data_X/, output/remarks_X.m
```

## 高优先级建议

### 1. 避免硬编码 case 选择

当前 `include/config.h` 中 `USE_CASE_LIST = true`，且 `get_case_list()` 返回固定 case。如果仓库中没有对应输入文件，直接运行 `df2d` 会失败。

建议：

- 默认改为自动扫描 `input/` 下的 `input_parameter_*.txt`。
- 增加命令行参数，例如 `./df2d --case 1` 或 `./df2d --cases 1,2,3`。
- 保留 `config.h` 的静态配置作为兼容模式，而不是默认入口。

预期收益：

- 直接运行程序更可靠。
- 不需要每次改 case 都重新编译。
- 更适合批处理和自动化运行。

### 2. Web UI 不应修改 `config.h`

当前 Web UI 会改写 `include/config.h` 来选择 case，然后重新编译。这会带来几个问题：

- 会污染 Git 工作区。
- 多个运行任务可能互相覆盖 case 配置。
- 改 case 就要重新编译，交互体验较差。

建议：

- 将 case 选择改为运行时参数。
- Web UI 调用 `./df2d --case <id>`。
- `config.h` 只保留真正需要编译期决定的选项。

预期收益：

- Web UI 更稳定。
- Git 状态更干净。
- 编译和运行职责更清晰。

### 3. 增加输入参数校验

当前参数读取后直接进入数值计算，容易因为非法输入导致 NaN、除零、循环异常或无意义结果。

建议至少校验：

```text
lam > 0
ny > 0
K0 > 0
alpha != 0
total_count > 0
coeff_dt > 0
endT > 0
0 <= xpo_l < xpo_r <= 1
```

发现非法参数时，应输出明确错误，例如：

```text
Case 3 invalid: K0 must be greater than 0.
```

预期收益：

- 更早发现配置错误。
- 减少运行到中途才崩溃的情况。
- Web UI 可以直接显示可理解的错误信息。

### 4. 默认启用 checkpoint 续算

当前 `FORCE_RESTART_ALL = true`，会导致程序默认忽略 checkpoint，从而削弱断点续算价值。

建议：

- 默认改为 `FORCE_RESTART_ALL = false`。
- 增加命令行参数 `--force-restart`。
- Web UI 增加“从头重跑”开关。

预期收益：

- 长时间仿真中断后可以继续。
- 减少重复计算成本。

## 中优先级建议

### 5. 统一构建方式

项目同时存在 `CMakeLists.txt` 和 `makefile`，但 Web UI 目前只调用 WSL 中的 `make clean && make`。

建议：

- 长期统一到 CMake。
- Web UI 调用 `cmake --build build`。
- 保留 makefile 作为轻量兼容入口，或明确标注为 legacy。

预期收益：

- Windows、Linux、WSL、CUDA 构建方式更一致。
- 更容易接入 CI。

### 6. 避免 OpenMP 嵌套过度并行

CPU 模式下，外层 case 循环使用 OpenMP 并行，内层数值 kernel 也使用 OpenMP 并行。case 较多时可能出现线程过度占用。

建议：

- 单 case 运行时，优先使用 kernel 内部并行。
- 多 case 批量运行时，可选择 case 级并行，并限制每个 case 的线程数。
- 增加运行参数控制，例如 `--threads` 和 `--case-parallelism`。

预期收益：

- 避免线程争抢。
- 性能更可预测。

### 7. 加强数值稳定性检测

当前主要检测 NaN。建议扩展到：

- `inf`
- 过大绝对值
- 负浓度
- `eta` 超出合理范围

并在日志中记录触发稳定性处理的具体原因。

预期收益：

- 更容易定位不稳定 case。
- 自动缩小时间步时有更清晰依据。

### 8. 改进输入文件格式

当前 case 文件是一行位置参数，维护时容易填错。

建议支持更可读的格式，例如 TOML：

```toml
lam = 0.033333
Pe = 10
Pe2 = 10
eps = 0.1
Da = 100
K0 = 1
ny = 50
xpo_l = 0.33333
xpo_r = 0.66667
endT = 60
total_count = 300
coeff_dt = 0.1
x_ini_posi = 5
alpha = 0.01
```

旧的一行格式可以继续支持，保证兼容历史数据。

## 可用性提升建议

### 9. 修复中文乱码

部分中文注释和 `webui/README.md` 存在编码乱码。建议统一转换为 UTF-8。

预期收益：

- 文档和注释可读性提升。
- 降低后续维护成本。

### 10. 增加运行元数据

建议每次运行输出 `run_metadata.json`，记录：

- case 编号
- 输入参数
- 网格信息
- 初始时间步与最终时间步
- 是否收敛
- 运行时间
- 后端类型
- Git commit hash

预期收益：

- 结果可追溯。
- 后处理和论文数据整理更方便。

### 11. 优化大结果文件输出

当前 MATLAB 文本格式直观，但大网格下文件会变大、写入变慢。

建议：

- 保留 `.m` 输出作为默认兼容格式。
- 增加 CSV、NPZ、HDF5 或压缩输出选项。
- 允许用户设置快照输出间隔。

预期收益：

- 大规模仿真更省空间。
- 后处理更灵活。

### 12. 增加一键 GitHub 同步脚本

为日常使用增加 `sync_to_github.ps1`，自动执行：

```powershell
git status
git add -A
git commit -m "<message>"
git push
```

建议脚本在没有改动时直接退出，避免空提交。

预期收益：

- 降低 Git 使用门槛。
- 项目完成阶段更容易备份到 GitHub。

## 推荐实施顺序

建议第一轮优先做：

1. 增加命令行 case 参数。
2. Web UI 改为运行时传 case，不再修改 `config.h`。
3. 增加输入参数校验。
4. 默认启用 checkpoint 续算。

完成这四项后，项目的直接可用性、运行稳定性和维护体验会明显提升。

