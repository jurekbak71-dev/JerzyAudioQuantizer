$ErrorActionPreference = "Stop"

Write-Host "=== JERZY AUDIO QUANTIZER / Windows build ==="

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake not found in PATH. Install CMake 3.22+ first."
}

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

$vst = Join-Path $PWD "build\JerzyAudioQuantizer_artefacts\Release\VST3\JERZY AUDIO QUANTIZER.vst3"

if (Test-Path $vst) {
    Write-Host ""
    Write-Host "BUILD OK: $vst"
    Write-Host "Copy to C:\Program Files\Common Files\VST3\ and rescan FL Studio."
} else {
    Write-Warning "Build finished, but expected VST3 path was not found: $vst"
}
