param(
  [Parameter(Mandatory = $true)][string]$A,
  [Parameter(Mandatory = $true)][string]$B,
  [int]$Top = 12,
  [double]$JsThreshold = 0.15
)

$ErrorActionPreference = "Stop"

function Read-Worldline([string]$Path) {
  if (!(Test-Path $Path)) { throw "Missing file: $Path" }

  $meta = $null
  $baseline = $null
  $windows = @()

  foreach ($line in Get-Content -LiteralPath $Path) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $obj = $line | ConvertFrom-Json
    switch ($obj.kind) {
      "runtime_worldline_meta" { $meta = $obj; continue }
      "runtime_worldline_baseline" { $baseline = $obj; continue }
      "runtime_worldline_window" { $windows += $obj; continue }
      default { continue }
    }
  }

  return [pscustomobject]@{
    Path = $Path
    Meta = $meta
    Baseline = $baseline
    Windows = $windows
  }
}

function Get-CountMap($countsObj) {
  $map = @{}
  if ($null -eq $countsObj) { return $map }
  foreach ($p in $countsObj.PSObject.Properties) {
    $v = [double]$p.Value
    if ($v -gt 0) { $map[$p.Name] = $v }
  }
  return $map
}

function JsDivergenceBits($a, $b) {
  # a,b: hashtable key->count (non-negative)
  $keys = New-Object System.Collections.Generic.HashSet[string]
  foreach ($k in $a.Keys) { [void]$keys.Add([string]$k) }
  foreach ($k in $b.Keys) { [void]$keys.Add([string]$k) }

  $sumA = 0.0
  foreach ($v in $a.Values) { $sumA += [double]$v }
  $sumB = 0.0
  foreach ($v in $b.Values) { $sumB += [double]$v }
  if ($sumA -le 0 -and $sumB -le 0) { return 0.0 }
  if ($sumA -le 0 -or $sumB -le 0) { return 1.0 } # maximal mismatch under this coarse view

  function KldBits($pMap, $pSum, $mMap, $mSum, $keys) {
    $kld = 0.0
    foreach ($k in $keys) {
      $p = 0.0
      if ($pMap.ContainsKey($k)) { $p = $pMap[$k] / $pSum }
      if ($p -le 0) { continue }
      $m = 0.0
      if ($mMap.ContainsKey($k)) { $m = $mMap[$k] / $mSum }
      if ($m -le 0) { continue }
      $kld += $p * ([Math]::Log($p / $m, 2))
    }
    return $kld
  }

  $m = @{}
  foreach ($k in $keys) {
    $av = 0.0
    if ($a.ContainsKey($k)) { $av = $a[$k] }
    $bv = 0.0
    if ($b.ContainsKey($k)) { $bv = $b[$k] }
    $m[$k] = 0.5 * ($av + $bv)
  }
  $sumM = 0.0
  foreach ($v in $m.Values) { $sumM += [double]$v }

  $kldA = KldBits $a $sumA $m $sumM $keys
  $kldB = KldBits $b $sumB $m $sumM $keys
  return 0.5 * ($kldA + $kldB)
}

$wa = Read-Worldline $A
$wb = Read-Worldline $B

Write-Host ("[worldline] A windows={0} B windows={1}" -f $wa.Windows.Count, $wb.Windows.Count)
if ($wa.Meta -and $wb.Meta) {
  Write-Host ("[worldline] windowSteps A={0} B={1}" -f $wa.Meta.windowSteps, $wb.Meta.windowSteps)
}

$n = [Math]::Min($wa.Windows.Count, $wb.Windows.Count)
if ($n -le 0) { throw "No windows to compare." }

$rows = @()
for ($i = 0; $i -lt $n; $i++) {
  $aWin = $wa.Windows[$i]
  $bWin = $wb.Windows[$i]

  $aActions = Get-CountMap $aWin.counts.actionTypes
  $bActions = Get-CountMap $bWin.counts.actionTypes
  $aTargets = Get-CountMap $aWin.counts.plannerTargets
  $bTargets = Get-CountMap $bWin.counts.plannerTargets

  $jsAction = JsDivergenceBits $aActions $bActions
  $jsTarget = JsDivergenceBits $aTargets $bTargets

  $rows += [pscustomobject]@{
    index = $i
    stepStart = $aWin.stepStart
    stepEnd = $aWin.stepEnd
    jsActionBits = [Math]::Round($jsAction, 4)
    jsTargetBits = [Math]::Round($jsTarget, 4)
    dCriticalNeedRate = [Math]::Round(([double]$bWin.metrics.criticalNeedRate - [double]$aWin.metrics.criticalNeedRate), 6)
    dStockoutShareAny = [Math]::Round(([double]$bWin.metrics.stockoutShareAny - [double]$aWin.metrics.stockoutShareAny), 6)
    dActionEntropyBits = [Math]::Round(([double]$bWin.metrics.actionEntropyBits - [double]$aWin.metrics.actionEntropyBits), 4)
    dTargetEntropyBits = [Math]::Round(([double]$bWin.metrics.plannerTargetEntropyBits - [double]$aWin.metrics.plannerTargetEntropyBits), 4)
    dPlannerTravelCostMean = [Math]::Round(([double]$bWin.metrics.plannerTravelCostMean - [double]$aWin.metrics.plannerTravelCostMean), 4)
  }
}

$interesting = $rows | Where-Object { $_.jsActionBits -ge $JsThreshold -or $_.jsTargetBits -ge $JsThreshold }
if ($interesting.Count -gt 0) {
  Write-Host ("[worldline] windows with JS >= {0}: {1}/{2}" -f $JsThreshold, $interesting.Count, $rows.Count)
}

$rows |
  Sort-Object -Property @{ Expression = { $_.jsActionBits + $_.jsTargetBits }; Descending = $true } |
  Select-Object -First $Top |
  Format-Table -AutoSize

