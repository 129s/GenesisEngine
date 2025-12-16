param(
  [string]$Generator = "Ninja",
  [string]$Config = "Release",
  [string]$CoreBuildDir = "build_regression_core",
  [string]$GuiBuildDir = "build_regression_gui",
  [switch]$UseMsvc
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

$cpmCache = $env:CPM_SOURCE_CACHE
if ([string]::IsNullOrWhiteSpace($cpmCache)) {
  $cpmCache = Join-Path $repoRoot ".cpmcache"
  $env:CPM_SOURCE_CACHE = $cpmCache
}
New-Item -ItemType Directory -Force -Path $cpmCache | Out-Null

function Import-MsvcDevEnv {
  $candidates = @()
  if (-not [string]::IsNullOrWhiteSpace($env:VSINSTALLDIR)) {
    $candidates += $env:VSINSTALLDIR
  }
  $candidates += @(
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
    "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise"
  )

  $vsDevCmd = $null
  foreach ($root in $candidates) {
    if ([string]::IsNullOrWhiteSpace($root)) { continue }
    $p = Join-Path $root "Common7\\Tools\\VsDevCmd.bat"
    if (Test-Path $p) {
      $vsDevCmd = $p
      break
    }
  }
  if ($null -eq $vsDevCmd) {
    throw "VS DevCmd not found. Install Visual Studio 2022 with C++ workload, or run from a VS Developer Prompt."
  }

  $envLines = cmd.exe /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && set"
  foreach ($line in $envLines) {
    if ($line -match "^(?<k>[^=]+)=(?<v>.*)$") {
      $key = $Matches["k"]
      $value = $Matches["v"]
      if (-not [string]::IsNullOrWhiteSpace($key)) {
        Set-Item -Path ("Env:{0}" -f $key) -Value $value
      }
    }
  }
}

if ($UseMsvc) {
  Import-MsvcDevEnv
}

function Invoke-BuildAndTest {
  param(
    [string]$BuildDir,
    [hashtable]$Definitions
  )

  $args = @("-S", $repoRoot, "-B", (Join-Path $repoRoot $BuildDir), "-G", $Generator)
  if ($Generator -eq "Ninja") {
    $args += ("-DCMAKE_BUILD_TYPE={0}" -f $Config)
  }
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
