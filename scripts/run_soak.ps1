param(
  [string]$Generator = "Ninja",
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "RelWithDebInfo",
  [string]$BuildDir = "build_soak",
  [ValidateSet("default", "short", "overnight", "24h")]
  [string]$Preset = "default",
  [string]$WorldFolder = "data/world_multiagent",
  [UInt64]$Steps = 0,
  [UInt64]$Agents = 12,
  [double]$WallHours = 0,
  [UInt64]$ProgressEvery = 50000,
  [switch]$Worldline,
  [UInt64]$WindowSteps = 5000,
  [switch]$Quiet,
  [switch]$UseMsvc
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot $BuildDir

function Ensure-Dir([string]$Path) {
  New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

$cpmCache = $env:CPM_SOURCE_CACHE
if ([string]::IsNullOrWhiteSpace($cpmCache)) {
  $cpmCache = Join-Path $repoRoot ".cpmcache"
  $env:CPM_SOURCE_CACHE = $cpmCache
}
Ensure-Dir $cpmCache

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

if (-not $PSBoundParameters.ContainsKey("Steps") -or $Steps -eq 0) {
  switch ($Preset) {
    "short" { $Steps = 800 }
    "overnight" { $Steps = 200000 }
    "24h" { $Steps = 999999999 }
    default { $Steps = 5000 }
  }
}

[UInt64]$wallSeconds = 0
if ($WallHours -gt 0) {
  $wallSeconds = [UInt64]([Math]::Floor($WallHours * 3600.0))
} elseif ($Preset -eq "24h") {
  $wallSeconds = 86400
}

Write-Host ("[soak] preset={0} world={1} steps={2} wallSeconds={3} agents={4} config={5}" -f $Preset, $WorldFolder, $Steps, $wallSeconds, $Agents, $Config)

Ensure-Dir (Join-Path $repoRoot "out")
Ensure-Dir (Join-Path $repoRoot "out\\soak")
Ensure-Dir (Join-Path $repoRoot "out\\worldline")

$cmakeArgs = @("-S", $repoRoot, "-B", $buildPath, "-G", $Generator)
$cmakeArgs += "-DGENESISENGINE_ENABLE_TESTS=OFF"
$cmakeArgs += "-DGENESIS_BUILD_GUI=OFF"
$cmakeArgs += "-DGENESIS_WITH_STYLE=OFF"
$cmakeArgs += ("-DCMAKE_BUILD_TYPE={0}" -f $Config)

& cmake @cmakeArgs | Write-Host

& cmake --build $buildPath --parallel --config $Config --target genesis_runtime_cli | Write-Host

$exe = Join-Path $buildPath "src\\genesis-runtime-cli.exe"
if (!(Test-Path $exe)) {
  throw "Missing executable: $exe"
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$metricsOut = Join-Path $repoRoot ("out\\soak\\soak_{0}_steps{1}.json" -f $timestamp, $Steps)
$summaryOut = Join-Path $repoRoot ("out\\soak\\summary_{0}_steps{1}.md" -f $timestamp, $Steps)
$worldlineOut = Join-Path $repoRoot ("out\\worldline\\worldline_{0}_steps{1}.jsonl" -f $timestamp, $Steps)

$soakArgs = @("soak", $WorldFolder, "--root", $repoRoot, "--steps", $Steps, "--out", $metricsOut, "--summary-out", $summaryOut)
if ($Agents -gt 0) {
  $soakArgs += @("--agents", $Agents)
}
if ($wallSeconds -gt 0) {
  $soakArgs += @("--wall-seconds", $wallSeconds)
}
if ($ProgressEvery -gt 0) {
  $soakArgs += @("--progress-every", $ProgressEvery)
}
if ($Worldline) {
  $soakArgs += @("--worldline-out", $worldlineOut, "--window-steps", $WindowSteps)
}
if ($Quiet) {
  $soakArgs += "--quiet"
}

& $exe @soakArgs | Write-Host
if ($LASTEXITCODE -ne 0) {
  throw ("soak failed (exit={0})" -f $LASTEXITCODE)
}

Write-Host ("[soak] wrote: {0}" -f $metricsOut)
if ($Worldline) {
  Write-Host ("[soak] wrote: {0}" -f $worldlineOut)
}
