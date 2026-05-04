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

默认地址是 `http://127.0.0.1:8000`。

## What It Does

- 浏览并编辑 `input/input_parameter_XXXX.toml`，并兼容读取旧的 `.txt` case
- 优先使用本机 CMake 配置并构建 `build/`，本机不可用时回退到 WSL + CMake
- 构建成功后运行选中的 case：`df2d --case <id>`
- 可勾选“从头重跑”，运行时追加 `--force-restart`
- 查看环境检查、构建日志、运行日志
- 解析 `output/eta_ave_X.m`、`output/remarks_X.m`、`output/data_X/cc_N.m`、`output/data_X/ee_N.m`

## Notes

- `makefile` 仅作为 legacy Linux/WSL 入口保留，Web UI 不再调用 `make clean && make`。
- 如果本机和 WSL CMake 都不可用，界面仍然允许编辑 case 和查看历史结果，但不会允许启动运行。
- 文件按 UTF-8 保存。若 PowerShell 显示中文乱码，请调整终端编码，而不是转换文件内容。
