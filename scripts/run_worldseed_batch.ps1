param(
  [string]$Generator = "Ninja",
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "RelWithDebInfo",
  [string]$BuildDir = "build_soak",
  [string]$WorldgenConfig = "data/worldgen/baselines/ecosystem_v1.toml",
  [UInt64]$SeedStart = 1000,
  [int]$SeedCount = 5,
  [UInt64]$Steps = 800,
  [UInt64]$Agents = 12,
  [UInt64]$WindowSteps = 100,
  [double]$StateQuantile = 0.75,
  [int]$MinWindows = 6,
  [switch]$UseMsvc,
  [switch]$Quiet
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

$batchId = Get-Date -Format "yyyyMMdd_HHmmss"
$worldgenConfigPath = Resolve-Path (Join-Path $repoRoot $WorldgenConfig)

$outWorldlineDir = Join-Path $repoRoot ("out\\worldline\\batch_{0}" -f $batchId)
$outSoakDir = Join-Path $repoRoot ("out\\soak\\batch_{0}" -f $batchId)
$outGeneratedDir = Join-Path $repoRoot ("out\\generated_worlds\\batch_{0}" -f $batchId)
$outAnalysisDir = Join-Path $repoRoot ("out\\worldline\\analysis")

Ensure-Dir (Join-Path $repoRoot "out")
Ensure-Dir $outWorldlineDir
Ensure-Dir $outSoakDir
Ensure-Dir $outGeneratedDir
Ensure-Dir $outAnalysisDir

Write-Host ("[batch] id={0} config={1} seeds=[{2}..{3}] steps={4} windowSteps={5}" -f $batchId, $worldgenConfigPath, $SeedStart, ($SeedStart + [UInt64]($SeedCount - 1)), $Steps, $WindowSteps)

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

for ($k = 0; $k -lt $SeedCount; $k++) {
  [UInt64]$seed = $SeedStart + [UInt64]$k
  $generatedWorld = Join-Path $outGeneratedDir ("seed_{0}" -f $seed)
  $worldlineOut = Join-Path $outWorldlineDir ("worldline_seed{0}.jsonl" -f $seed)
  $metricsOut = Join-Path $outSoakDir ("soak_seed{0}.json" -f $seed)
  $summaryOut = Join-Path $outSoakDir ("summary_seed{0}.md" -f $seed)

  $args = @(
    "soak",
    "--root", $repoRoot,
    "--steps", $Steps,
    "--agents", $Agents,
    "--worldgen-config", $worldgenConfigPath,
    "--seed", $seed,
    "--generated-world-out", $generatedWorld,
    "--worldline-out", $worldlineOut,
    "--window-steps", $WindowSteps,
    "--out", $metricsOut,
    "--summary-out", $summaryOut,
    "--quiet"
  )

  Write-Host ("[batch] seed={0}" -f $seed)
  & $exe @args | Out-Null
  if ($LASTEXITCODE -ne 0) {
    throw ("soak failed for seed={0} (exit={1})" -f $seed, $LASTEXITCODE)
  }
}

Write-Host ("[batch] worldlines: {0}" -f $outWorldlineDir)

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot "scripts\\analyze_worldline_motifs.ps1") `
  -InputPath $outWorldlineDir `
  -Pattern "worldline_seed*.jsonl" `
  -MinWindows $MinWindows `
  -TopEdges 30 `
  -TopMotifs 30 `
  -StateQuantile $StateQuantile | Write-Host

Write-Host ("[batch] analysis in: {0}" -f $outAnalysisDir)

