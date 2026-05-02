@echo off
cd /d "%~dp0demo1"
set PYTHONUTF8=1
python webui\server.py --port 8001 --open-browser
if errorlevel 1 pause
