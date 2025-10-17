param(
    [string]$BuildDir = "build",
    [int]$Steps = 120,
    [int]$Fps = 60,
    [switch]$Reconfigure,
    [switch]$Rebuild,
    [switch]$NoClear,
    [switch]$Interactive,
    [switch]$UseGeneratedWorld,
    [string]$Commands
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot $BuildDir

if (-not (Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath | Out-Null
}

if ($Fps -lt 0) {
    throw "Fps must be greater than or equal to zero."
}

if ($Reconfigure -or -not (Test-Path (Join-Path $buildPath "CMakeCache.txt"))) {
    Write-Host "Configuring CMake project in $BuildDir..." -ForegroundColor Cyan
    & cmake -S $repoRoot -B $buildPath
}

Write-Host "Building sandbox CLI..." -ForegroundColor Cyan
$buildArgs = @('--target','genesis_sandbox_cli')
if ($Reconfigure -or $Rebuild) {
    & cmake --build $buildPath @buildArgs
} else {
    & cmake --build $buildPath @buildArgs | Out-Null
}

$cliExe = Join-Path $buildPath "src/genesis-sandbox-cli.exe"
if (-not (Test-Path $cliExe)) {
    throw "Failed to locate genesis-sandbox-cli.exe in $buildPath/src"
}

$layoutPath = Join-Path $repoRoot "data/ascii_layout.json"
$generatedWorldPath = Join-Path $repoRoot "data/world/generated/noise_mvp.json"
$generatedLayoutPath = Join-Path $repoRoot "data/world/generated/noise_mvp_layout.json"

if ($UseGeneratedWorld) {
    if (-not (Test-Path $generatedWorldPath)) {
        throw "Generated world JSON not found at $generatedWorldPath. Run scripts/generate_noise_world.ps1 first."
    }
    if (Test-Path $generatedLayoutPath) {
        $layoutPath = $generatedLayoutPath
    } else {
        Write-Warning "Generated layout JSON not found at $generatedLayoutPath. Falling back to default layout."
    }
}

Write-Host "Launching sandbox CLI..." -ForegroundColor Green

$cliArgs = @()
if (Test-Path $layoutPath) {
    $cliArgs += "--layout"
    $cliArgs += $layoutPath
}
if ($NoClear) {
    $cliArgs += "--no-clear"
}
if ($Fps -gt 0) {
    $cliArgs += "--fps"
    $cliArgs += $Fps
} elseif ($Fps -eq 0) {
    $cliArgs += "--fps"
    $cliArgs += "0"
}
if (-not $Interactive) {
    $cliArgs += "--commands"
    if (![string]::IsNullOrWhiteSpace($Commands)) {
        $cliArgs += $Commands
    } else {
        $cliArgs += "step $Steps;quit"
    }
}

$env:PATH = "{0};{1};{2}" -f (Join-Path $buildPath "src"), (Join-Path $buildPath "tests"), $env:PATH
$previousWorldPath = [System.Environment]::GetEnvironmentVariable("GENESIS_WORLD_PATH", "Process")
$hadPreviousWorld = $null -ne $previousWorldPath

if ($UseGeneratedWorld) {
    [System.Environment]::SetEnvironmentVariable("GENESIS_WORLD_PATH", $generatedWorldPath, "Process")
}

try {
    & $cliExe @cliArgs
} finally {
    if ($UseGeneratedWorld) {
        if ($hadPreviousWorld) {
            [System.Environment]::SetEnvironmentVariable("GENESIS_WORLD_PATH", $previousWorldPath, "Process")
        } else {
            [System.Environment]::SetEnvironmentVariable("GENESIS_WORLD_PATH", $null, "Process")
        }
    }
}
