@echo off
setlocal
set "PORT=8000"

for /f "tokens=5" %%P in ('netstat -ano ^| findstr /R /C:":%PORT% .*LISTENING"') do (
    echo Stopping Web UI process on port %PORT%, PID %%P...
    taskkill /PID %%P /F
    if errorlevel 1 (
        echo Failed to stop PID %%P.
        exit /b 1
    )
    echo Web UI stopped.
    exit /b 0
)

echo No Web UI process is listening on port %PORT%.
exit /b 0
