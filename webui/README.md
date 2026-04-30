# PDE Solver Web UI

## Start

在项目根目录运行：

```bat
start_webui.bat
```

或者直接执行：

```bat
python webui\server.py --open-browser
```

默认地址是 `http://127.0.0.1:8123`。

## What It Does

- 浏览并编辑 `input/input_parameter_XXXX.txt`
- 每次运行前自动更新 `include/config.h` 中的 `get_case_list()` 为当前 case
- 先执行 `make clean && make`，成功后再运行 `./df2d`
- 查看环境检查、构建日志、运行日志
- 解析 `output/eta_ave_X.m`、`output/remarks_X.m`、`output/data_X/cc_N.m`、`output/data_X/ee_N.m`

## Notes

- 当前 `df2d` 是 Linux ELF，可执行和编译流程依赖 WSL/Linux 环境。
- 如果 WSL 或编译器不可用，界面仍然允许编辑 case 和查看历史结果，但不会允许启动运行。
