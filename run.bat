@echo off
cd /d "%~dp0"
wsl -e bash -c "cd '%cd:\=/\%' && ./df2d"
pause