# inherit_summary.ps1 -- fold for the IMPULSE_NO_INHERIT probe (RESEARCH_LOG.md, "The inherited-impulse probe").
#   powershell -NoProfile -ExecutionPolicy Bypass -File experiments\inherit_summary.ps1 `
#     -Probe build\inherit_L7.out -Baseline build\both_L7_repro.out
# Compares two first_era_trace logs of the same configuration, one built with -D IMPULSE_NO_INHERIT and
# one without, frame by frame, on the lines the displacement-channel question is read from: the flight
# alignment (move-split), the drift buckets (move-drift), the writer mask (move-mask) and the probe's own
# counter reseed-carried-reloc.  Prints the per-frame table, the first frame at which anything differs,
# and whether the two files are byte-identical -- which is the claim the RESEARCH_LOG.md makes.  The probe build is
# the `inherit` variant of experiments\trace_build_variants.bat.
#
# NB: the parsed tables must not be called $P/$B while the paths are $Probe/$Baseline -- PowerShell
# variable names are case-insensitive, so a table named $probe would overwrite the path.  Hence $tp/$tb.
param([string]$Probe = 'build\inherit_L7.out', [string]$Baseline = 'build\both_L7_repro.out',
      [int]$FromFrame = 1, [int]$ToFrame = 999)
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture

$newRow = { @{ fl=0; fl3=0; coh=0; on=0; onAl=0; off=0; offAl=0; one=0; many=0; carried=0 } }
$tables = @{}
foreach ($side in @($Probe, $Baseline))
{
  $table = @{}
  foreach ($ln in (Get-Content -LiteralPath $side))
  {
    $f = 0; $r = $null
    if ($ln -match 'frame (\d+) move-split: flight=(\d+)\s+0/1/2/3 = (\d+)/(\d+)/(\d+)/(\d+)\s+\|\s+cohesion=(\d+)\s+0/1/2/3 = (\d+)/')
    { $f = [int]$Matches[1]; $r = @{ fl=[int]$Matches[2]; fl3=[int]$Matches[6]; coh=[int]$Matches[7] } }
    elseif ($ln -match 'frame (\d+) move-drift: flight on its octant side \(all 3 axes\) = (\d+), octant-aligned = (\d+)\s+\|\s+off side \(<3 axes\) = (\d+), octant-aligned = (\d+)')
    { $f = [int]$Matches[1]; $r = @{ on=[int]$Matches[2]; onAl=[int]$Matches[3]; off=[int]$Matches[4]; offAl=[int]$Matches[5] } }
    elseif ($ln -match 'frame (\d+) move-mask: one writer = (\d+) \(octant-aligned (\d+)\)\s+\|\s+two or more writers = (\d+) \(octant-aligned (\d+)\)')
    { $f = [int]$Matches[1]; $r = @{ one=[int]$Matches[2]; many=[int]$Matches[4] } }
    elseif ($ln -match 'frame (\d+) clocks:.*reseat-at-contact=(\d+)\s+\|\s+reseed-carried-reloc=(\d+)')
    { $f = [int]$Matches[1]; $r = @{ carried=[int]$Matches[3] } }
    if ($f -gt 0)
    {
      if (-not $table.ContainsKey($f)) { $table[$f] = & $newRow }
      foreach ($k in $r.Keys) { $table[$f][$k] = $r[$k] }
    }
  }
  $tables[$side] = $table
}
$tp = $tables[$Probe]
$tb = $tables[$Baseline]
Write-Host ("probe    " + $Probe + "  (" + $tp.Count + " frames)")
Write-Host ("baseline " + $Baseline + "  (" + $tb.Count + " frames)")
$ha = (Get-FileHash -LiteralPath $Probe -Algorithm SHA256).Hash
$hb = (Get-FileHash -LiteralPath $Baseline -Algorithm SHA256).Hash
Write-Host ("byte-identical: " + ($ha -eq $hb) + "   sha256 " + $ha)

$form = "flight {0,3} aligned {1,3} | on {2,3}({3,3}) off {4,3}({5,3}) | one {6,3} many {7,3} | carried {8,3}"
$firstDiff = 'none'; $carriedTotal = 0
foreach ($f in (($tp.Keys + $tb.Keys | Sort-Object -Unique) | Where-Object { $_ -ge $FromFrame -and $_ -le $ToFrame }))
{
  $x = $tp[$f]; $y = $tb[$f]
  $same = $true
  foreach ($k in @('fl','fl3','coh','on','onAl','off','offAl','one','many','carried'))
  {
    if ($x[$k] -ne $y[$k]) { $same = $false }
  }
  $carriedTotal += $x.carried
  if (-not $same -and $firstDiff -eq 'none') { $firstDiff = $f }
  Write-Host (("{0,5} | " -f $f) + ($form -f $x.fl,$x.fl3,$x.on,$x.onAl,$x.off,$x.offAl,$x.one,$x.many,$x.carried) +
              " | " + ($form -f $y.fl,$y.fl3,$y.on,$y.onAl,$y.off,$y.offAl,$y.one,$y.many,$y.carried) +
              $(if (-not $same) { '   <-- DIFFERS' }))
}
Write-Host ("first differing frame: " + $firstDiff + "   |   probe total reseed-carried-reloc: " + $carriedTotal)
