@echo off
chcp 65001 > nul
setlocal
set "PORT=8004"
set "FOUND=0"

for /f "tokens=5" %%P in ('netstat -ano ^| findstr /R /C:":%PORT% .*LISTENING"') do (
    set "FOUND=1"
    echo Stopping demo4 Web UI process on port %PORT%, PID %%P...
    taskkill /PID %%P /F
    if errorlevel 1 (
        echo Failed to stop PID %%P.
        exit /b 1
    )
)

if "%FOUND%"=="1" (
    echo demo4 Web UI stopped.
    exit /b 0
)

echo No demo4 Web UI process is listening on port %PORT%.
exit /b 0
