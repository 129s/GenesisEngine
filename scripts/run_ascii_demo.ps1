param(
    [string]$BuildDir = "build",
    [int]$Steps = 360,
    [string]$TelemetryOutput = "telemetry_demo.json",
    [int]$Frames = 80,
    [int]$Delay = 120,
    [switch]$Reconfigure,
    [switch]$Rebuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot $BuildDir

if (-not (Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath | Out-Null
}

if ($Reconfigure -or -not (Test-Path (Join-Path $buildPath "CMakeCache.txt"))) {
    Write-Host "Configuring CMake project in $BuildDir..." -ForegroundColor Cyan
    & cmake -S $repoRoot -B $buildPath
}

if ($Reconfigure -or $Rebuild) {
    Write-Host "Building project..." -ForegroundColor Cyan
    & cmake --build $buildPath
}

$engineExe = Join-Path $buildPath "src/genesis-engine.exe"
$viewerExe = Join-Path $buildPath "src/genesis-ascii-viewer.exe"

if (-not (Test-Path $engineExe) -or -not (Test-Path $viewerExe)) {
    Write-Host "Required executables missing, building..." -ForegroundColor Cyan
    & cmake --build $buildPath
}

if (-not (Test-Path $engineExe) -or -not (Test-Path $viewerExe)) {
    throw "Failed to locate genesis-engine.exe or genesis-ascii-viewer.exe in $buildPath/src"
}

$telemetryPath = Join-Path $buildPath $TelemetryOutput
Write-Host "Running simulation for $Steps steps..." -ForegroundColor Green
& $engineExe "--steps=$Steps" "--telemetry-file=$telemetryPath"

if (-not (Test-Path $telemetryPath)) {
    throw "Telemetry output was not generated at $telemetryPath"
}

$layoutPath = Join-Path $repoRoot "data/ascii_layout.json"

Write-Host "Launching ASCII viewer (frames=$Frames delay=${Delay}ms)..." -ForegroundColor Green
& $viewerExe "--telemetry=$telemetryPath" "--layout=$layoutPath" "--frames=$Frames" "--delay=$Delay"

Write-Host "Playback complete." -ForegroundColor Green
