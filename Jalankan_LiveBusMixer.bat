@echo off
title LiveBus Mixer v1.0
cd /d "%~dp0"
if not exist "%~dp0bin\LiveBusMixer.exe" (
    echo LiveBusMixer.exe tidak ditemukan di folder bin.
    echo Ekstrak seluruh isi paket ZIP sebelum menjalankan aplikasi.
    pause
    exit /b 1
)
start "" "%~dp0bin\LiveBusMixer.exe"
