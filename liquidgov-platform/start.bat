@echo off
cd /d "%~dp0"
echo Starting LiquidGov preview...
echo Leave this window open. In your browser open:
echo   http://127.0.0.1:8080
echo.
py preview.py 2>nul
if errorlevel 1 python preview.py
pause
