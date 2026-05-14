@echo off
cd /d "%~dp0"
wsl -e bash -c "cd '%cd:\=/\%' && ./build/df2d"
pause
