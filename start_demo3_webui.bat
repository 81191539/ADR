@echo off
chcp 65001 > nul
cd /d "%~dp0demo3"
set PYTHONUTF8=1
set PYTHONIOENCODING=utf-8
python webui\server.py --port 8003 --open-browser
if errorlevel 1 pause
