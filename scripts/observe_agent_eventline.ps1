param(
  [Parameter(Mandatory = $true)]
  [string]$Eventline,
  [string]$Out = "",
  [int]$FromStep = -1,
  [int]$ToStep = -1,
  [int]$MaxEvents = 600
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$script = Join-Path $repoRoot "scripts\\observe_agent_eventline.py"

if (!(Test-Path $script)) {
  throw "Missing script: $script"
}

$argsList = @(
  $script,
  "--eventline", $Eventline,
  "--max-events", $MaxEvents
)

if ($FromStep -ge 0) {
  $argsList += @("--from-step", $FromStep)
}
if ($ToStep -ge 0) {
  $argsList += @("--to-step", $ToStep)
}
if (-not [string]::IsNullOrWhiteSpace($Out)) {
  $argsList += @("--out", $Out)
}

& python @argsList | Write-Host
if ($LASTEXITCODE -ne 0) {
  throw ("observe_agent_eventline failed (exit={0})" -f $LASTEXITCODE)
}

