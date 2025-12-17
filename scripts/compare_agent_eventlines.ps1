param(
  [Parameter(Mandatory = $true)]
  [string]$Input,
  [string]$Pattern = "eventline_seed*.jsonl",
  [int]$WindowSteps = 100,
  [int]$TopTurns = 6,
  [string]$Out = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$script = Join-Path $repoRoot "scripts\\compare_agent_eventlines.py"

if (!(Test-Path $script)) {
  throw "Missing script: $script"
}

$argsList = @(
  $script,
  "--input", $Input,
  "--pattern", $Pattern,
  "--window-steps", $WindowSteps,
  "--top-turns", $TopTurns
)

if (-not [string]::IsNullOrWhiteSpace($Out)) {
  $argsList += @("--out", $Out)
}

& python @argsList | Write-Host
if ($LASTEXITCODE -ne 0) {
  throw ("compare_agent_eventlines failed (exit={0})" -f $LASTEXITCODE)
}

