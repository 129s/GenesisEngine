param(
  [string]$Generator = "Ninja",
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "RelWithDebInfo",
  [string]$BuildDir = "build_soak",
  [string]$WorldgenConfig = "data/worldgen/baselines/tool_feedback_v3.toml",
  [UInt64]$SeedStart = 1000,
  [int]$SeedCount = 8,
  [UInt64]$Steps = 5000,
  [UInt64]$Agents = 12,
  [UInt64]$WorldlineWindowSteps = 100,
  [int]$EventlineAgentIndex = 0,
  [int]$LensWindowSteps = 100,
  [int]$TopTurns = 6,
  [int]$MaxEvents = 600,
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

$outRoot = Join-Path $repoRoot ("out\\observability\\batch_{0}" -f $batchId)
$outWorldlineDir = Join-Path $outRoot "worldline"
$outEventlineDir = Join-Path $outRoot "eventline"
$outSoakDir = Join-Path $outRoot "soak"
$outReportsDir = Join-Path $outRoot "reports"
$outGeneratedDir = Join-Path $outRoot "generated_worlds"

Ensure-Dir (Join-Path $repoRoot "out")
Ensure-Dir (Join-Path $repoRoot "out\\observability")
Ensure-Dir $outRoot
Ensure-Dir $outWorldlineDir
Ensure-Dir $outEventlineDir
Ensure-Dir $outSoakDir
Ensure-Dir $outReportsDir
Ensure-Dir $outGeneratedDir

Write-Host ("[obs-batch] id={0} config={1} seeds=[{2}..{3}] steps={4} agents={5}" -f $batchId, $worldgenConfigPath, $SeedStart, ($SeedStart + [UInt64]($SeedCount - 1)), $Steps, $Agents)
Write-Host ("[obs-batch] worldlineWindowSteps={0} eventlineAgentIndex={1} lensWindowSteps={2}" -f $WorldlineWindowSteps, $EventlineAgentIndex, $LensWindowSteps)

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

$chronicleScript = Join-Path $repoRoot "scripts\\chronicle_worldline.py"
$observeScript = Join-Path $repoRoot "scripts\\observe_agent_eventline.py"
$compareScript = Join-Path $repoRoot "scripts\\compare_agent_eventlines.py"

if (!(Test-Path $chronicleScript)) { throw "Missing script: $chronicleScript" }
if (!(Test-Path $observeScript)) { throw "Missing script: $observeScript" }
if (!(Test-Path $compareScript)) { throw "Missing script: $compareScript" }

for ($k = 0; $k -lt $SeedCount; $k++) {
  [UInt64]$seed = $SeedStart + [UInt64]$k

  $generatedWorld = Join-Path $outGeneratedDir ("seed_{0}" -f $seed)
  $worldlineOut = Join-Path $outWorldlineDir ("worldline_seed{0}.jsonl" -f $seed)
  $eventlineOut = Join-Path $outEventlineDir ("eventline_seed{0}_agentIndex{1}.jsonl" -f $seed, $EventlineAgentIndex)
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
    "--window-steps", $WorldlineWindowSteps,
    "--eventline-out", $eventlineOut,
    "--eventline-agent-index", $EventlineAgentIndex,
    "--out", $metricsOut,
    "--summary-out", $summaryOut
  )

  if ($Quiet) {
    $args += "--quiet"
  }

  Write-Host ("[obs-batch] seed={0}" -f $seed)
  & $exe @args | Write-Host
  if ($LASTEXITCODE -ne 0) {
    throw ("soak failed for seed={0} (exit={1})" -f $seed, $LASTEXITCODE)
  }

  $worldlineMd = Join-Path $outReportsDir ("chronicle_seed{0}.md" -f $seed)
  & python $chronicleScript --input $worldlineOut --top-turns $TopTurns --out $worldlineMd | Write-Host
  if ($LASTEXITCODE -ne 0) { throw ("chronicle_worldline failed for seed={0} (exit={1})" -f $seed, $LASTEXITCODE) }

  $eventlineMd = Join-Path $outReportsDir ("agent_lens_seed{0}_agentIndex{1}.md" -f $seed, $EventlineAgentIndex)
  & python $observeScript --eventline $eventlineOut --max-events $MaxEvents --out $eventlineMd | Write-Host
  if ($LASTEXITCODE -ne 0) { throw ("observe_agent_eventline failed for seed={0} (exit={1})" -f $seed, $LASTEXITCODE) }
}

$batchChronicle = Join-Path $outReportsDir ("chronicle_batch_{0}.md" -f $batchId)
& python $chronicleScript --input $outWorldlineDir --pattern "worldline_seed*.jsonl" --top-turns $TopTurns --out $batchChronicle | Write-Host
if ($LASTEXITCODE -ne 0) { throw ("chronicle_worldline batch failed (exit={0})" -f $LASTEXITCODE) }

$batchCompare = Join-Path $outReportsDir ("compare_eventline_agentIndex{0}_batch_{1}.md" -f $EventlineAgentIndex, $batchId)
& python $compareScript --input $outEventlineDir --pattern ("eventline_seed*_agentIndex{0}.jsonl" -f $EventlineAgentIndex) --window-steps $LensWindowSteps --top-turns $TopTurns --out $batchCompare | Write-Host
if ($LASTEXITCODE -ne 0) { throw ("compare_agent_eventlines batch failed (exit={0})" -f $LASTEXITCODE) }

Write-Host ("[obs-batch] outRoot: {0}" -f $outRoot)
Write-Host ("[obs-batch] reports: {0}" -f $outReportsDir)

