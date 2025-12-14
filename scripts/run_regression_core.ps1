param(
  [string]$Generator = "Ninja",
  [string]$Config = "Release",
  [string]$BuildDir = "build_regression_core"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

$args = @("-S", $repoRoot, "-B", (Join-Path $repoRoot $BuildDir), "-G", $Generator)
$args += "-DGENESISENGINE_ENABLE_TESTS=ON"
$args += "-DGENESIS_BUILD_GUI=OFF"
$args += "-DGENESIS_WITH_STYLE=OFF"

& cmake @args | Write-Host
& cmake --build (Join-Path $repoRoot $BuildDir) --parallel --config $Config | Write-Host
& ctest --test-dir (Join-Path $repoRoot $BuildDir) --output-on-failure -C $Config | Write-Host

