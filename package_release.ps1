$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
$buildRoot = Join-Path $root "build"
$releaseFiles = @{
    "LiveBusMixer.exe" = Join-Path $buildRoot "LiveBusMixer_artefacts\Release\LiveBusMixer.exe"
    "LiveBusPluginHost.exe" = Join-Path $buildRoot "LiveBusPluginHost_artefacts\Release\LiveBusPluginHost.exe"
}
$demoFiles = @("Cello_Solo.wav", "Guitar_Riff.wav")
$documentationFiles = @("FEATURES-AND-LIMITATIONS.md", "FINAL_HARDWARE_CHECKLIST.md")

foreach ($name in $releaseFiles.Keys) {
    if (-not (Test-Path -LiteralPath $releaseFiles[$name] -PathType Leaf)) {
        throw "Required Release binary is missing: $($releaseFiles[$name])"
    }
}

foreach ($name in $demoFiles + $documentationFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $name) -PathType Leaf)) {
        throw "Required release file is missing: $(Join-Path $root $name)"
    }
}

$releaseName = "LiveBusMixer-Release-" + (Get-Date -Format "yyyyMMdd-HHmmss")
$staging = Join-Path $root "dist\$releaseName"
$zipPath = "$staging.zip"
if ((Test-Path -LiteralPath $staging) -or (Test-Path -LiteralPath $zipPath)) {
    throw "Release output already exists; refusing to overwrite it: $releaseName"
}

$bin = Join-Path $staging "bin"
New-Item -ItemType Directory -Path $bin -Force | Out-Null
foreach ($name in $releaseFiles.Keys) {
    Copy-Item -LiteralPath $releaseFiles[$name] -Destination (Join-Path $bin $name)
}
foreach ($name in $demoFiles) {
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $bin $name)
}
Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $staging
Copy-Item -LiteralPath (Join-Path $root "Jalankan_LiveBusMixer.bat") -Destination $staging
Copy-Item -LiteralPath (Join-Path $root "run_app.py") -Destination $staging
foreach ($name in $documentationFiles) {
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $staging
}

Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $zipPath -CompressionLevel Optimal

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $entries = @($archive.Entries | ForEach-Object { $_.FullName.Replace("\", "/") })
    foreach ($entry in @(
        "bin/LiveBusMixer.exe",
        "bin/LiveBusPluginHost.exe",
        "bin/Cello_Solo.wav",
        "bin/Guitar_Riff.wav",
        "Jalankan_LiveBusMixer.bat",
        "run_app.py",
        "README.md",
        "FEATURES-AND-LIMITATIONS.md",
        "FINAL_HARDWARE_CHECKLIST.md"
    )) {
        if ($entries -notcontains $entry) {
            throw "Release archive verification failed; missing entry: $entry"
        }
    }
}
finally {
    $archive.Dispose()
}

Get-Item -LiteralPath $zipPath | Select-Object FullName, Length, LastWriteTime
