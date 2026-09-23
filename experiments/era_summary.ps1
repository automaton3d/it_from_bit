# era_summary.ps1 -- per-era summary of a first_era_trace log.
#   powershell -NoProfile -ExecutionPolicy Bypass -File experiments\era_summary.ps1 -Log build\long2_L7.out [-Era 6]
# One era of the L=7 seed is 2*RMAX = 6 light frames, so era k ends at frame 6k.
# The sector split is read at the END of each era (the paper's convention); the flight and
# cohesion counters are summed over the era's frames, with the octant-aligned share (3 of 3).
# -ExecutionPolicy Bypass is needed because the default policy refuses script files; the culture
# is pinned below because a pt-BR host would print the decimal separator as a comma.
param([Parameter(Mandatory=$true)][string]$Log, [int]$Era = 6)
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
$pos = @{}; $ms = @{}
foreach ($line in Get-Content -LiteralPath $Log) {
  if ($line -match '^#\s+frame (\d+) positions: Orbis n=(\d+) mean=\(([-\d.]+),([-\d.]+),([-\d.]+)\) \| Umbra n=(\d+) mean=\(([-\d.]+),([-\d.]+),([-\d.]+)\) \| CoM-\(centre\)=\(([-\d.]+),([-\d.]+),([-\d.]+)\)') {
    $pos[[int]$Matches[1]] = [pscustomobject]@{ oy=[double]$Matches[4]; uy=[double]$Matches[8]
                                               cx=[double]$Matches[10]; cy=[double]$Matches[11]; cz=[double]$Matches[12] }
  }
  elseif ($line -match '^#\s+frame (\d+) move-split: flight=(\d+)\s+0/1/2/3 = (\d+)/(\d+)/(\d+)/(\d+)\s+\|\s+cohesion=(\d+)\s+0/1/2/3 = (\d+)/(\d+)/(\d+)/(\d+)') {
    $ms[[int]$Matches[1]] = [pscustomobject]@{ fl=[int]$Matches[2]; fl3=[int]$Matches[6]
                                               coh=[int]$Matches[7]; coh3=[int]$Matches[11] }
  }
}
$frames = ($pos.Keys | Sort-Object)
if (-not $frames) { Write-Output "no frames parsed in $Log"; exit 1 }
$maxEra = [math]::Floor(($frames[-1]) / $Era)
Write-Output ("| era | end frame | Orbis y | Umbra y | max|CoM| | cascade frame: flight moved / aligned | era sums: flight moved/aligned, cohesion moved/aligned |")
Write-Output ("|---|---|---|---|---|---|---|")
for ($k = 1; $k -le $maxEra; $k++) {
  $lo = $Era * ($k - 1) + 1; $hi = $Era * $k
  $p = $pos[$hi]; if (-not $p) { continue }
  $fl = 0; $fl3 = 0; $coh = 0; $coh3 = 0; $cmax = 0.0; $cfl = -1; $cfl3 = 0; $cbest = -1
  for ($f = $lo; $f -le $hi; $f++) {
    if ($ms[$f]) { $fl += $ms[$f].fl; $fl3 += $ms[$f].fl3; $coh += $ms[$f].coh; $coh3 += $ms[$f].coh3
                   if ($ms[$f].fl -gt $cbest) { $cbest = $ms[$f].fl; $cfl = $f; $cfl3 = $ms[$f].fl3 } }
    if ($pos[$f]) { $m = [math]::Sqrt($pos[$f].cx*$pos[$f].cx + $pos[$f].cy*$pos[$f].cy + $pos[$f].cz*$pos[$f].cz); if ($m -gt $cmax) { $cmax = $m } }
  }
  $casc = if ($cfl -ge 0) { ('f' + $cfl + ': ' + $cbest + ' / ' + $cfl3) } else { '-' }
  Write-Output ("| {0} | {1} | {2:+0.000;-0.000} | {3:+0.000;-0.000} | {4:0.000} | {5} | {6}/{7}, {8}/{9} |" -f $k, $hi, $p.oy, $p.uy, $cmax, $casc, $fl, $fl3, $coh, $coh3)
}
Write-Output ("frames parsed: {0} (1..{1})" -f $frames.Count, $frames[-1])
