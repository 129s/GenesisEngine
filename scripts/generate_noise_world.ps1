param(
    [string]$BuildDir = "build",
    [UInt64]$Seed,
    [UInt32]$Width = 64,
    [UInt32]$Height = 64,
    [double]$Threshold = 0.5,
    [double]$SoilDensity = 0.05,
    [UInt32]$Capacity = 24,
    [UInt32]$Rate = 3,
    [string]$WorldPath = "data/world/generated/noise_mvp.json",
    [string]$LayoutPath = "data/world/generated/noise_mvp_layout.json",
    [switch]$Reconfigure,
    [switch]$Rebuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($Seed -eq 0) {
    throw "Seed is required and must be non-zero. Use --Seed <value>."
}

if ($Threshold -lt 0.0 -or $Threshold -gt 1.0) {
    throw "Threshold must be within [0, 1]."
}

if ($SoilDensity -lt 0.0 -or $SoilDensity -gt 1.0) {
    throw "SoilDensity must be within [0, 1]."
}

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot $BuildDir

if (-not (Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath | Out-Null
}

if ($Reconfigure -or -not (Test-Path (Join-Path $buildPath "CMakeCache.txt"))) {
    Write-Host "Configuring CMake project in $BuildDir..." -ForegroundColor Cyan
    & cmake -S $repoRoot -B $buildPath
}

$needsBuild = $Reconfigure -or $Rebuild
$generatorExe = Join-Path $buildPath "src/genesis-noise-generator.exe"

if ($needsBuild -or -not (Test-Path $generatorExe)) {
    Write-Host "Building noise generator tool..." -ForegroundColor Cyan
    & cmake --build $buildPath --target genesis_noise_generator
    if (-not (Test-Path $generatorExe)) {
        throw "genesis-noise-generator.exe was not produced in $($buildPath)\src"
    }
}

function Resolve-ProjectPath([string]$path) {
    if ([string]::IsNullOrWhiteSpace($path)) {
        return $path
    }
    if ([System.IO.Path]::IsPathRooted($path)) {
        $parent = Split-Path -Parent $path
        if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path $parent)) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        return $path
    }
    $fullPath = Join-Path $repoRoot $path
    $dir = Split-Path -Parent $fullPath
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    return $fullPath
}

$resolvedWorld = Resolve-ProjectPath $WorldPath
$resolvedLayout = Resolve-ProjectPath $LayoutPath

$arguments = @(
    "--seed=$Seed",
    "--width=$Width",
    "--height=$Height",
    "--threshold=$Threshold",
    "--soil-density=$SoilDensity",
    "--capacity=$Capacity",
    "--rate=$Rate",
    "--world=$resolvedWorld",
    "--layout=$resolvedLayout"
)

Write-Host "Generating noise map to:" -ForegroundColor Green
Write-Host "  World : $resolvedWorld"
Write-Host "  Layout: $resolvedLayout"

& $generatorExe @arguments
