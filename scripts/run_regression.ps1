param(
  [string]$Generator = "Ninja",
  [string]$Config = "Release",
  [string]$CoreBuildDir = "build_regression_core",
  [string]$GuiBuildDir = "build_regression_gui"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

function Invoke-BuildAndTest {
  param(
    [string]$BuildDir,
    [hashtable]$Definitions
  )

  $args = @("-S", $repoRoot, "-B", (Join-Path $repoRoot $BuildDir), "-G", $Generator)
  foreach ($k in $Definitions.Keys) {
    $args += ("-D{0}={1}" -f $k, $Definitions[$k])
  }

  & cmake @args | Write-Host
  & cmake --build (Join-Path $repoRoot $BuildDir) --parallel --config $Config | Write-Host
  & ctest --test-dir (Join-Path $repoRoot $BuildDir) --output-on-failure -C $Config | Write-Host
}

Invoke-BuildAndTest -BuildDir $CoreBuildDir -Definitions @{
  "GENESISENGINE_ENABLE_TESTS" = "ON"
  "GENESIS_BUILD_GUI" = "OFF"
  "GENESIS_WITH_STYLE" = "OFF"
}

Invoke-BuildAndTest -BuildDir $GuiBuildDir -Definitions @{
  "GENESISENGINE_ENABLE_TESTS" = "ON"
  "GENESIS_BUILD_GUI" = "ON"
  "GENESIS_WITH_STYLE" = "OFF"
}
