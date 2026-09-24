# scaling_summary.ps1 -- the finite-size table: one row per trace log, with the dimensionless
# observables the manuscript's "Limitations and open questions" asks for (Sect. 9.4, item iii:
# the plateau ratio K/W, the ledger's saturation, the charge-word roster) plus, for a
# configuration that moves, the era-1 dynamics (where the election lands, what the cascade did,
# where the freeze breaks).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File experiments\scaling_summary.ps1 `
#       -Logs build\scale_ref_L5.out,build\scale_ref_L7.out,...
#
# L and W are read from each log's own header, so a row is never labelled by hand.  The script
# prints two tables: the static observables (every configuration) and the era-1 dynamics (only
# for logs that move -- a log with no displacement and no election gets the static table only).

param([Parameter(Mandatory = $true)][string[]]$Logs)

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot

# The log prints decimals with a dot; the cultures differ (`-` vs `,` as decimal separator), so
# the parsing and the printing are pinned to the invariant culture, as in era_summary.ps1.
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture

function Num($line, [string]$pattern)
{
  if ($line -and $line -match $pattern) { return [int]$Matches[1] }
  return $null
}
function Dbl($line, [string]$pattern)
{
  if ($line -and $line -match $pattern) { return [double]$Matches[1] }
  return $null
}
function Get-FrameLines($lines, [string]$what)
{
  $out = @{}
  foreach ($ln in $lines)
  {
    if ($ln -match ('frame\s+(\d+)\s+' + $what + ':')) { $out[[int]$Matches[1]] = $ln }
  }
  return $out
}

$static = New-Object System.Collections.ArrayList
$dynamics = New-Object System.Collections.ArrayList

# `-File` hands the arguments over as plain strings, so a comma-separated list arrives as one
# element: split every element again.
$list = @()
foreach ($e in $Logs) { foreach ($one in ($e -split ',')) { if ($one.Trim()) { $list += $one.Trim() } } }

foreach ($rel in $list)
{
  $path = if ([System.IO.Path]::IsPathRooted($rel)) { $rel } else { Join-Path $root $rel }
  if (-not (Test-Path $path)) { Write-Host ("missing log: " + $rel) -ForegroundColor Red; continue }

  $lines = Get-Content $path
  $head = ($lines | Where-Object { $_ -match '^# first-era trace' } | Select-Object -First 1)
  if (-not $head) { Write-Host ("no trace header in " + $rel) -ForegroundColor Red; continue }
  $L = Num $head 'L=(\d+)'
  $W = Num $head 'W_USED=(\d+)'
  $RMAX = Num $head 'RMAX=(\d+)'
  $era = 2 * $RMAX

  $census = @()
  foreach ($ln in $lines)
  {
    if ($ln -notmatch '^\s+\d+\s+\d+\s+\d+') { continue }
    $t = @($ln -split '\s+' | Where-Object { $_ -ne '' })
    if ($t.Count -lt 19) { continue }
    $census += [pscustomobject]@{ Frame = [int]$t[0]; K = [int]$t[12]; D = [int]$t[13]
                                 S = [int]$t[14]; P = [int]$t[15]; Dt = [int]$t[17] }
  }
  $f2 = $census | Where-Object { $_.Frame -eq 2 } | Select-Object -First 1
  $frames = $census.Count

  # The roster summary is printed on its own line, right after the frame's word list:
  #   #   distinct words=8  addresses that admit a pair rule=56
  $words = ""; $pairable = ""
  $summary = ($lines | Where-Object { $_ -match 'distinct words=' } | Select-Object -First 1)
  if ($summary)
  {
    $words    = Num $summary 'distinct words=\s*(\d+)'
    $pairable = Num $summary 'pair rule=\s*(\d+)'
  }

  $period = ($lines | Where-Object { $_ -match '^# period of the full recorded state\s*:' } | Select-Object -First 1)
  $periodTxt = "not reported"
  if ($period -match ':\s*none') { $periodTxt = "none" }
  elseif ($period -match ':\s*(\S+)\s+frames') { $periodTxt = $Matches[1] + " frames" }

  $sep   = Get-FrameLines $lines 'separation'
  $aln   = Get-FrameLines $lines 'move-align'
  $clk   = Get-FrameLines $lines 'clocks'
  $pos   = Get-FrameLines $lines 'positions'

  $disp = 0
  foreach ($f in $clk.Keys) { $d = Num $clk[$f] 'charge-dispersion=(\d+)'; if ($d -and $d -gt $disp) { $disp = $d } }
  $firstM = 0
  foreach ($f in ($sep.Keys | Sort-Object)) { if ((Num $sep[$f] 'm!=0\(sources\)=(\d+)/') -gt 0) { $firstM = $f; break } }
  $offPlateau = 0; $offK = ""; $offD = ""
  foreach ($c in ($census | Where-Object { $_.Frame -ge 2 }))
  {
    if ($c.K -ne $W - 8 -or $c.D -ne 8) { $offPlateau = $c.Frame; $offK = $c.K; $offD = $c.D; break }
  }
  $eraEnd = $L - 1
  # Era 1 = frames 1..L-1.  The movers are counted from the move-align lines (flight + cohesion,
  # the way the README's "(a)/(b)" tables count them) and bucketed by how many of the three
  # signs agree with the layer's own octant.
  $eraMoved = 0; $eraAligned = 0; $peakFrame = 0; $peakMoved = 0; $peakAligned = 0
  foreach ($f in ($aln.Keys | Where-Object { $_ -ge 2 -and $_ -le $eraEnd }))
  {
    $mv = Num $aln[$f] 'moved=(\d+)'
    $al = Num $aln[$f] 'sign-match 0/1/2/3 = \d+/\d+/\d+/(\d+)'
    if ($null -ne $mv) { $eraMoved += $mv }
    if ($null -ne $al) { $eraAligned += $al }
    # The peak of the era's own cascade: the dispersal frame (2) is excluded on purpose, since
    # it dwarfs everything else and says nothing about the encounter's transport.
    if ($f -ge 3 -and $null -ne $mv -and $mv -gt $peakMoved)
      { $peakMoved = $mv; $peakFrame = $f; $peakAligned = $al }
  }
  $eraAlignedPct = ""
  if ($eraMoved -gt 0) { $eraAlignedPct = ("{0:F1}" -f (100.0 * $eraAligned / $eraMoved)) }
  $offEra = ""
  if ($offPlateau -gt 0) { $offEra = [math]::Ceiling($offPlateau / $era) }
  $gap = $null
  if ($pos[$eraEnd] -match 'Orbis n=\d+ mean=\([-\d.]+,\s*([-\d.]+),[^)]*\) \| Umbra n=\d+ mean=\([-\d.]+,\s*([-\d.]+),')
    { $gap = [double]$Matches[2] - [double]$Matches[1] }

  [void]$static.Add([pscustomobject]@{
    Log = [System.IO.Path]::GetFileName($rel); L = $L; W = $W; Frames = $frames
    K = $f2.K; D = $f2.D; S = $f2.S; P = $f2.P
    Sum = $f2.K + $f2.D; KW = [math]::Round(100.0 * $f2.K / $W, 2); DW = [math]::Round(100.0 * $f2.D / $W, 2)
    Words = $words; Pairable = $pairable; Period = $periodTxt })

  if ($disp -gt 0 -or $firstM -gt 0)
  {
    [void]$dynamics.Add([pscustomobject]@{
      Log = [System.IO.Path]::GetFileName($rel); L = $L; Era = $era
      Dispersal = $disp; AlignedAtF2 = (Num $aln[2] 'sign-match 0/1/2/3 = \d+/\d+/\d+/(\d+)')
      Elected = $firstM
      OffPlateau = $offPlateau; OffEra = $offEra; OffK = $offK; OffD = $offD
      Era1Movers = $eraMoved; Era1AlignedPct = $eraAlignedPct
      PeakFrame = $peakFrame; PeakMoved = $peakMoved; PeakAligned = $peakAligned
      GapAtEraEnd = if ($null -eq $gap) { "" } else { ("{0:F3}" -f $gap) } })
  }
}

Write-Host ""
Write-Host "Static observables (frame 2, and the roster of the same frame; K+D should be W = 3L^2)"
$static | Format-Table Log, L, W, Frames, K, D, S, P, Sum, KW, DW, Words, Pairable, Period -AutoSize

if ($dynamics.Count)
{
  Write-Host "Era-1 dynamics (only configurations that move; era = 2 RMAX = L-1 frames here)"
  $dynamics | Format-Table Log, L, Era, Dispersal, AlignedAtF2, Elected, OffPlateau, OffEra, OffK,
    OffD, Era1Movers, Era1AlignedPct, PeakFrame, PeakMoved, PeakAligned, GapAtEraEnd -AutoSize
}
else { Write-Host "No log in this set moves, so there is no dynamics table." }
