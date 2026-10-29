@echo off
setlocal EnableDelayedExpansion

title LiveBus VST3 Plugin Installer
color 0B

echo ============================================================
echo        LIVEBUS AUDIO SUITE - VST3 AUTOMATIC INSTALLER       
echo ============================================================
echo.

:: Check for Administrative Rights
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [INFO] Membutuhkan hak akses Administrator untuk menyalin ke Program Files.
    echo [INFO] Meminta elevasi Administrator via UAC...
    echo.
    powershell -Command "Start-Process cmd -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)

set "TARGET_DIR=%CommonProgramFiles%\VST3"
if not defined CommonProgramFiles (
    set "TARGET_DIR=C:\Program Files\Common Files\VST3"
)

echo [1/3] Memeriksa direktori tujuan VST3:
echo       "%TARGET_DIR%"
if not exist "%TARGET_DIR%" (
    echo [INFO] Membuat direktori VST3...
    mkdir "%TARGET_DIR%"
)
echo.

set "SCRIPT_DIR=%~dp0"
set "SRC_DIR=%SCRIPT_DIR%Plugins"

if not exist "%SRC_DIR%" (
    if exist "%SCRIPT_DIR%build\LiveBusMixer_artefacts\Release\Plugins" (
        set "SRC_DIR=%SCRIPT_DIR%build\LiveBusMixer_artefacts\Release\Plugins"
    )
)

if not exist "%SRC_DIR%" (
    echo [ERROR] Folder bundle VST3 tidak ditemukan di:
    echo         "%SRC_DIR%"
    echo.
    pause
    exit /b 1
)

echo [2/3] Menyalin bundle plugin VST3...
echo.

:: 1. LiveBus Alesis.vst3
if exist "%SRC_DIR%\LiveBus Alesis.vst3" (
    echo  - [COPYING] LiveBus Alesis.vst3 ...
    robocopy "%SRC_DIR%\LiveBus Alesis.vst3" "%TARGET_DIR%\LiveBus Alesis.vst3" /E /IS /IT /NP /NJH /NJS >nul
    if %errorlevel% leq 3 (
        echo    [OK] LiveBus Alesis.vst3 berhasil diinstal!
    ) else (
        echo    [GAGAL] Gagal menyalin LiveBus Alesis.vst3!
    )
) else (
    echo  - [SKIP] LiveBus Alesis.vst3 tidak ditemukan di sumber.
)

:: 2. Vocal FX Pro.vst3
if exist "%SRC_DIR%\Vocal FX Pro.vst3" (
    echo  - [COPYING] Vocal FX Pro.vst3 ...
    robocopy "%SRC_DIR%\Vocal FX Pro.vst3" "%TARGET_DIR%\Vocal FX Pro.vst3" /E /IS /IT /NP /NJH /NJS >nul
    if %errorlevel% leq 3 (
        echo    [OK] Vocal FX Pro.vst3 berhasil diinstal!
    ) else (
        echo    [GAGAL] Gagal menyalin Vocal FX Pro.vst3!
    )
) else (
    echo  - [SKIP] Vocal FX Pro.vst3 tidak ditemukan di sumber.
)

:: 3. LiveBus RNNoise.vst3
if exist "%SRC_DIR%\LiveBus RNNoise.vst3" (
    echo  - [COPYING] LiveBus RNNoise.vst3 ...
    robocopy "%SRC_DIR%\LiveBus RNNoise.vst3" "%TARGET_DIR%\LiveBus RNNoise.vst3" /E /IS /IT /NP /NJH /NJS >nul
    if %errorlevel% leq 3 (
        echo    [OK] LiveBus RNNoise.vst3 berhasil diinstal!
    ) else (
        echo    [GAGAL] Gagal menyalin LiveBus RNNoise.vst3!
    )
) else (
    echo  - [SKIP] LiveBus RNNoise.vst3 tidak ditemukan di sumber.
)

echo.
echo [3/3] Selesai!
echo ============================================================
echo SEMUA PLUGIN BERHASIL DIINSTAL KE:
echo "%TARGET_DIR%"
echo.
echo Buka DAW Anda (FL Studio, Reaper, Cubase, Ableton, Studio One)
echo lalu jalankan "Rescan Plugins / Manage Plugins" untuk memuat:
echo  1. LiveBus Alesis (Reverb, Delay, 7-Band EQ FFT, 3630 Comp, Vocal FX)
echo  2. Vocal FX Pro (AutoTune, 3-Band EQ, Comp, Reverb, Delay)
echo  3. LiveBus RNNoise (AI Deep Learning Noise Suppressor)
echo ============================================================
echo.
pause
