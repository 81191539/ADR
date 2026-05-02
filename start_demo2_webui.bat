@echo off
cd /d "%~dp0demo2"
set PYTHONUTF8=1
python webui\server.py --port 8002 --open-browser
if errorlevel 1 pause
