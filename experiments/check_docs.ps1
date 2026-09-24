# check_docs.ps1 -- the documents against the artifacts (`nmake check-docs`).
#
# The repository rests on quoted numbers and on paths that point at evidence, so this script
# checks the three kinds of claim that CAN be checked mechanically, and then says explicitly
# what it does not check -- the coverage is never assumed to be wider than it is.
#
#   A. cited paths.  Every path the documents quote (README.md, FSM.txt, the Makefile, the
#      manuscript, the build scripts and the source comments) resolves to a file in this tree,
#      or is declared as living elsewhere -- the README's "lives in" table, or
#      experiments\check_docs.allow -- or is a generated artifact (build\, obj\).
#   B. registry vs sources.  Every macro of the FSM.txt registry (section 9) is defined by an
#      #ifdef/#ifndef in the sources, and every macro the Makefile turns on exists.  The other
#      direction -- macros the sources carry that the registry does not name -- is reported as
#      INFO with its count: the registry is a curated list, not an inventory, and the number is
#      printed so the difference stays visible instead of growing unnoticed.
#   C. invariants vs logs.  The machine-checkable rows of FSM.txt section 10 (and the candidate
#      rows the README quotes from the same logs) are recomputed from the trace logs in build\.
#      A log that is not in the working tree is reported as NOT BUILT, not as a failure: build\
#      is generated and never committed, so a fresh clone legitimately has none of them.
#
# NOT checked: the prose of the manuscript and of the README beyond the rows encoded in C, the
# LaTeX build, and anything that needs the study archive (E:\alpha), which is not part of this
# repository.  The rows of C are listed with the document and section they come from, so a
# failing row points at the sentence that has to be re-scoped, not only at the number.
#
# Exit code: 0 when no ERROR was raised, 1 otherwise.  WARN and INFO never fail.
#
# Usage, from the repository root:
#   powershell -NoProfile -ExecutionPolicy Bypass -File experiments\check_docs.ps1

param([switch]$Quiet)

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot

$script:errors = 0; $script:warns = 0; $script:infos = 0

function Report([string]$kind, [string]$text)
{
  switch ($kind)
  {
    'ERROR' { $script:errors++; Write-Host ("ERROR  " + $text) -ForegroundColor Red }
    'WARN'  { $script:warns++;  Write-Host ("WARN   " + $text) -ForegroundColor Yellow }
    'INFO'  { $script:infos++;  Write-Host ("INFO   " + $text) }
    'OK'    { if (-not $Quiet) { Write-Host ("ok     " + $text) -ForegroundColor DarkGreen } }
  }
}

function Read-Text([string]$relPath)
{
  $p = Join-Path $root $relPath
  if (Test-Path $p) { return [System.IO.File]::ReadAllText($p) }
  return $null
}

# The project's own sources: everything under src\, glad\ and experiments\ except the vendored
# libraries (glm, zlib, glad, GLFW, KHR, stb_*), whose #ifdefs are not the model's.
function Get-OwnSources
{
  Get-ChildItem (Join-Path $root 'src'), (Join-Path $root 'glad'), (Join-Path $root 'experiments') `
      -Recurse -File -Include *.cpp, *.h, *.inc, *.cu |
    Where-Object {
      $_.FullName -notmatch '\\include\\(glm|zlib|glad|GLFW|KHR)\\' -and
      $_.Name -notmatch '^stb_'
    }
}

# ======================================================================================
# A. cited paths
# ======================================================================================
# The documents that quote evidence paths.  The sources are included on purpose: the
# references that rot are exactly the ones written in a code comment.
$docPaths = @('README.md', 'FSM.txt', 'Makefile', 'it_from_bit.tex', 'build_gui.bat', 'build.bat')
$docPaths += (Get-OwnSources | ForEach-Object { $_.FullName.Substring($root.Length + 1) })

# Declared as living elsewhere: the README's "lives in" table (by file name), plus
# experiments\check_docs.allow (glob patterns, one per line).
$declared = @{}
$inTable = $false
foreach ($line in ((Read-Text 'README.md') -split "`r?`n"))
{
  if ($line -match '\|\s*quoted in the text\s*\|\s*lives in\s*\|') { $inTable = $true; continue }
  if ($inTable)
  {
    if ($line -notmatch '^\s*\|') { $inTable = $false; continue }
    foreach ($m in [regex]::Matches($line, '[A-Za-z0-9_./\\-]+\.(md|csv|py|c|cpp|ps1)'))
      { $declared[[System.IO.Path]::GetFileName($m.Value.Replace('/','\'))] = 'README "lives in"' }
  }
}
$allowPath = Join-Path $root 'experiments\check_docs.allow'
if (Test-Path $allowPath)
{
  foreach ($line in (Get-Content $allowPath))
  {
    $t = ($line -replace '#.*$', '').Trim()
    if ($t) { $declared[$t] = 'check_docs.allow' }
  }
}

# A cited path, in either separator, but not one that belongs to another tree (E:\alpha\...).
$pathRe = [regex]'(?<![\w:\\/])(?:src|experiments|attic|build|obj|glad|fonts|lib)[\\/][A-Za-z0-9_.\\/<>*%-]+'
$cited = [ordered]@{}
foreach ($rel in $docPaths)
{
  $t = Read-Text $rel
  if ($null -eq $t) { continue }
  $t = $t.Replace('/', '\').Replace('\_', '_')   # LaTeX escapes its underscores
  foreach ($m in $pathRe.Matches($t))
  {
    # A brace set (odr_gate_tu{1,2}.cpp) or a template (cmdline_<name>.txt) is not a path.
    $ng = $m.Index + $m.Length
    if ($ng -lt $t.Length -and $t[$ng] -match '[\{*<>]') { continue }
    $p = $m.Value.TrimEnd('\', '.', ',', ';', ':', ')', ']', '"', "'")
    $p = $p -replace '\\[nrt0]+$', ''               # C string escapes glued to the name
    if ($p -match '[*<>%{}]' -or $p -match '\$') { continue }
    if (-not $cited.Contains($p)) { $cited[$p] = @() }
    if ($cited[$p] -notcontains $rel) { $cited[$p] += $rel }
  }
}

$missing = 0; $generated = 0; $external = 0; $prose = 0
foreach ($p in $cited.Keys)
{
  if (Test-Path (Join-Path $root $p)) { continue }
  $top = ($p -split '\\')[0].ToLower()
  if ($top -eq 'build' -or $top -eq 'obj') { $generated++; continue }
  $base = [System.IO.Path]::GetFileName($p)
  $hit = $false
  foreach ($k in $declared.Keys) { if ($base -like $k -or $p -like $k) { $hit = $true; break } }
  if ($hit) { $external++; continue }
  $where = "   (cited in: " + (($cited[$p] | Sort-Object) -join ', ') + ")"
  if ($base -match '\.')   # a file name that is not there and is not declared
  {
    $missing++
    Report 'ERROR' ("cited path missing and not declared elsewhere: " + $p + $where)
  }
  else                     # no extension: a directory, or a mention of one in prose
  {
    $prose++
    Report 'WARN' ("cited directory missing -- check by hand, prose may be intended: " + $p + $where)
  }
}
Report 'OK' ("A. cited paths: " + $cited.Count + " distinct, " + $generated + " generated, " +
             $external + " declared elsewhere, " + $missing + " missing, " + $prose + " to check by hand")

# ======================================================================================
# B. the FSM.txt registry against the sources
# ======================================================================================
$fsm = (Read-Text 'FSM.txt') -split "`r?`n"

# The registry is the table of section 9: from its header line to the "Build:" block below it.
$regStart = -1; $regEnd = -1
for ($i = 0; $i -lt $fsm.Count; $i++)
{
  if ($regStart -lt 0 -and $fsm[$i] -match '^\s*macro\s+what it changes\s+status\s*$') { $regStart = $i; continue }
  if ($regStart -ge 0 -and $fsm[$i] -match '^\s*Build:\s') { $regEnd = $i; break }
}
$registry = [ordered]@{}
$statusTokens = @('[CANDIDATE]', '[PROBE]', 'measurement', 'not built')
if ($regStart -lt 0 -or $regEnd -lt 0)
{
  Report 'ERROR' 'B. registry: the section 9 table header or its "Build:" block was not found in FSM.txt'
}
else
{
  for ($i = $regStart + 1; $i -lt $regEnd; $i++)
  {
    if ($fsm[$i] -match '^    ([A-Z][A-Z0-9_]{3,})\s{2,}(\S.*)$') { $registry[$Matches[1]] = $fsm[$i] }
  }
}

# A macro is "tested by the sources" either as #ifdef/#ifndef NAME or inside a compound
# condition, defined(NAME) -- POLAR_SEED_FROM_PLACEMENT is only ever the latter.  The
# parentheses are required on purpose: bare "defined" in prose ("defined in ...") would
# capture the next word.
$macroRe = [regex]'#\s*ifn?def\s+([A-Za-z_][A-Za-z0-9_]*)|defined\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)'
$srcMacros = @{}
foreach ($f in Get-OwnSources)
{
  foreach ($m in $macroRe.Matches([System.IO.File]::ReadAllText($f.FullName)))
  {
    $name = $m.Groups[1].Value
    if (-not $name) { $name = $m.Groups[2].Value }
    if ($name) { $srcMacros[$name] = $true }
  }
}

# Include guards and toolchain switches are not model mechanisms.
$noise  = '^(__|_WIN32$|DEBUG$|NOMINMAX$|GLAPIENTRY$|USE_CUDA$|CUDA_BRIDGE_CU$|GLFW_|KHRONOS_|_glfw)'
$guards = '_H$|_H_$|_H__$'
$ownMacros = @($srcMacros.Keys | Where-Object { $_ -notmatch $noise -and $_ -notmatch $guards } | Sort-Object)

$regMissing = 0; $regNoStatus = 0
foreach ($name in $registry.Keys)
{
  if (-not $srcMacros.ContainsKey($name))
  {
    $regMissing++
    Report 'ERROR' ("B. registry macro has no #ifdef in the sources: " + $name)
  }
  elseif (($statusTokens | Where-Object { $registry[$name].Contains($_) }).Count -eq 0)
  {
    $regNoStatus++
    Report 'WARN' ("B. registry row carries no status token ([CANDIDATE]/[PROBE]/measurement/not built): " + $name)
  }
}

$mkMacros = @([regex]::Matches((Read-Text 'Makefile'), '/D\s+"([A-Za-z_][A-Za-z0-9_]*)"') |
              ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
$mkMissing = 0
foreach ($name in $mkMacros)
{
  if (-not $srcMacros.ContainsKey($name))
  {
    $mkMissing++
    Report 'ERROR' ("B. the Makefile defines a macro the sources never test: " + $name)
  }
}

Report 'OK' ("B. registry: " + $registry.Count + " entries, " + $regMissing + " without an #ifdef, " +
             $regNoStatus + " without a status token; Makefile /D switches: " + ($mkMacros -join ' ') +
             " (" + $mkMissing + " missing)")
$notInReg = @($ownMacros | Where-Object { -not $registry.Contains($_) })
Report 'INFO' ("B. the sources carry " + $notInReg.Count + " other macros the registry does not name " +
               "(the registry is curated; listed so the difference stays visible, not as a defect):")
Report 'INFO' ("   " + ($notInReg -join ' '))

# ======================================================================================
# C. the machine-checkable invariants against the trace logs
# ======================================================================================
function Read-TraceLog([string]$name)
{
  $p = Join-Path $root ('build\' + $name)
  if (Test-Path $p) { return (Get-Content $p) }
  return $null
}

# The per-frame table row: frame r_clk active shell_rf lost gain lost_s2B lost_K lost_S
# lost_D lost_P s2B_tot K D S P_halves dev dt t_share -- the 19 columns of FSM.txt section 8.
function Get-Census($lines)
{
  $out = @()
  foreach ($l in $lines)
  {
    if ($l -notmatch '^\s+\d+\s+\d+\s+\d+') { continue }
    $t = @($l -split '\s+' | Where-Object { $_ -ne '' })
    if ($t.Count -lt 19) { continue }
    $out += [pscustomobject]@{ Frame = [int]$t[0]; K = [int]$t[12]; D = [int]$t[13]
                               S = [int]$t[14]; P = [int]$t[15]; Dt = [int]$t[17] }
  }
  return $out
}

# The annotated "# frame N <what>: ..." lines, keyed by frame.
function Get-FrameLines($lines, [string]$what)
{
  $out = @{}
  foreach ($l in $lines)
  {
    if ($l -match ('frame\s+(\d+)\s+' + $what + ':')) { $out[[int]$Matches[1]] = $l }
  }
  return $out
}

# The gap of a frame: Umbra's mean y minus Orbis's, from the positions line.
function Get-Gap($lines)
{
  $pos = Get-FrameLines $lines 'positions'
  $gap = @{}
  foreach ($f in $pos.Keys)
  {
    if ($pos[$f] -match 'Orbis n=\d+ mean=\([-\d.]+,\s*([-\d.]+),[^)]*\) \| Umbra n=\d+ mean=\([-\d.]+,\s*([-\d.]+),')
      { $gap[$f] = [double]$Matches[2] - [double]$Matches[1] }
  }
  return $gap
}

function Val($line, [string]$pattern)
{
  if ($line -and $line -match $pattern) { return [int]$Matches[1] }
  return $null
}

function Dbl($line, [string]$pattern)
{
  if ($line -and $line -match $pattern) { return [double]$Matches[1] }
  return $null
}

$script:ruleBad = @()
function Want([string]$frameText, [string]$what, $actual, $expected)
{
  if ($null -eq $actual) { $script:ruleBad += ($frameText + ": cannot read " + $what) }
  elseif ($actual -ne $expected)
    { $script:ruleBad += ($frameText + ": " + $what + " = " + $actual + ", documented " + $expected) }
}
function WantNear([string]$frameText, [string]$what, $actual, $expected, [double]$tol)
{
  if ($null -eq $actual) { $script:ruleBad += ($frameText + ": cannot read " + $what) }
  elseif ([math]::Abs([double]$actual - [double]$expected) -gt $tol)
    { $script:ruleBad += ($frameText + ": " + $what + " = " + $actual + ", documented " + $expected) }
}

# Every rule quotes the document it verifies, as a fragment checked against the document as
# well as against the log.  Without that, the rule's expectation is a private copy and a
# re-wording of the document -- the thing a lint is supposed to notice -- would pass silently.
function Report-Rule([string]$name, [string]$docFiles, [string]$docText, [string]$logName, [scriptblock]$check)
{
  $found = $false
  foreach ($fn in ($docFiles -split ','))
  {
    $t = Read-Text $fn.Trim()
    if ($t -and (($t -replace '\s+', ' ').Contains($docText))) { $found = $true }
  }
  if (-not $found)
  {
    Report 'ERROR' ("C. " + $name + ": the claim this rule verifies is no longer written in " +
                    $docFiles + " -- update the document, or this rule and its numbers: """ + $docText + """")
  }

  $lines = Read-TraceLog $logName
  if ($null -eq $lines)
  {
    Report 'INFO' ("C. " + $name + " -- NOT BUILT here (build\" + $logName + " is absent)")
    return
  }
  $script:ruleBad = @()
  & $check $lines
  if ($script:ruleBad.Count -eq 0)
    { Report 'OK' ("C. " + $name + "   [build\" + $logName + " | " + $docFiles + "]") }
  else
    { foreach ($b in $script:ruleBad) { Report 'ERROR' ("C. " + $name + " " + $b) } }
}

# --- C1: the reference configuration never moves and never elects -----------------------
Report-Rule 'reference invariants' 'FSM.txt' `
  'every frame, reference m == 0 on all W layers; 0 displacements; dt = 1 (<= 42 frames)' `
  'long2ref_L7.out' {
  param($lines)
  $census = Get-Census $lines
  if ($census.Count -ne 42) { $script:ruleBad += ("42 frames documented, the log has " + $census.Count) }
  foreach ($c in ($census | Where-Object { $_.Frame -ge 2 }))
  {
    Want ("frame " + $c.Frame) 'K' $c.K 139
    Want ("frame " + $c.Frame) 'D' $c.D 8
    Want ("frame " + $c.Frame) 'S' $c.S 0
    Want ("frame " + $c.Frame) 'P' $c.P 0
    Want ("frame " + $c.Frame) 'dt' $c.Dt 1
  }
  $sep = Get-FrameLines $lines 'separation'
  $clk = Get-FrameLines $lines 'clocks'
  foreach ($f in $sep.Keys)
  {
    Want ("frame " + $f) 'layers carrying m != 0' (Val $sep[$f] 'm!=0\(sources\)=(\d+)/') 0
    Want ("frame " + $f) 'displacements' (Val $sep[$f] 'moved-this-frame=(\d+)') 0
  }
  foreach ($f in $clk.Keys)
    { Want ("frame " + $f) 'charge-dispersion' (Val $clk[$f] 'charge-dispersion=(\d+)') 0 }
}

# --- C2: the candidate's alignment, which decays ----------------------------------------
Report-Rule 'candidate m-alignment' 'FSM.txt' `
  'm alignment (candidate) 147/147 from frame 4 to frame 14, then decaying: 119 at 18, 113 at 24, 74 at 38, 56 at 54, 39 at 72 (L=7)' `
  'long2_L7.out' {
  param($lines)
  $axis = Get-FrameLines $lines 'axis-align'
  foreach ($f in 4..14)
  {
    Want ("frame " + $f) 'layers with m == 0' (Val $axis[$f] 'm==0=(\d+)') 0
    Want ("frame " + $f) 'layers fully aligned' (Val $axis[$f] 'sign-match 0/1/2/3 = \d+/\d+/\d+/(\d+)') 147
  }
  $decay = @{ 18 = 119; 24 = 113; 38 = 74; 54 = 56; 72 = 39 }
  foreach ($f in ($decay.Keys | Sort-Object))
    { Want ("frame " + $f) 'layers fully aligned' (Val $axis[$f] 'sign-match 0/1/2/3 = \d+/\d+/\d+/(\d+)') $decay[$f] }
}

# --- C3: the twelve era-end gaps, i.e. the README's twelve-era table --------------------
Report-Rule 'twelve-era gaps' 'FSM.txt' `
  'twelve eras (candidate) gap 3.06 -> 1.42 (era 4) -> floors 0.2-0.75; one negative window, frames 65-67' `
  'long2_L7.out' {
  param($lines)
  $gap = Get-Gap $lines
  $era = @{ 6 = 3.063; 12 = 2.883; 18 = 1.881; 24 = 1.420; 30 = 0.372; 36 = 0.409
            42 = 0.606; 48 = 0.402; 54 = 0.403; 60 = 0.746; 66 = -0.194; 72 = 0.220 }
  foreach ($f in ($era.Keys | Sort-Object))
    { WantNear ("era " + ($f / 6) + " (frame " + $f + ")") 'gap' $gap[$f] $era[$f] 0.005 }
  $inverted = @($gap.Keys | Where-Object { $gap[$_] -lt 0 } | Sort-Object)
  if (($inverted -join ',') -ne '65,66,67')
    { $script:ruleBad += ("the inverted frames are " + ($inverted -join ',') + ", documented 65,66,67") }
}

# --- C4: the dispersion alone at L=7 (the `disp` variant) --------------------------------
Report-Rule 'dispersion alone, L=7' 'FSM.txt' `
  'from f2 on the ledger holds at K=139, D=8, dt=1 in every frame (f1 is the seed: K=D=0, S=W)' `
  'disp_L7.out' {
  param($lines)
  $census = Get-Census $lines
  if ($census.Count -ne 24) { $script:ruleBad += ("24 frames documented, the log has " + $census.Count) }
  foreach ($c in ($census | Where-Object { $_.Frame -ge 2 }))
  {
    Want ("frame " + $c.Frame) 'K' $c.K 139
    Want ("frame " + $c.Frame) 'D' $c.D 8
    Want ("frame " + $c.Frame) 'S' $c.S 0
    Want ("frame " + $c.Frame) 'P' $c.P 0
    Want ("frame " + $c.Frame) 'dt' $c.Dt 1
  }
  $clk = Get-FrameLines $lines 'clocks'
  $fired = @{}
  foreach ($f in $clk.Keys)
  {
    $d = Val $clk[$f] 'charge-dispersion=(\d+)'
    if ($null -eq $d) { $script:ruleBad += ("frame " + $f + ": the clocks line carries no charge-dispersion field") }
    elseif ($d -gt 0) { $fired[$f] = $d }
  }
  foreach ($f in 2, 8, 14, 20) { Want ("frame " + $f) 'dispersion steps' $fired[$f] 147 }
  $extra = @($fired.Keys | Where-Object { $_ -notin 2, 8, 14, 20 } | Sort-Object)
  if ($extra.Count)
    { $script:ruleBad += ("the dispersion also fired at " + ($extra -join ',') + ", documented only at 2, 8, 14, 20") }
  $sep = Get-FrameLines $lines 'separation'
  foreach ($f in $sep.Keys)
  {
    Want ("frame " + $f) 'layers carrying m != 0' (Val $sep[$f] 'm!=0\(sources\)=(\d+)/') 0
    Want ("frame " + $f) 'cells carrying pol != 0' (Val $sep[$f] 'pol!=0\(cells\)=(\d+)') 0
  }
  $pos = Get-FrameLines $lines 'positions'
  WantNear 'frame 14' 'Orbis y' (Dbl $pos[14] 'Orbis n=\d+ mean=\([-\d.]+,\s*([-\d.]+),') -3.0 0.005
  WantNear 'frame 20' 'Orbis y (after the wrap)' (Dbl $pos[20] 'Orbis n=\d+ mean=\([-\d.]+,\s*([-\d.]+),') 3.0 0.005
  $drift = Get-FrameLines $lines 'move-drift'
  Want 'frame 20' 'layers reading off side' (Val $drift[20] 'off side \(<3 axes\) = (\d+)') 147
}

# --- C5: the dispersion alone at L=9 ----------------------------------------------------
Report-Rule 'dispersion alone, L=9' 'README.md' `
  'f2 books 243 steps, 243/243 aligned' `
  'disp_L9.out' {
  param($lines)
  $census = Get-Census $lines
  if ($census.Count -ne 12) { $script:ruleBad += ("12 frames documented, the log has " + $census.Count) }
  foreach ($c in ($census | Where-Object { $_.Frame -ge 2 }))
  {
    Want ("frame " + $c.Frame) 'K' $c.K 235
    Want ("frame " + $c.Frame) 'D' $c.D 8
    Want ("frame " + $c.Frame) 'S' $c.S 0
    Want ("frame " + $c.Frame) 'P' $c.P 0
    Want ("frame " + $c.Frame) 'dt' $c.Dt 1
  }
  $clk = Get-FrameLines $lines 'clocks'
  $fired = @{}
  foreach ($f in $clk.Keys)
  {
    $d = Val $clk[$f] 'charge-dispersion=(\d+)'
    if ($d -gt 0) { $fired[$f] = $d }
  }
  foreach ($f in 2, 10) { Want ("frame " + $f) 'dispersion steps' $fired[$f] 243 }
  $extra = @($fired.Keys | Where-Object { $_ -notin 2, 10 } | Sort-Object)
  if ($extra.Count)
    { $script:ruleBad += ("the dispersion also fired at " + ($extra -join ',') + ", documented only at 2, 10") }
  $aln = Get-FrameLines $lines 'move-align'
  foreach ($f in 2, 10)
  {
    Want ("frame " + $f) 'layers moved' (Val $aln[$f] 'moved=(\d+)') 243
    Want ("frame " + $f) 'layers fully aligned' (Val $aln[$f] 'sign-match 0/1/2/3 = \d+/\d+/\d+/(\d+)') 243
  }
  $sep = Get-FrameLines $lines 'separation'
  foreach ($f in $sep.Keys)
  {
    Want ("frame " + $f) 'layers carrying m != 0' (Val $sep[$f] 'm!=0\(sources\)=(\d+)/') 0
    Want ("frame " + $f) 'cells carrying pol != 0' (Val $sep[$f] 'pol!=0\(cells\)=(\d+)') 0
  }
}

# --- C6: the candidate at L=9, where the ordering is decided inside the first era --------
Report-Rule 'candidate at L=9' 'FSM.txt' `
  'L=9 (candidate) dispersal 243/243 aligned; gap 2.00 -> 1.38 -> 0.98 over 2 eras' `
  'long9_L9.out' {
  param($lines)
  $aln = Get-FrameLines $lines 'move-align'
  Want 'frame 2 (the dispersal)' 'layers moved' (Val $aln[2] 'moved=(\d+)') 243
  Want 'frame 2 (the dispersal)' 'layers fully aligned' (Val $aln[2] 'sign-match 0/1/2/3 = \d+/\d+/\d+/(\d+)') 243
  $gap = Get-Gap $lines
  WantNear 'frame 2 (the dispersal)' 'gap' $gap[2] 2.000 0.005
  WantNear 'frame 8 (end of era 1)' 'gap' $gap[8] 1.378 0.005
  WantNear 'frame 16 (end of era 2)' 'gap' $gap[16] 0.982 0.005
  foreach ($c in (Get-Census $lines | Where-Object { $_.Frame -ge 2 -and $_.Frame -le 7 }))
  {
    Want ("frame " + $c.Frame) 'K' $c.K 235
    Want ("frame " + $c.Frame) 'D' $c.D 8
  }
}

# --- C7: who owns the standing axis (the election probe) --------------------------------
Report-Rule 'axis ownership' 'FSM.txt' `
  'm alignment, attributed the decay is the placement rule installing the ray the transport left the centre on' `
  'axisL7_bothab.out' {
  param($lines)
  $owner = Get-FrameLines $lines 'axis-owner'
  $elect = Get-FrameLines $lines 'axis-elect'
  if ($owner.Count -eq 0 -or $elect.Count -eq 0)
  { $script:ruleBad += "the log carries no axis-owner/axis-elect lines: it was not built with /D AXIS_ELECTION_TRACE"; return }

  # Every election is the placement rule: the classic field candidate never wins one.
  foreach ($f in ($elect.Keys | Sort-Object))
  {
    Want ("frame " + $f) 'installations from the classic path' (Val $elect[$f] '\| classic=(\d+)') 0
  }
  # The first election: all 147 layers, none of them a re-election.
  Want 'frame 4' 'installations (cumulative)' (Val $elect[4] 'total=(\d+)') 147
  Want 'frame 4' 'first elections' (Val $elect[4] 'first=(\d+)') 147
  # The era-3 turnaround: 145 re-elections and 28 layers left off their octant ray.
  Want 'frame 16' 'installations (cumulative)' (Val $elect[16] 'total=(\d+)') 439
  Want 'frame 16' 'installations landing aligned' (Val $elect[16] 'placement=\d+ \(aligned3=(\d+)\)') 411
  Want 'frame 16' 'layers off their octant ray' (Val $owner[16] 'placement=\d+ \(misaligned (\d+)\)') 28
  # The last era read: the same quantity keeps stepping.
  Want 'frame 28' 'installations (cumulative)' (Val $elect[28] 'total=(\d+)') 704
  Want 'frame 28' 'layers off their octant ray' (Val $owner[28] 'placement=\d+ \(misaligned (\d+)\)') 46
}

# --- C8: the counterfactual, the axis anchored to the charge octant ---------------------
Report-Rule 'octant counterfactual' 'FSM.txt' `
  'The counterfactual, with the axis anchored to the octant instead: 147/147 to frame 30 and an era-5 gap of 0.624 against 0.372' `
  'axisL7_octant.out' {
  param($lines)
  # With the axis anchored, the alignment never decays: every frame after the first
  # election (frame 4) must have all 147 layers parallel to their octant.
  $axis = Get-FrameLines $lines 'axis-align'
  foreach ($f in ($axis.Keys | Where-Object { $_ -ge 4 }))
  {
    Want ("frame " + $f) 'layers with all three signs matching' (Val $axis[$f] 'sign-match 0/1/2/3 = \d+/\d+/\d+/(\d+)') 147
    Want ("frame " + $f) 'layers with m == 0' (Val $axis[$f] 'm==0=(\d+)') 0
  }
  # The era-5 gap, and the same frame of the placement build, for the contrast.
  $own = Get-Gap $lines
  WantNear 'frame 30 (era 5)' 'gap' $own[30] 0.624 0.005
  $other = Read-TraceLog 'axisL7_bothab.out'
  if ($null -eq $other) { $script:ruleBad += 'the contrast log build\axisL7_bothab.out is absent (the placement baseline of this comparison)' }
  else
  {
    $ref = Get-Gap $other
    WantNear 'frame 30 (era 5), placement baseline' 'gap' $ref[30] 0.372 0.005
  }
}

# ======================================================================================
# verdict
# ======================================================================================
Write-Host ""
$color = 'Green'
if ($script:errors -gt 0) { $color = 'Red' }
Write-Host ("check-docs: " + $script:errors + " error(s), " + $script:warns + " warning(s), " +
            $script:infos + " note(s)") -ForegroundColor $color
if ($script:errors -gt 0) { exit 1 }
exit 0

