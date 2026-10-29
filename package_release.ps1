param()

$exportDir = "c:\Users\miran\Documents\mixer\build\export"
$rootDir = "c:\Users\miran\Documents\mixer"

if (-not (Test-Path $exportDir)) {
    New-Item -ItemType Directory -Force -Path $exportDir | Out-Null
}

Remove-Item -Path "$exportDir\LiveBus_Alesis_Release_VST3.zip","$exportDir\LiveBus_All_VST3_Plugins_Bundle.zip" -Force -ErrorAction SilentlyContinue

# 1. Package Alesis
$alesisFolder = "$exportDir\LiveBus_Alesis_Package"
Remove-Item -Recurse -Force $alesisFolder -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path "$alesisFolder\Plugins" | Out-Null
Copy-Item -Recurse -Force "$rootDir\build\LiveBusMixer_artefacts\Release\Plugins\LiveBus Alesis.vst3" "$alesisFolder\Plugins\LiveBus Alesis.vst3"
Copy-Item "$rootDir\install_vst3_plugins.bat" "$alesisFolder\install_vst3_plugins.bat"
Copy-Item "$rootDir\install_vst3_plugins.ps1" "$alesisFolder\install_vst3_plugins.ps1"
Compress-Archive -Path "$alesisFolder\*" -DestinationPath "$exportDir\LiveBus_Alesis_Release_VST3.zip" -Force
Remove-Item -Recurse -Force $alesisFolder

# 2. Package Master Bundle
$bundleFolder = "$exportDir\LiveBus_Master_Bundle"
Remove-Item -Recurse -Force $bundleFolder -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path "$bundleFolder\Plugins" | Out-Null
Copy-Item -Recurse -Force "$rootDir\build\LiveBusMixer_artefacts\Release\Plugins\*" "$bundleFolder\Plugins"
Copy-Item "$rootDir\install_vst3_plugins.bat" "$bundleFolder\install_vst3_plugins.bat"
Copy-Item "$rootDir\install_vst3_plugins.ps1" "$bundleFolder\install_vst3_plugins.ps1"
Compress-Archive -Path "$bundleFolder\*" -DestinationPath "$exportDir\LiveBus_All_VST3_Plugins_Bundle.zip" -Force
Remove-Item -Recurse -Force $bundleFolder

Write-Host "Packaging Complete!"
Get-ChildItem -Path $exportDir | Select-Object Name, Length, LastWriteTime
