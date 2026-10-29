$ErrorActionPreference = "Stop"

$sourceDirectory = Join-Path $PSScriptRoot "bin"
$requiredFiles = @(
    "LiveBusMixer.exe",
    "LiveBusPluginHost.exe",
    "MSVCP140.dll",
    "VCRUNTIME140.dll",
    "VCRUNTIME140_1.dll"
)

foreach ($name in $requiredFiles) {
    $filePath = Join-Path $sourceDirectory $name
    if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
        throw "The release package is incomplete. Required file not found: $filePath"
    }
}

$pluginPath = Join-Path $sourceDirectory "Plugins\LiveBus RNNoise.vst3\Contents\x86_64-win\LiveBus RNNoise.vst3"
if (-not (Test-Path -LiteralPath $pluginPath -PathType Leaf)) {
    throw "The bundled RNNoise VST3 plugin is missing: $pluginPath"
}

if (-not $env:LOCALAPPDATA) {
    throw "The per-user Local AppData directory could not be determined."
}

$installDirectory = Join-Path $env:LOCALAPPDATA "Programs\LiveBus Mixer"
$applicationPath = Join-Path $installDirectory "LiveBusMixer.exe"
$programsDirectory = [Environment]::GetFolderPath([Environment+SpecialFolder]::Programs)
if (-not $programsDirectory) {
    throw "The current user's Start Menu Programs directory could not be determined."
}

New-Item -ItemType Directory -Path $installDirectory -Force | Out-Null
Copy-Item -Path (Join-Path $sourceDirectory "*") -Destination $installDirectory -Recurse -Force

$shortcutPath = Join-Path $programsDirectory "LiveBus Mixer.lnk"
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $applicationPath
$shortcut.WorkingDirectory = $installDirectory
$shortcut.IconLocation = "$applicationPath,0"
$shortcut.Save()

Write-Output "LiveBus Mixer was installed for the current Windows user."
Write-Output "Application: $applicationPath"
Write-Output "Start Menu shortcut: $shortcutPath"
Write-Output "To uninstall, remove the application folder and the Start Menu shortcut."
