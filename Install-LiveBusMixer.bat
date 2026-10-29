@echo off
title Install LiveBus Mixer
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-LiveBusMixer.ps1"
if errorlevel 1 (
    echo.
    echo Installation failed. Read the error above, then try again.
    pause
    exit /b 1
)
echo.
pause
