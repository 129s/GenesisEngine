param(
  [string]$Generator = "Ninja",
  [string]$Config = "Release",
  [string]$BuildDir = "build_regression_core",
  [switch]$SkipRuntimeCliScript,
  [string]$RuntimeCliScript = "data/scripts/world_cycle.json",
  [UInt64]$RuntimeCliMaxSteps = 256,
  [UInt64]$RuntimeCliAfterSteps = 32
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot $BuildDir

function Ensure-Dir([string]$Path) {
  New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

$args = @("-S", $repoRoot, "-B", $buildPath, "-G", $Generator)
$args += "-DGENESISENGINE_ENABLE_TESTS=ON"
$args += "-DGENESIS_BUILD_GUI=OFF"
$args += "-DGENESIS_WITH_STYLE=OFF"

& cmake @args | Write-Host
& cmake --build $buildPath --parallel --config $Config | Write-Host
& ctest --test-dir $buildPath --output-on-failure -C $Config | Write-Host

if (-not $SkipRuntimeCliScript) {
  & cmake --build $buildPath --parallel --config $Config --target genesis_runtime_cli | Write-Host

  $exe = Join-Path $buildPath "src\\genesis-runtime-cli.exe"
  if (!(Test-Path $exe)) {
    throw "Missing executable: $exe"
  }

  Ensure-Dir (Join-Path $repoRoot "out")
  Ensure-Dir (Join-Path $repoRoot "out\\regression")
  Ensure-Dir (Join-Path $repoRoot "out\\regression\\scripts")

  $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
  $backupFolder = Join-Path $repoRoot ("out\\regression\\world_new_backup_{0}" -f $timestamp)

  $scriptPath = Resolve-Path (Join-Path $repoRoot $RuntimeCliScript)
  if (!(Test-Path $scriptPath)) {
    throw "Missing runtime-cli script: $scriptPath"
  }

  $scriptJson = Get-Content -Raw $scriptPath | ConvertFrom-Json
  if ($null -eq $scriptJson.commands) {
    throw "Invalid runtime-cli script (expected object with 'commands'): $scriptPath"
  }

  foreach ($cmd in $scriptJson.commands) {
    if ($cmd.action -eq "world.db.save") {
      $cmd.folder = $backupFolder
    }
  }

  $patchedScriptPath = Join-Path $repoRoot ("out\\regression\\scripts\\world_cycle_{0}.json" -f $timestamp)
  ($scriptJson | ConvertTo-Json -Depth 16) | Out-File -FilePath $patchedScriptPath -Encoding utf8

  & $exe run-script $patchedScriptPath --root $repoRoot --max-steps $RuntimeCliMaxSteps --after-steps $RuntimeCliAfterSteps | Write-Host
  if ($LASTEXITCODE -ne 0) {
    throw ("runtime-cli run-script failed (exit={0})" -f $LASTEXITCODE)
  }
}
