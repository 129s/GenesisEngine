param(
  [string]$Generator = "Ninja",
  [string]$Config = "Release",
  [string]$BuildDir = "build_baseline_core",
  [UInt64]$Steps = 5000,
  [UInt64]$Seed = 1337
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot $BuildDir

function Ensure-Dir([string]$Path) {
  New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

Ensure-Dir (Join-Path $repoRoot "out")
Ensure-Dir (Join-Path $repoRoot "out\\baselines")
Ensure-Dir (Join-Path $repoRoot "out\\metrics")
Ensure-Dir (Join-Path $repoRoot "out\\baseline_scripts")

& cmake -S $repoRoot -B $buildPath -G $Generator `
  -DGENESISENGINE_ENABLE_TESTS=ON `
  -DGENESIS_BUILD_GUI=OFF `
  -DGENESIS_WITH_STYLE=OFF | Write-Host

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
  @{ Name = "logistics_v2"; ConfigPath = "data/worldgen/baselines/logistics_v2.toml"; Seed = $Seed; Agents = 10 }
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
  $soakArgs = @("soak", $worldOut, "--root", $repoRoot, "--steps", $Steps, "--out", $metricsOut, "--quiet")
  if ($agents -gt 0) {
    $soakArgs += @("--agents", $agents)
  }
  & $exe @soakArgs | Write-Host
  if ($LASTEXITCODE -ne 0) {
    throw ("soak failed for baseline '{0}' (exit={1})" -f $name, $LASTEXITCODE)
  }

  Write-Host ("[baseline] {0} -> {1}" -f $name, $metricsOut)
}
