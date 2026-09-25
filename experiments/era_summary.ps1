# era_summary.ps1 -- per-era summary of a first_era_trace log.
#   powershell -NoProfile -ExecutionPolicy Bypass -File experiments\era_summary.ps1 -Log build\long2_L7.out [-Era 6]
# One era of the L=7 seed is 2*RMAX = 6 light frames, so era k ends at frame 6k.
# The sector split is read at the END of each era (the paper's convention); the flight and
# cohesion counters are summed over the era's frames, with the octant-aligned share (3 of 3).
# The second table is the disposition of those flight movers against the octant of their own charge
# word: how many are still on their octant's side of the lattice centre in all three axes ("on side")
# and how many have crossed it in at least one ("off side"), each with its aligned share.  That is the
# decomposition the README reads the drift mechanism from (the drift bucket concentrates the misaligned
# steps; it does not account for them).  Both counts come from the model's own annotations of the log
# (move-drift, move-split, move-mask), so nothing is re-derived here.
# -ExecutionPolicy Bypass is needed because the default policy refuses script files; the culture
# is pinned below because a pt-BR host would print the decimal separator as a comma.
param([Parameter(Mandatory=$true)][string]$Log, [int]$Era = 6)
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture
$pos = @{}; $ms = @{}; $dr = @{}; $mk = @{}; $cr = @{}; $wr = @{}; $tw2 = @{}; $vc = @{}
foreach ($line in Get-Content -LiteralPath $Log) {
  if ($line -match '^#\s+frame (\d+) positions: Orbis n=(\d+) mean=\(([-\d.]+),([-\d.]+),([-\d.]+)\) \| Umbra n=(\d+) mean=\(([-\d.]+),([-\d.]+),([-\d.]+)\) \| CoM-\(centre\)=\(([-\d.]+),([-\d.]+),([-\d.]+)\)') {
    $pos[[int]$Matches[1]] = [pscustomobject]@{ oy=[double]$Matches[4]; uy=[double]$Matches[8]
                                               cx=[double]$Matches[10]; cy=[double]$Matches[11]; cz=[double]$Matches[12] }
  }
  elseif ($line -match '^#\s+frame (\d+) move-split: flight=(\d+)\s+0/1/2/3 = (\d+)/(\d+)/(\d+)/(\d+)\s+\|\s+cohesion=(\d+)\s+0/1/2/3 = (\d+)/(\d+)/(\d+)/(\d+)') {
    $ms[[int]$Matches[1]] = [pscustomobject]@{ fl=[int]$Matches[2]; fl3=[int]$Matches[6]
                                               coh=[int]$Matches[7]; coh3=[int]$Matches[11] }
  }
  elseif ($line -match '^#\s+frame (\d+) move-drift: flight on its octant side \(all 3 axes\) = (\d+), octant-aligned = (\d+)\s+\|\s+off side \(<3 axes\) = (\d+), octant-aligned = (\d+)') {
    $dr[[int]$Matches[1]] = [pscustomobject]@{ on=[int]$Matches[2]; onAl=[int]$Matches[3]
                                               off=[int]$Matches[4]; offAl=[int]$Matches[5] }
  }
  elseif ($line -match '^#\s+frame (\d+) move-mask: one writer = (\d+) \(octant-aligned (\d+)\)\s+\|\s+two or more writers = (\d+) \(octant-aligned (\d+)\)') {
    $mk[[int]$Matches[1]] = [pscustomobject]@{ one=[int]$Matches[2]; oneAl=[int]$Matches[3]
                                               many=[int]$Matches[4]; manyAl=[int]$Matches[5] }
  }
  elseif ($line -match '^#\s+frame (\d+) move-carrier: queue-carried = (\d+) \(octant-aligned (\d+)\)\s+\|\s+booked this frame = (\d+) \(octant-aligned (\d+)\)\s+\|\s+both = (\d+) \(octant-aligned (\d+)\)\s+\|\s+no writer = (\d+) \(octant-aligned (\d+)\)') {
    $cr[[int]$Matches[1]] = [pscustomobject]@{ q=[int]$Matches[2]; qA=[int]$Matches[3]
                                               f=[int]$Matches[4]; fA=[int]$Matches[5]
                                               b=[int]$Matches[6]; bA=[int]$Matches[7]
                                               n=[int]$Matches[8]; nA=[int]$Matches[9] }
  }
  elseif ($line -match '^#\s+frame (\d+) move-thrustvec: follows the impulse = (\d+) \(octant-aligned (\d+)\)\s+\|\s+differs = (\d+) \(octant-aligned (\d+)\)\s+\|\s+thrust bit with no impulse = (\d+)\s+\|\s+impulse axes 3/2/1 = (\d+)/(\d+)/(\d+)') {
    $vc[[int]$Matches[1]] = [pscustomobject]@{ s=[int]$Matches[2]; sA=[int]$Matches[3]
                                               d=[int]$Matches[4]; dA=[int]$Matches[5]
                                               e=[int]$Matches[6]; a3=[int]$Matches[7]
                                               a2=[int]$Matches[8]; a1=[int]$Matches[9] }
  }
  elseif ($line -match '^#\s+frame (\d+) move-writer: walk = (\d+) \((\d+)\)\s+\|\s+relay = (\d+) \((\d+)\)\s+\|\s+relay-contact = (\d+) \((\d+)\)\s+\|\s+cohesion = (\d+) \((\d+)\)\s+\|\s+dispersion = (\d+) \((\d+)\)\s+\|\s+thrust = (\d+) \((\d+)\)') {
    $wr[[int]$Matches[1]] = [pscustomobject]@{ w=[int]$Matches[2]; wA=[int]$Matches[3]
                                               r=[int]$Matches[4]; rA=[int]$Matches[5]
                                               rc=[int]$Matches[6]; rcA=[int]$Matches[7]
                                               c=[int]$Matches[8]; cA=[int]$Matches[9]
                                               d=[int]$Matches[10]; dA=[int]$Matches[11]
                                               t=[int]$Matches[12]; tA=[int]$Matches[13] }
  }
  elseif ($line -match '^#\s+frame (\d+) move-thrust: word verified = (\d+) \(octant-aligned (\d+)\)\s+\|\s+word differs = (\d+) \(octant-aligned (\d+)\)(?:\s+\|\s+bookings, cumulative: calls = (\d+) \(aligned (\d+), partial (\d+), against (\d+)\))?') {
    $tw2[[int]$Matches[1]] = [pscustomobject]@{ v=[int]$Matches[2]; vA=[int]$Matches[3]
                                                x=[int]$Matches[4]; xA=[int]$Matches[5]
                                                calls=[int]$Matches[6]; bAl=[int]$Matches[7]
                                                bPa=[int]$Matches[8]; bAg=[int]$Matches[9] }
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

# Second table: the disposition of each era's flight movers against the octant of their own charge word,
# with the writer-mask split alongside.  This is the decomposition of README, "The twelve-era
# decomposition: the drift bucket concentrates it, and does not explain it".
Write-Output ""
Write-Output ("| era | flight moved | fully aligned | on its octant side (aligned) | off side (aligned) | one writer (aligned) | two or more (aligned) |")
Write-Output ("|---|---|---|---|---|---|---|")
$tt = @{ fl=0; fl3=0; on=0; onAl=0; off=0; offAl=0; one=0; oneAl=0; many=0; manyAl=0 }
for ($k = 1; $k -le $maxEra; $k++) {
  $lo = $Era * ($k - 1) + 1; $hi = $Era * $k
  $s = @{ fl=0; fl3=0; on=0; onAl=0; off=0; offAl=0; one=0; oneAl=0; many=0; manyAl=0 }
  for ($f = $lo; $f -le $hi; $f++) {
    if ($ms[$f]) { $s.fl += $ms[$f].fl; $s.fl3 += $ms[$f].fl3 }
    if ($dr[$f]) { $s.on += $dr[$f].on; $s.onAl += $dr[$f].onAl; $s.off += $dr[$f].off; $s.offAl += $dr[$f].offAl }
    if ($mk[$f]) { $s.one += $mk[$f].one; $s.oneAl += $mk[$f].oneAl
                   $s.many += $mk[$f].many; $s.manyAl += $mk[$f].manyAl }
  }
  foreach ($key in @('fl','fl3','on','onAl','off','offAl','one','oneAl','many','manyAl')) { $tt[$key] += $s[$key] }
  if ($s.fl -eq 0 -and $s.on -eq 0 -and $s.off -eq 0) { continue }
  Write-Output ("| {0} | {1} | {2} | {3} ({4}) | {5} ({6}) | {7} ({8}) | {9} ({10}) |" -f `
    $k, $s.fl, $s.fl3, $s.on, $s.onAl, $s.off, $s.offAl, $s.one, $s.oneAl, $s.many, $s.manyAl)
}
Write-Output ("| **all** | **{0}** | **{1}** | **{2} ({3})** | **{4} ({5})** | **{6} ({7})** | **{8} ({9})** |" -f `
  $tt.fl, $tt.fl3, $tt.on, $tt.onAl, $tt.off, $tt.offAl, $tt.one, $tt.oneAl, $tt.many, $tt.manyAl)
if ($tt.on -gt 0) {
  Write-Output ("on-side rows: {0} moved, {1} aligned = {2:N1}%   |   off-side rows: {3} moved, {4} aligned = {5:N1}%" -f `
    $tt.on, $tt.onAl, (100.0 * $tt.onAl / $tt.on), $tt.off, $tt.offAl, (100.0 * $tt.offAl / $tt.off))
}
if ($tt.fl -gt 0) {
  Write-Output ("flight rows over the eras: {0} moved, {1} aligned = {2:N1}%" -f $tt.fl, $tt.fl3, (100.0 * $tt.fl3 / $tt.fl))
}

# Third table: what CARRIED each flight displacement (the harness line `move-carrier`).  The classes
# are disjoint and cover every flight mover: queue = the pending queue's drain (bit 64, a booking made
# in an earlier frame landing now), frame = a writer fired in this frame, both, none = the center moved
# with neither.  This is the attribution the drift buckets could not give (README, "The carrier of the
# flight steps").
Write-Output ""
if ($cr.Count -eq 0) {
  Write-Output ("no move-carrier lines in " + $Log + ": the harness predates the carrier instrumentation")
  exit 0
}
Write-Output ("| era | queue-carried (aligned) | booked this frame (aligned) | both (aligned) | no writer (aligned) |")
Write-Output ("|---|---|---|---|---|")
$tc = @{ q=0; qA=0; f=0; fA=0; b=0; bA=0; n=0; nA=0 }
for ($k = 1; $k -le $maxEra; $k++) {
  $lo = $Era * ($k - 1) + 1; $hi = $Era * $k
  $s = @{ q=0; qA=0; f=0; fA=0; b=0; bA=0; n=0; nA=0 }
  for ($f = $lo; $f -le $hi; $f++) {
    if ($cr[$f]) { foreach ($key in @('q','qA','f','fA','b','bA','n','nA')) { $s[$key] += $cr[$f].$key } }
  }
  foreach ($key in @('q','qA','f','fA','b','bA','n','nA')) { $tc[$key] += $s[$key] }
  if (($s.q + $s.f + $s.b + $s.n) -eq 0) { continue }
  Write-Output ("| {0} | {1} ({2}) | {3} ({4}) | {5} ({6}) | {7} ({8}) |" -f `
    $k, $s.q, $s.qA, $s.f, $s.fA, $s.b, $s.bA, $s.n, $s.nA)
}
Write-Output ("| **all** | **{0} ({1})** | **{2} ({3})** | **{4} ({5})** | **{6} ({7})** |" -f `
  $tc.q, $tc.qA, $tc.f, $tc.fA, $tc.b, $tc.bA, $tc.n, $tc.nA)
$carTotal = $tc.q + $tc.f + $tc.b + $tc.n
if ($carTotal -ne $tt.fl) {
  Write-Output ("WARNING: the carriers add up to {0} but the flight channel moved {1} -- a class is missing or a line stopped parsing" -f $carTotal, $tt.fl)
}
foreach ($cls in @(@('queue-carried', 'q'), @('booked this frame', 'f'), @('both', 'b'), @('no writer', 'n'))) {
  $moved = $tc[$cls[1]]; $aligned = $tc[$cls[1] + 'A']
  if ($moved -gt 0) {
    Write-Output ("{0,-18} {1,4} moved, {2,4} aligned = {3,5:N1}%   unaligned: {4}" -f `
      ($cls[0] + ':'), $moved, $aligned, (100.0 * $aligned / $moved), ($moved - $aligned))
  }
}

# Fourth summary: WHICH writer booked the flight step (the harness line `move-writer`).  These columns
# do NOT partition the movers -- a layer several writers touched appears in each of their columns -- so
# each share below is a share of that writer's own movers.
Write-Output ""
if ($wr.Count -eq 0) {
  Write-Output ("no move-writer lines in " + $Log + ": the harness predates the per-writer split")
  exit 0
}
$tw = @{ w=0; wA=0; r=0; rA=0; rc=0; rcA=0; c=0; cA=0; d=0; dA=0; t=0; tA=0 }
foreach ($f in ($wr.Keys | Sort-Object)) { foreach ($key in @('w','wA','r','rA','rc','rcA','c','cA','d','dA','t','tA')) { $tw[$key] += $wr[$f].$key } }
Write-Output ("flight movers by writer bit (moved / octant-aligned / unaligned) over the whole log:")
foreach ($cls in @(@('walk funnel', 'w'), @('relay', 'r'), @('relay contact', 'rc'), @('cohesion table', 'c'), @('dispersion', 'd'), @('own-axis thrust', 't'))) {
  $moved = $tw[$cls[1]]; $aligned = $tw[$cls[1] + 'A']
  $pct = if ($moved -gt 0) { 100.0 * $aligned / $moved } else { 0.0 }
  Write-Output ("  {0,-16} {1,5} / {2,5} / {3,5}   aligned share {4,5:N1}%" -f `
    $cls[0], $moved, $aligned, ($moved - $aligned), $pct)
}

# Fifth summary: the thrust word probe (harness line `move-thrust`).  Of the flight movers the own-axis
# thrust booked, how many were thrust with the word the commit installs for that layer and how many
# with a different one.  If the off-octant steps are the second group, the residual is a word mismatch
# at the decision point rather than a wrong direction.
Write-Output ""
if ($tw2.Count -eq 0) {
  Write-Output ("no move-thrust lines in " + $Log + ": the harness predates the thrust word probe")
  exit 0
}
$tv = 0; $tvA = 0; $tx = 0; $txA = 0
foreach ($f in ($tw2.Keys | Sort-Object))
{
  $tv += $tw2[$f].v; $tvA += $tw2[$f].vA; $tx += $tw2[$f].x; $txA += $tw2[$f].xA
}
$tvUn = $tv - $tvA; $txUn = $tx - $txA
$tvPct = if ($tv -gt 0) { 100.0 * $tvA / $tv } else { 0.0 }
$txPct = if ($tx -gt 0) { 100.0 * $txA / $tx } else { 0.0 }
Write-Output ("thrust movers by the word the thrust READ (moved / octant-aligned / unaligned):")
Write-Output ("  word verified {0,6} / {1,6} / {2,6}   aligned share {3,5:N1}%" -f $tv, $tvA, $tvUn, $tvPct)
Write-Output ("  word differs  {0,6} / {1,6} / {2,6}   aligned share {3,5:N1}%" -f $tx, $txA, $txUn, $txPct)
Write-Output ("  of the {0} unaligned thrust movers, {1} read the committed word and {2} read a different one" -f `
  ($tvUn + $txUn), $tvUn, $txUn)
$lastFrame = ($tw2.Keys | Sort-Object | Select-Object -Last 1)
Write-Output ("thrust bookings, scored at the decision point against the COMMITTED word (cumulative to frame {0}):" -f $lastFrame)
Write-Output ("  calls {0} = aligned {1} + partial {2} + against {3}" -f `
  $tw2[$lastFrame].calls, $tw2[$lastFrame].bAl, $tw2[$lastFrame].bPa, $tw2[$lastFrame].bAg)

# Sixth summary: the written-impulse probe (harness line `move-thrustvec`, needs THRUST_VEC_TRACE).  Of the
# flight movers the own-axis thrust booked, those whose MEASURED centre displacement follows the vector the
# thrust wrote and those it does not.  With the impulse's axis count this is the decomposition of the
# unaligned population into its two components: a PARTIAL-axis impulse (fewer than three axes touched --
# the mover can follow it exactly and still fail a 3-of-3 test) and a RELOCATION (the displacement does not
# follow the impulse at all, so it is not the impulse).
Write-Output ""
if ($vc.Count -eq 0) {
  Write-Output ("no move-thrustvec lines in " + $Log + ": the harness was built without THRUST_VEC_TRACE")
  exit 0
}
$vs = 0; $vsA = 0; $vd = 0; $vdA = 0; $ve = 0; $va3 = 0; $va2 = 0; $va1 = 0
foreach ($f in ($vc.Keys | Sort-Object)) {
  $vs += $vc[$f].s; $vsA += $vc[$f].sA; $vd += $vc[$f].d; $vdA += $vc[$f].dA
  $ve += $vc[$f].e; $va3 += $vc[$f].a3; $va2 += $vc[$f].a2; $va1 += $vc[$f].a1
}
Write-Output ("thrust movers by whether the measured displacement follows the impulse that was written:")
Write-Output ("  follows {0,6} (octant-aligned {1,6})  |  differs {2,6} (octant-aligned {3,6})  |  thrust bit with no impulse {4,6}" -f `
  $vs, $vsA, $vd, $vdA, $ve)
Write-Output ("  impulse axes 3 / 2 / 1 = {0} / {1} / {2}" -f $va3, $va2, $va1)
Write-Output ("  unaligned thrust movers {0} = {1} following a partial-axis impulse + {2} relocations (displacement not following the impulse)" -f `
  (($vs - $vsA) + $vd), ($vs - $vsA), $vd)
