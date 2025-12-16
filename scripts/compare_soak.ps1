param(
  [Parameter(Mandatory = $true)]
  [string]$Base,
  [Parameter(Mandatory = $true)]
  [string]$Cand
)

$ErrorActionPreference = "Stop"

function Read-Json([string]$Path) {
  if (!(Test-Path $Path)) {
    throw "Missing file: $Path"
  }
  return (Get-Content -Raw $Path | ConvertFrom-Json)
}

function Get-Num($Obj, [string]$Path) {
  $cur = $Obj
  foreach ($p in ($Path -split "\\.")) {
    if ($null -eq $cur) { return $null }
    if ($cur.PSObject.Properties.Name -contains $p) {
      $cur = $cur.$p
    } else {
      return $null
    }
  }
  if ($cur -is [ValueType] -or $cur -is [double] -or $cur -is [int] -or $cur -is [long]) {
    return [double]$cur
  }
  return $null
}

function Print-Row([string]$Name, $A, $B) {
  if ($null -eq $A -and $null -eq $B) {
    return
  }
  $delta = $null
  $pct = $null
  if ($null -ne $A -and $null -ne $B) {
    $delta = $B - $A
    if ([Math]::Abs($A) -gt 1e-9) {
      $pct = 100.0 * ($delta / $A)
    }
  }

  $aStr = if ($null -eq $A) { "n/a" } else { "{0:N4}" -f $A }
  $bStr = if ($null -eq $B) { "n/a" } else { "{0:N4}" -f $B }
  $dStr = if ($null -eq $delta) { "n/a" } else { "{0:+N4;-N4;0.0000}" -f $delta }
  $pStr = if ($null -eq $pct) { "n/a" } else { "{0:+0.00;-0.00;0.00}%%" -f $pct }

  "{0,-40} base={1,12} cand={2,12} delta={3,12} pct={4,10}" -f $Name, $aStr, $bStr, $dStr, $pStr | Write-Host
}

$basePath = $Base
$candPath = $Cand

$baseReport = Read-Json $basePath
$candReport = Read-Json $candPath

Write-Host ("[soak-compare] base={0}" -f (Resolve-Path $basePath))
Write-Host ("[soak-compare] cand={0}" -f (Resolve-Path $candPath))
Write-Host ""

Print-Row "steps" (Get-Num $baseReport "steps") (Get-Num $candReport "steps")
Print-Row "elapsedSeconds" (Get-Num $baseReport "elapsedSeconds") (Get-Num $candReport "elapsedSeconds")
Print-Row "agentCount" (Get-Num $baseReport "agentCount") (Get-Num $candReport "agentCount")
Print-Row "worldVersion" (Get-Num $baseReport "worldVersion") (Get-Num $candReport "worldVersion")

Write-Host ""
Print-Row "diversity.actionTypes.entropy_bits" (Get-Num $baseReport "diversity.actionTypes.entropy_bits") (Get-Num $candReport "diversity.actionTypes.entropy_bits")
Print-Row "diversity.plannerTargets.entropy_bits" (Get-Num $baseReport "diversity.plannerTargets.entropy_bits") (Get-Num $candReport "diversity.plannerTargets.entropy_bits")

Write-Host ""
Print-Row "resourceEconomy.totalProduced" (Get-Num $baseReport "resourceEconomy.totalProduced") (Get-Num $candReport "resourceEconomy.totalProduced")
Print-Row "resourceEconomy.totalConsumed" (Get-Num $baseReport "resourceEconomy.totalConsumed") (Get-Num $candReport "resourceEconomy.totalConsumed")
Print-Row "resourceEconomy.totalDecayed" (Get-Num $baseReport "resourceEconomy.totalDecayed") (Get-Num $candReport "resourceEconomy.totalDecayed")
Print-Row "resourceEconomy.stockoutStepsSources" (Get-Num $baseReport "resourceEconomy.stockoutStepsSources") (Get-Num $candReport "resourceEconomy.stockoutStepsSources")
Print-Row "resourceEconomy.stockoutStepsWorkshops" (Get-Num $baseReport "resourceEconomy.stockoutStepsWorkshops") (Get-Num $candReport "resourceEconomy.stockoutStepsWorkshops")
Print-Row "resourceEconomy.avgUtilization" (Get-Num $baseReport "resourceEconomy.avgUtilization") (Get-Num $candReport "resourceEconomy.avgUtilization")
Print-Row "summary.resourceEconomy.avgOscillation" (Get-Num $baseReport "summary.resourceEconomy.avgOscillation") (Get-Num $candReport "summary.resourceEconomy.avgOscillation")

Write-Host ""
Print-Row "summary.production.failedTotal" (Get-Num $baseReport "summary.production.failedTotal") (Get-Num $candReport "summary.production.failedTotal")
Print-Row "summary.resourceAttempts.failedTotal" (Get-Num $baseReport "summary.resourceAttempts.failedTotal") (Get-Num $candReport "summary.resourceAttempts.failedTotal")
