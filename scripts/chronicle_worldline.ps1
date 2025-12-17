param(
  [Alias("Input")]
  [string]$InputPath = "out/worldline",
  [string]$Pattern = "worldline_*.jsonl",
  [int]$TopTurns = 8,
  [int]$KFeatures = 8,
  [switch]$KeepPartials,
  [string]$Out = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$script = Join-Path $repoRoot "scripts\\chronicle_worldline.py"

if (!(Test-Path $script)) {
  throw "Missing script: $script"
}

$argsList = @(
  $script,
  "--input", $InputPath,
  "--pattern", $Pattern,
  "--top-turns", $TopTurns,
  "--k-features", $KFeatures
)
if ($KeepPartials) {
  $argsList += "--keep-partials"
}
if (-not [string]::IsNullOrWhiteSpace($Out)) {
  $argsList += @("--out", $Out)
}

& python @argsList | Write-Host
if ($LASTEXITCODE -ne 0) {
  throw ("chronicle_worldline failed (exit={0})" -f $LASTEXITCODE)
}
