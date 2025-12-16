param(
  [Alias("Input")]
  [string]$InputPath = "out/worldline",
  [string]$Pattern = "worldline_*.jsonl",
  [int]$TopEdges = 30,
  [int]$TopMotifs = 40,
  [double]$DirectionMargin = 0.02,
  [double]$StateQuantile = 0.9,
  [int]$MinWindows = 6,
  [string]$Out = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$script = Join-Path $repoRoot "scripts\\analyze_worldline_motifs.py"

if (!(Test-Path $script)) {
  throw "Missing script: $script"
}

$argsList = @(
  $script,
  "--input", $InputPath,
  "--pattern", $Pattern,
  "--top-edges", $TopEdges,
  "--top-motifs", $TopMotifs,
  "--direction-margin", $DirectionMargin,
  "--state-quantile", $StateQuantile,
  "--min-windows", $MinWindows
)
if (-not [string]::IsNullOrWhiteSpace($Out)) {
  $argsList += @("--out", $Out)
}

& python @argsList | Write-Host
if ($LASTEXITCODE -ne 0) {
  throw ("analyze_worldline_motifs failed (exit={0})" -f $LASTEXITCODE)
}
