param(
  [string]$Generator = "Ninja",
  [string]$Config = "Release",
  [string]$BuildDir = "build_baseline_core",
  [switch]$UseMsvc,
  [ValidateSet("default", "short", "long")]
  [string]$Preset = "default",
  [UInt64]$Steps = 5000,
  [UInt64]$Seed = 1337
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot $BuildDir

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

if (-not $PSBoundParameters.ContainsKey("Steps")) {
  switch ($Preset) {
    "short" { $Steps = 800 }
    "long" { $Steps = 20000 }
    default { }
  }
}

Write-Host ("[baseline] preset={0} steps={1} seed={2} config={3}" -f $Preset, $Steps, $Seed, $Config)

function Ensure-Dir([string]$Path) {
  New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

Ensure-Dir (Join-Path $repoRoot "out")
Ensure-Dir (Join-Path $repoRoot "out\\baselines")
Ensure-Dir (Join-Path $repoRoot "out\\metrics")
Ensure-Dir (Join-Path $repoRoot "out\\baseline_scripts")

$cmakeArgs = @("-S", $repoRoot, "-B", $buildPath, "-G", $Generator)
$cmakeArgs += "-DGENESISENGINE_ENABLE_TESTS=ON"
$cmakeArgs += "-DGENESIS_BUILD_GUI=OFF"
$cmakeArgs += "-DGENESIS_WITH_STYLE=OFF"
if ($Generator -eq "Ninja") {
  $cmakeArgs += ("-DCMAKE_BUILD_TYPE={0}" -f $Config)
}

& cmake @cmakeArgs | Write-Host

& cmake --build $buildPath --parallel --config $Config --target genesis_runtime_cli | Write-Host

$exe = Join-Path $buildPath "src\\genesis-runtime-cli.exe"
if (!(Test-Path $exe)) {
  throw "Missing executable: $exe"
}

$baselines = @(
  @{ Name = "scarcity";  ConfigPath = "data/worldgen/baselines/scarcity.toml";  Seed = $Seed; Agents = 1 },
  @{ Name = "diversity"; ConfigPath = "data/worldgen/baselines/diversity.toml"; Seed = $Seed; Agents = 6 },
  @{ Name = "abundance"; ConfigPath = "data/worldgen/baselines/abundance.toml"; Seed = $Seed; Agents = 1 },
  @{ Name = "crowding";  ConfigPath = "data/worldgen/baselines/crowding.toml";  Seed = $Seed; Agents = 12 },
  @{ Name = "ecosystem_v1"; ConfigPath = "data/worldgen/baselines/ecosystem_v1.toml"; Seed = $Seed; Agents = 8 },
  @{ Name = "ecosystem_v2"; ConfigPath = "data/worldgen/baselines/ecosystem_v2.toml"; Seed = $Seed; Agents = 10 },
  @{ Name = "substitution_v2"; ConfigPath = "data/worldgen/baselines/substitution_v2.toml"; Seed = $Seed; Agents = 10 },
  @{ Name = "logistics_v2"; ConfigPath = "data/worldgen/baselines/logistics_v2.toml"; Seed = $Seed; Agents = 10 },
  @{ Name = "tool_feedback_v3"; ConfigPath = "data/worldgen/baselines/tool_feedback_v3.toml"; Seed = $Seed; Agents = 12 },
  @{ Name = "ecology_chain_v1"; ConfigPath = "data/worldgen/baselines/ecology_chain_v1.toml"; Seed = $Seed; Agents = 12 }
)

foreach ($b in $baselines) {
  $name = $b.Name
  $configRel = $b.ConfigPath
  $seed = [UInt64]$b.Seed
  $agents = [UInt64]$b.Agents

  $worldOut = Join-Path $repoRoot ("out\\baselines\\{0}_seed{1}" -f $name, $seed)
  Ensure-Dir $worldOut

  $scriptPath = Join-Path $repoRoot ("out\\baseline_scripts\\worldgen_{0}_seed{1}.json" -f $name, $seed)
  $commands = @(
    @{
      action = "world.db.generate"
      configPath = $configRel
      seed = $seed
      outputFolder = $worldOut
      waitForSuccess = $true
    }
  )
  $script = @{
    name = ("baseline-worldgen-{0}" -f $name)
    commands = $commands
  }
  ($script | ConvertTo-Json -Depth 6) | Out-File -FilePath $scriptPath -Encoding utf8

  & $exe run-script $scriptPath --root $repoRoot --max-steps 128 --quiet | Write-Host
  if ($LASTEXITCODE -ne 0) {
    throw ("worldgen failed for baseline '{0}' (exit={1})" -f $name, $LASTEXITCODE)
  }

  $metricsOut = Join-Path $repoRoot ("out\\metrics\\soak_{0}_seed{1}_steps{2}.json" -f $name, $seed, $Steps)
  $summaryOut = Join-Path $repoRoot ("out\\metrics\\summary_{0}_seed{1}_steps{2}.md" -f $name, $seed, $Steps)
  $soakArgs = @("soak", $worldOut, "--root", $repoRoot, "--steps", $Steps, "--out", $metricsOut, "--summary-out", $summaryOut, "--quiet")
  if ($agents -gt 0) {
    $soakArgs += @("--agents", $agents)
  }
  & $exe @soakArgs | Write-Host
  if ($LASTEXITCODE -ne 0) {
    throw ("soak failed for baseline '{0}' (exit={1})" -f $name, $LASTEXITCODE)
  }

  Write-Host ("[baseline] {0} -> {1}" -f $name, $metricsOut)
}
