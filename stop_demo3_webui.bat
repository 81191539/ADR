@echo off
chcp 65001 > nul
setlocal
set "PORT=8003"

for /f "tokens=5" %%P in ('netstat -ano ^| findstr /R /C:":%PORT% .*LISTENING"') do (
    echo Stopping demo3 Web UI process on port %PORT%, PID %%P...
    taskkill /PID %%P /F
    if errorlevel 1 (
        echo Failed to stop PID %%P.
        exit /b 1
    )
    echo demo3 Web UI stopped.
    exit /b 0
)

echo No demo3 Web UI process is listening on port %PORT%.
exit /b 0
