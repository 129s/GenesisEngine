param(
    [string]$BuildDir = "build",
    [int]$Steps = 120,
    [switch]$Reconfigure,
    [switch]$Rebuild,
    [switch]$NoClear,
    [switch]$Interactive
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

Write-Host "Launching sandbox CLI..." -ForegroundColor Green

$cliArgs = @()
if (Test-Path $layoutPath) {
    $cliArgs += "--layout"
    $cliArgs += $layoutPath
}
if ($NoClear) {
    $cliArgs += "--no-clear"
}
if (-not $Interactive) {
    $cliArgs += "--commands"
    $cliArgs += "step $Steps;quit"
}

$env:PATH = "{0};{1};{2}" -f (Join-Path $buildPath "src"), (Join-Path $buildPath "tests"), $env:PATH
& $cliExe @cliArgs
