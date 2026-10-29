# LiveBus VST3 Plugin Automatic Installer for PowerShell
# Requires Run as Administrator to write to Program Files

[CmdletBinding()]
param()

$Host.UI.RawUI.WindowTitle = "LiveBus VST3 Plugin Installer"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "       LIVEBUS AUDIO SUITE - VST3 AUTOMATIC INSTALLER       " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# 1. Elevate if not admin
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[INFO] Membutuhkan hak akses Administrator untuk menyalin ke Program Files." -ForegroundColor Yellow
    Write-Host "[INFO] Meminta elevasi Administrator via UAC..." -ForegroundColor Yellow
    Start-Process powershell.exe -Verb RunAs -ArgumentList ("-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"")
    exit
}

# 2. Target Directory
$targetDir = Join-Path $env:CommonProgramFiles "VST3"
if (-not (Test-Path $targetDir)) {
    Write-Host "[INFO] Membuat direktori tujuan: $targetDir" -ForegroundColor Yellow
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
}
Write-Host "[1/3] Direktori tujuan VST3: $targetDir" -ForegroundColor Green
Write-Host ""

# 3. Source Directory
$scriptDir = Split-Path -Parent $PSCommandPath
$sourceDir = Join-Path $scriptDir "Plugins"
if (-not (Test-Path $sourceDir)) {
    $fallback = Join-Path $scriptDir "build\LiveBusMixer_artefacts\Release\Plugins"
    if (Test-Path $fallback) {
        $sourceDir = $fallback
    }
}

if (-not (Test-Path $sourceDir)) {
    Write-Host "[ERROR] Folder plugin sumber tidak ditemukan di: $sourceDir" -ForegroundColor Red
    Read-Host "Tekan Enter untuk keluar..."
    exit 1
}

Write-Host "[2/3] Menyalin bundle plugin VST3 dari: $sourceDir" -ForegroundColor Cyan
Write-Host ""

$plugins = @("LiveBus Alesis.vst3", "Vocal FX Pro.vst3", "LiveBus RNNoise.vst3")
foreach ($plugin in $plugins) {
    $srcPlugin = Join-Path $sourceDir $plugin
    $dstPlugin = Join-Path $targetDir $plugin

    if (Test-Path $srcPlugin) {
        Write-Host "  - [COPYING] $plugin ..." -NoNewline
        try {
            Copy-Item -Path $srcPlugin -Destination $dstPlugin -Recurse -Force
            Write-Host " [BERHASIL]" -ForegroundColor Green
        } catch {
            Write-Host " [GAGAL: $($_.Exception.Message)]" -ForegroundColor Red
        }
    } else {
        Write-Host "  - [SKIP] $plugin tidak ditemukan di sumber." -ForegroundColor DarkGray
    }
}

Write-Host ""
Write-Host "[3/3] Selesai!" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "SEMUA PLUGIN BERHASIL DIINSTAL KE:" -ForegroundColor Yellow
Write-Host " $targetDir" -ForegroundColor White
Write-Host ""
Write-Host "Buka DAW Anda (FL Studio, Reaper, Cubase, Ableton, Studio One)" -ForegroundColor White
Write-Host "lalu jalankan 'Rescan Plugins' untuk menggunakan plugin." -ForegroundColor White
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""
Read-Host "Tekan Enter untuk menutup installer..."
