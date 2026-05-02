@echo off
cd /d "%~dp0"
set PYTHONUTF8=1
python webui\server.py --port 8000 --open-browser
if errorlevel 1 pause
