# check_docs.ps1 -- the documents against the artifacts (`nmake check-docs`).
#
# The repository rests on quoted numbers and on paths that point at evidence, so this script
# checks the three kinds of claim that CAN be checked mechanically, and then says explicitly
# what it does not check -- the coverage is never assumed to be wider than it is.
#
#   A. cited paths.  Every path the documents quote (RESEARCH_LOG.md, FSM.txt, the Makefile, the
#      manuscript, the build scripts and the source comments) resolves to a file in this tree,
#      or is declared as living elsewhere -- the RESEARCH_LOG.md's "lives in" table, or
#      experiments\check_docs.allow -- or is a generated artifact (build\, obj\).
#   B. registry vs sources.  Every macro of the FSM.txt registry (section 9) is defined by an
#      #ifdef/#ifndef in the sources, and every macro the Makefile turns on exists.  The other
#      direction -- macros the sources carry that the registry does not name -- is reported as
#      INFO with its count: the registry is a curated list, not an inventory, and the number is
#      printed so the difference stays visible instead of growing unnoticed.
#   C. invariants vs logs.  The machine-checkable rows of FSM.txt section 10 (and the candidate
#      rows the RESEARCH_LOG.md quotes from the same logs) are recomputed from the trace logs in build\.
#      A log that is not in the working tree is reported as NOT BUILT, not as a failure: build\
#      is generated and never committed, so a fresh clone legitimately has none of them.
#
# NOT checked: the prose of the manuscript and of the RESEARCH_LOG.md beyond the rows encoded in C, the
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
$docPaths = @('RESEARCH_LOG.md', 'RESEARCH_LOG.md', 'FSM.txt', 'Makefile', 'it_from_bit.tex', 'build_gui.bat', 'build.bat')
$docPaths += (Get-OwnSources | ForEach-Object { $_.FullName.Substring($root.Length + 1) })

# Declared as living elsewhere: the RESEARCH_LOG.md's "lives in" table (by file name), plus
# experiments\check_docs.allow (glob patterns, one per line).
$declared = @{}
$inTable = $false
foreach ($line in ((Read-Text 'RESEARCH_LOG.md') -split "`r?`n"))
{
  if ($line -match '\|\s*quoted in the text\s*\|\s*lives in\s*\|') { $inTable = $true; continue }
  if ($inTable)
  {
    if ($line -notmatch '^\s*\|') { $inTable = $false; continue }
    foreach ($m in [regex]::Matches($line, '[A-Za-z0-9_./\\-]+\.(md|csv|py|c|cpp|ps1)'))
      { $declared[[System.IO.Path]::GetFileName($m.Value.Replace('/','\'))] = 'RESEARCH_LOG.md "lives in"' }
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
$statusTokens = @('[CANDIDATE]', '[PROBE]', 'measurement', 'not built', 'default')
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
  try { & $check $lines }
  catch { $script:ruleBad += ("the rule threw instead of checking: " + $_.Exception.Message) }
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

# --- C3: the twelve era-end gaps, i.e. the RESEARCH_LOG.md's twelve-era table --------------------
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
Report-Rule 'dispersion alone, L=9' 'RESEARCH_LOG.md' `
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
  'm alignment, attributed the decay is the placement rule installing the ray the transport left the center on' `
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

# --- C9: the anchored axis over twelve eras: alignment held, ordering not ----------------
Report-Rule 'octant over twelve eras' 'FSM.txt' `
  'Over twelve eras the same anchoring holds 147/147 in all 72 frames and still loses the ordering: eight inverted frames (44, 45, 59, 68-72) against three (65-67)' `
  'five_L7.out' {
  param($lines)
  # The alignment never decays, in all 72 frames, as in the five-era log.
  $axis = Get-FrameLines $lines 'axis-align'
  foreach ($f in ($axis.Keys | Where-Object { $_ -ge 4 }))
  {
    Want ("frame " + $f) 'layers with all three signs matching' (Val $axis[$f] 'sign-match 0/1/2/3 = \d+/\d+/\d+/(\d+)') 147
  }
  # The inversion windows: eight frames here, three in the four-macro build.
  $ownGap = Get-Gap $lines
  $own = @($ownGap.Keys | Where-Object { $ownGap[$_] -lt 0 } | Sort-Object)
  if (($own -join ',') -ne '44,45,59,68,69,70,71,72')
  { $script:ruleBad += ("this build inverts in " + $own.Count + " frame(s) (" + ($own -join ',') + "), documented 8: 44, 45, 59, 68-72") }
  WantNear 'frame 72 (era 12)' 'gap' $ownGap[72] -0.498 0.005
  $other = Read-TraceLog 'long2_L7.out'
  if ($null -eq $other) { $script:ruleBad += 'the contrast log build\long2_L7.out is absent (the four-macro baseline of this comparison)' }
  else
  {
    $refGap = Get-Gap $other
    $ref = @($refGap.Keys | Where-Object { $refGap[$_] -lt 0 } | Sort-Object)
    if (($ref -join ',') -ne '65,66,67')
    { $script:ruleBad += ("the four-macro build inverts in " + $ref.Count + " frame(s) (" + ($ref -join ',') + "), documented 3: 65, 66, 67") }
  }
}

# --- C10: the corrected manuscript sentence on the twelve-era ledger ---------------------
Report-Rule 'twelve-era ledger' 'it_from_bit.tex' `
  'The ledger holds its plateau ($K=139$, $D=8$ at $L=7$) from frame~2 to frame~13 and then drifts with the cascades, reaching $K=124$, $D=23$ at frame~72' `
  'long2_L7.out' {
  param($lines)
  $census = Get-Census $lines
  $rows = @($census | Where-Object { $_.Frame -ge 2 })
  # The plateau holds from frame 2 to frame 13 ...
  foreach ($c in @($rows | Where-Object { $_.Frame -le 13 }))
  {
    Want ("frame " + $c.Frame) 'K' $c.K 139
    Want ("frame " + $c.Frame) 'D' $c.D 8
  }
  # ... the first departure is frame 14 ...
  $first = @($rows | Where-Object { $_.K -ne 139 -or $_.D -ne 8 } | Select-Object -First 1)
  if ($first.Count -eq 0) { $script:ruleBad += 'the ledger never leaves the plateau, so the sentence is now wrong the other way' }
  else
  {
    Want ("frame " + $first[0].Frame + " (first departure)") 'K' $first[0].K 136
    Want ("frame " + $first[0].Frame + " (first departure)") 'D' $first[0].D 11
  }
  # ... and frame 72 carries what the sentence quotes.
  $last = @($rows | Where-Object { $_.Frame -eq 72 } | Select-Object -First 1)
  if ($last.Count -eq 0) { $script:ruleBad += 'the log has no frame 72 row' }
  else { Want 'frame 72' 'K' $last[0].K 124; Want 'frame 72' 'D' $last[0].D 23 }
  # The saturation is what holds throughout: K + D = W = 147, in every frame.
  foreach ($c in $rows)
  {
    if ($c.K + $c.D -ne 147)
      { $script:ruleBad += ("frame " + $c.Frame + ": K + D = " + ($c.K + $c.D) + ", documented 147 (= W)") }
  }
}

# --- C11: the finite-size table (the plateau and the roster at several sizes) ------------
Report-Rule 'finite-size table' 'RESEARCH_LOG.md' `
  '`D = 8` at every size, so `K = W - 8` exactly' `
  'scale_ref_L9.out' {
  param($lines)
  # One row per size, each from its own log.  L=11 is checked when its log is present: two frames
  # suffice for the static row, but a clone of this repository will not have it.
  $rows = @(
    @{ L = 5;  log = 'scale_ref_L5.out' },
    @{ L = 7;  log = 'scale_ref_L7.out' },
    @{ L = 9;  log = 'scale_ref_L9.out' },
    @{ L = 11; log = 'scale_ref_L11.out' })
  foreach ($row in $rows)
  {
    $side = $row.L
    $W = 3 * $side * $side
    $ls = Read-TraceLog $row.log
    if ($null -eq $ls)
    {
      Report 'INFO' ("C. finite-size table: " + $row.log + " is not built here, so the L=" + $side + " row is not checked")
      continue
    }
    $f2 = @(Get-Census $ls | Where-Object { $_.Frame -eq 2 } | Select-Object -First 1)
    if ($f2.Count -eq 0) { $script:ruleBad += ($row.log + ": no frame-2 row"); continue }
    Want ("L=" + $side) 'W (3L^2)' (Val ($ls | Where-Object { $_ -match '^# first-era trace' } | Select-Object -First 1) 'W_USED=(\d+)') $W
    Want ("L=" + $side) 'D' $f2[0].D 8
    Want ("L=" + $side) 'K' $f2[0].K ($W - 8)
    Want ("L=" + $side) 'K + D' ($f2[0].K + $f2[0].D) $W
    # The roster: eight words at every size, and the pairable fraction the section calls about 38%.
    $summary = @($ls | Where-Object { $_ -match 'distinct words=' } | Select-Object -First 1)
    if ($summary.Count -eq 0) { $script:ruleBad += ($row.log + ": no charge-word summary line") }
    else
    {
      Want ("L=" + $side) 'distinct charge words' (Val $summary[0] 'distinct words=\s*(\d+)') 8
      $pairable = Val $summary[0] 'pair rule=\s*(\d+)'
      if ($null -eq $pairable) { $script:ruleBad += ($row.log + ": cannot read the pairable count") }
      else
      {
        $frac = 100.0 * $pairable / $W
        if ($frac -lt 35.0 -or $frac -gt 41.0)
          { $script:ruleBad += ("L=" + $side + ": the pairable fraction is " + ("{0:F1}" -f $frac) + "%, documented as about 38%") }
      }
    }
  }
}

# --- C12: the inherited-impulse probe (it is zero, and the frames do not move) -----------
Report-Rule 'the inherited-impulse probe' 'RESEARCH_LOG.md' `
  'the probe log and the log of the same run without the macro are **byte-identical** (same SHA-256, `build\inherit_L7.out` against `build\both_L7_repro.out`), and `reseed-carried-reloc = 0` in each of the 20 frames' `
  'inherit_L7.out' {
  param($lines)
  $clk = Get-FrameLines $lines 'clocks'
  if ($clk.Count -ne 20) { $script:ruleBad += ("20 frames documented, the log has " + $clk.Count) }
  foreach ($f in ($clk.Keys | Sort-Object))
  {
    # The probe's own count of what it dropped, and the passive sum of |reloc| over the centre cells at
    # the same reseed (printed whether or not the macro is on).  Both are documented as zero.
    Want ("frame " + $f) 'impulses inherited at the reseed' (Val $clk[$f] 'reseed-carried-reloc=(\d+)') 0
    Want ("frame " + $f) '|reloc| carried on the lattice at the reseed' (Val $clk[$f] 'captured-sum=(\d+)') 0
  }
  # The carrier arithmetic the RESEARCH_LOG.md quotes for frame 14: what was applied is the queue plus the frame's
  # own bookings (64 + 8 = 72), and the 64 is a queue length, not a number of layers carrying a reloc.
  if ($null -eq $clk[14]) { $script:ruleBad += 'the probe log has no frame 14 clocks line' }
  else
  {
    $x = $clk[14]
    $applied = Val $x 'applied=(\d+)'; $pending = Val $x 'pending-impulse=(\d+)'
    $booked = Val $x 'booked-on-lattice=(\d+)'
    Want 'frame 14' 'encounter bookings installed at the commit' $pending 64
    Want 'frame 14' 'impulse bookings made by the frame' $booked 8
    Want 'frame 14' 'impulses applied' $applied 72
    if ($pending + $booked -ne $applied)
      { $script:ruleBad += ("frame 14: the encounter bookings (" + $pending + ") plus the frame's own bookings (" + $booked +
                            ") do not add up to what was applied (" + $applied + ")") }
  }
  # The byte-identity claim, and the same zero in the baseline (which has no probe macro compiled).
  $probeText = Read-Text 'build\inherit_L7.out'
  $baseText = Read-Text 'build\both_L7_repro.out'
  if ($null -eq $baseText) { $script:ruleBad += 'the baseline log build\both_L7_repro.out is absent (the promoted run this probe is compared with)' }
  elseif ($probeText -ne $baseText) { $script:ruleBad += 'the probe log is no longer byte-identical to its baseline' }
  $baseline = Read-TraceLog 'both_L7_repro.out'
  if ($null -ne $baseline)
  {
    foreach ($f in ((Get-FrameLines $baseline 'clocks').Keys | Sort-Object))
      { Want ("baseline frame " + $f) '|reloc| carried on the lattice at the reseed' (Val (Get-FrameLines $baseline 'clocks')[$f] 'captured-sum=(\d+)') 0 }
  }
}

# --- C13: the twelve-era decomposition of the flight channel ------------------------------
Report-Rule 'twelve-era decomposition' 'RESEARCH_LOG.md' `
  '| **all** | **811** | **475 (58.6 %)** | **331 (277 = 83.7 %)** | **480 (198 = 41.3 %)** |' `
  'long2_L7.out' {
  param($lines)
  $split = Get-FrameLines $lines 'move-split'
  $drift = Get-FrameLines $lines 'move-drift'
  if ($split.Count -ne 72) { $script:ruleBad += ("72 frames documented, the log has " + $split.Count) }
  $fl = 0; $fl3 = 0; $on = 0; $onAl = 0; $off = 0; $offAl = 0
  foreach ($f in ($split.Keys | Sort-Object))
  {
    $fl += Val $split[$f] 'flight=(\d+)'
    $fl3 += Val $split[$f] 'flight=\d+\s+0/1/2/3 = \d+/\d+/\d+/(\d+)'
    $d = $drift[$f]
    if ($null -eq $d) { $script:ruleBad += ("frame " + $f + ": the move-split line has no move-drift companion"); continue }
    $on += Val $d 'on its octant side \(all 3 axes\) = (\d+)'
    $onAl += Val $d 'octant-aligned = (\d+)\s+\|\s+off side'
    $off += Val $d 'off side \(<3 axes\) = (\d+)'
    $offAl += Val $d 'off side \(<3 axes\) = \d+, octant-aligned = (\d+)'
  }
  # The invariant that ties the two lines together: every flight mover is in exactly one drift bucket.
  # Without it a line that silently stops parsing would still produce a plausible total.
  if ($on + $off -ne $fl)
    { $script:ruleBad += ("the drift buckets (" + $on + " + " + $off + ") do not add up to the flight counter (" + $fl + ")") }
  if ($off -eq 0) { $script:ruleBad += 'the off-side bucket is empty everywhere: the drift lines are not being read' }
  # The documented totals.
  Want 'all twelve eras' 'flight movers' $fl 811
  Want 'all twelve eras' 'flight fully aligned' $fl3 475
  Want 'all twelve eras' 'on-side movers' $on 331
  Want 'all twelve eras' 'on-side aligned' $onAl 277
  Want 'all twelve eras' 'off-side movers' $off 480
  Want 'all twelve eras' 'off-side aligned' $offAl 198
  # The three shares of the totals row, and the reading it draws: the bucket concentrates the
  # misalignment (on side well above off side) but does not account for it.
  WantNear 'all twelve eras' 'flight aligned %' (100.0 * $fl3 / $fl) 58.6 0.05
  WantNear 'all twelve eras' 'on-side aligned %' (100.0 * $onAl / $on) 83.7 0.05
  WantNear 'all twelve eras' 'off-side aligned %' (100.0 * $offAl / $off) 41.3 0.05
  if ((100.0 * $onAl / $on) -le (100.0 * $offAl / $off)) { $script:ruleBad += 'the on-side share is no longer above the off-side share, which is what the table concludes' }
  # The first era is the one the alignment claim is about, and it is fully one-sided.
  $era1 = @($split.Keys | Sort-Object | Select-Object -First 6)
  $e1fl = 0; $e1fl3 = 0; $e1off = 0
  foreach ($f in $era1) { $e1fl += Val $split[$f] 'flight=(\d+)'; $e1fl3 += Val $split[$f] 'flight=\d+\s+0/1/2/3 = \d+/\d+/\d+/(\d+)'; $e1off += (Val $drift[$f] 'off side \(<3 axes\) = (\d+)') }
  Want 'era 1 (frames 1-6)' 'flight movers' $e1fl 179
  Want 'era 1 (frames 1-6)' 'flight fully aligned' $e1fl3 179
  Want 'era 1 (frames 1-6)' 'off-side movers' $e1off 0
}

# --- C14: the carrier of the flight steps (not the queue -- the own-axis thrust) ----------
Report-Rule 'carrier of the flight steps' 'RESEARCH_LOG.md' `
  'Every unaligned flight step carries the own-axis thrust bit (32)' `
  'carrier_L7_20.out' {
  param($lines)
  $split = Get-FrameLines $lines 'move-split'
  $car = Get-FrameLines $lines 'move-carrier'
  $wri = Get-FrameLines $lines 'move-writer'
  if ($split.Count -ne 20) { $script:ruleBad += ("20 frames documented, the log has " + $split.Count) }
  if ($car.Count -ne 20) { $script:ruleBad += ("20 move-carrier lines expected, the log has " + $car.Count) }
  if ($wri.Count -ne 20) { $script:ruleBad += ("20 move-writer lines expected, the log has " + $wri.Count) }
  # The four carrier classes partition the flight movers, in every frame.  This is the invariant that
  # makes the reading below a decomposition instead of a coincidence.
  $un = 0
  foreach ($f in ($split.Keys | Sort-Object))
  {
    $fl = Val $split[$f] 'flight=(\d+)'; $flA = Val $split[$f] 'flight=\d+\s+0/1/2/3 = \d+/\d+/\d+/(\d+)'
    $q = Val $car[$f] 'queue-carried = (\d+)'; $fr = Val $car[$f] 'booked this frame = (\d+)'
    $bo = Val $car[$f] 'both = (\d+)';        $no = Val $car[$f] 'no writer = (\d+)'
    if ($null -eq $q -or $null -eq $fr -or $null -eq $bo -or $null -eq $no)
      { $script:ruleBad += ("frame " + $f + ": the move-carrier line does not parse"); continue }
    if ($q + $fr + $bo + $no -ne $fl)
      { $script:ruleBad += ("frame " + $f + ": the carriers (" + $q + " + " + $fr + " + " + $bo + " + " + $no +
                            ") do not add up to the flight counter (" + $fl + ")") }
    $un += ($fl - $flA)
  }
  # The documented window totals.  The carrier rows of the RESEARCH_LOG.md's table stop at frame 18 (the last
  # complete era); the writer rows cover the whole 20 frames, which is why the two ragged totals differ.
  $t = @{ q=0; qA=0; f=0; fA=0; b=0; bA=0; n=0; nA=0 }
  foreach ($f in ($split.Keys | Sort-Object | Where-Object { $_ -le 18 }))
  {
    $t.q += (Val $car[$f] 'queue-carried = (\d+)');      $t.qA += (Val $car[$f] 'queue-carried = \d+ \(octant-aligned (\d+)\)')
    $t.f += (Val $car[$f] 'booked this frame = (\d+)');  $t.fA += (Val $car[$f] 'booked this frame = \d+ \(octant-aligned (\d+)\)')
    $t.b += (Val $car[$f] 'both = (\d+)');               $t.bA += (Val $car[$f] 'both = \d+ \(octant-aligned (\d+)\)')
    $t.n += (Val $car[$f] 'no writer = (\d+)');          $t.nA += (Val $car[$f] 'no writer = \d+ \(octant-aligned (\d+)\)')
  }
  Want 'frames 1-18' 'queue-carried' $t.q 0
  Want 'frames 1-18' 'booked this frame' $t.f 122
  Want 'frames 1-18' 'booked this frame, aligned' $t.fA 101
  Want 'frames 1-18' 'both' $t.b 147
  Want 'frames 1-18' 'both, aligned' $t.bA 147
  Want 'frames 1-18' 'no writer' $t.n 0
  Want 'frames 1-18' 'flight movers (all carriers)' ($t.q + $t.f + $t.b + $t.n) 269
  Want 'frames 1-18' 'flight movers, aligned' ($t.qA + $t.fA + $t.bA + $t.nA) 248
  # The writer columns: the dispersion's steps are fully aligned, every other bit except the thrust
  # contributes no unaligned step at all, and the thrust accounts for at least the unaligned total.
  $bits = @(@('walk funnel', 'walk'), @('relay', 'relay'), @('relay contact', 'relay-contact'),
            @('cohesion table', 'cohesion'), @('dispersion', 'dispersion'), @('thrust', 'thrust'))
  $thrustUn = 0
  foreach ($b in $bits)
  {
    $m = 0; $a = 0
    foreach ($f in ($wri.Keys | Sort-Object))
    {
      $m += (Val $wri[$f] ($b[1] + ' = (\d+)'))
      $a += (Val $wri[$f] ($b[1] + ' = \d+ \((\d+)\)'))
    }
    if ($b[1] -eq 'thrust') { $thrustUn = $m - $a
      Want 'frames 1-20' 'own-axis thrust movers' $m 128
      Want 'frames 1-20' 'own-axis thrust movers, aligned' $a 103 }
    elseif ($b[1] -eq 'dispersion') { Want 'frames 1-20' 'dispersion movers' $m 147
      Want 'frames 1-20' 'dispersion movers, aligned' $a 147 }
    elseif ($m -ne 0) { $script:ruleBad += ("the " + $b[0] + " bit is documented as contributing no flight step, but it carries " + $m) }
    if ($b[1] -ne 'thrust' -and ($m - $a) -gt 0)
      { $script:ruleBad += ("the " + $b[0] + " bit carries " + ($m - $a) + " unaligned flight step(s), documented none") }
  }
  if ($thrustUn -lt $un)
    { $script:ruleBad += ("the thrust bit carries " + $thrustUn + " unaligned steps, fewer than the " + $un +
                          " the flight channel produced -- so not every unaligned step is a thrust") }
  # The same claim over the whole twelve eras, when that log is present.  Here the sharp form is an
  # equality: the thrust's unaligned count IS the flight channel's unaligned count.
  $long = Read-TraceLog 'carrier_L7_72.out'
  if ($null -eq $long)
    { Report 'INFO' 'C. carrier of the flight steps: build\carrier_L7_72.out is not built here, so the twelve-era half of the claim is not checked' }
  else
  {
    $lsp = Get-FrameLines $long 'move-split'
    $lcar = Get-FrameLines $long 'move-carrier'
    $lwr = Get-FrameLines $long 'move-writer'
    if ($lsp.Count -ne 72) { $script:ruleBad += ("72 frames documented for the twelve-era log, it has " + $lsp.Count) }
    $tot = 0; $totUn = 0
    foreach ($f in ($lsp.Keys | Sort-Object))
    {
      $lfl = Val $lsp[$f] 'flight=(\d+)'; $lflA = Val $lsp[$f] 'flight=\d+\s+0/1/2/3 = \d+/\d+/\d+/(\d+)'
      $lq = Val $lcar[$f] 'queue-carried = (\d+)'; $lf = Val $lcar[$f] 'booked this frame = (\d+)'
      $lb = Val $lcar[$f] 'both = (\d+)'; $ln = Val $lcar[$f] 'no writer = (\d+)'
      if ($null -eq $lq -or $null -eq $lf -or $null -eq $lb -or $null -eq $ln)
        { $script:ruleBad += ("twelve-era frame " + $f + ": the move-carrier line does not parse"); continue }
      if ($lq + $lf + $lb + $ln -ne $lfl)
        { $script:ruleBad += ("twelve-era frame " + $f + ": the carriers do not add up to the flight counter (" + $lfl + ")") }
      $tot += $lfl; $totUn += ($lfl - $lflA)
    }
    Want 'twelve eras' 'flight movers' $tot 811
    Want 'twelve eras' 'flight movers, unaligned' $totUn 336
    $lm = 0; $la = 0
    foreach ($f in ($lwr.Keys | Sort-Object))
    {
      $lm += (Val $lwr[$f] 'thrust = (\d+)')
      $la += (Val $lwr[$f] 'thrust = \d+ \((\d+)\)')
    }
    Want 'twelve eras' 'own-axis thrust movers' $lm 664
    Want 'twelve eras' 'own-axis thrust movers, aligned' $la 328
    if (($lm - $la) -ne $totUn)
      { $script:ruleBad += ("twelve eras: the thrust carries " + ($lm - $la) + " unaligned steps against the " +
                            $totUn + " the flight channel produced -- the two are documented as equal") }
  }
}

# --- C15: the carrier instrumentation is reporting only -----------------------------------
Report-Rule 'the carrier instrument is reporting only' 'RESEARCH_LOG.md' `
  'reporting only: the new log, with its two added lines removed, is **identical line for line** to the log of the same run before the change (`build\carrier_L7_20.out` against `build\both_L7_repro.out`)' `
  'carrier_L7_20.out' {
  param($lines)
  $before = Read-TraceLog 'both_L7_repro.out'
  if ($null -eq $before)
    { $script:ruleBad += 'the pre-instrumentation log build\both_L7_repro.out is absent (the run this comparison is against)' }
  else
  {
    $stripped = @($lines | Where-Object { $_ -notmatch 'move-carrier|move-writer' })
    if ($stripped.Count -ne $before.Count)
      { $script:ruleBad += ("with the two new lines removed the log has " + $stripped.Count + " lines, the run before the change " + $before.Count) }
    else
    {
      for ($i = 0; $i -lt $stripped.Count; $i++)
      {
        if ($stripped[$i] -ne $before[$i])
        {
          $script:ruleBad += ("line " + ($i + 1) + " differs once the new lines are removed: [" + $stripped[$i].Trim() + "] against [" + $before[$i].Trim() + "]")
          break
        }
      }
    }
  }
}

# --- C16: the thrust decision point (the word it reads, the vector it writes) --------------
Report-Rule 'the thrust decision point' 'RESEARCH_LOG.md' `
  'every thrust call through frame 14 -- 256 of them, in the eras where the flight channel reads 49 % aligned -- is a three-axis step along the octant of the word the layer keeps' `
  'thrustword_L7_20.out' {
  param($lines)
  $thr = Get-FrameLines $lines 'move-thrust'
  if ($thr.Count -ne 20) { $script:ruleBad += ("20 frames documented, the log has " + $thr.Count) }
  foreach ($f in ($thr.Keys | Sort-Object))
  {
    # The word comparison: the thrust never aims by a word the commit discards.
    Want ("frame " + $f) 'thrusts booked by a word other than the committed one' (Val $thr[$f] 'word differs = (\d+)') 0
    # The booking-side score, and the invariant that ties the three classes to the call count.
    $calls = Val $thr[$f] 'calls = (\d+)'
    $al = Val $thr[$f] 'calls = \d+ \(aligned (\d+)'
    $pa = Val $thr[$f] 'aligned \d+, partial (\d+)'
    $ag = Val $thr[$f] 'partial \d+, against (\d+)'
    if ($null -eq $calls -or $null -eq $al -or $null -eq $pa -or $null -eq $ag)
      { $script:ruleBad += ("frame " + $f + ": the move-thrust line does not carry the booking score"); continue }
    if ($al + $pa + $ag -ne $calls)
      { $script:ruleBad += ("frame " + $f + ": aligned+partial+against (" + ($al + $pa + $ag) + ") is not the call count (" + $calls + ")") }
    if ($pa -ne 0 -or $ag -ne 0)
      { $script:ruleBad += ("frame " + $f + ": " + $pa + " partial and " + $ag + " against bookings, documented none") }
  }
  # The cumulative row the RESEARCH_LOG.md quotes.
  $x = $thr[14]
  if ($null -eq $x) { $script:ruleBad += 'the log has no frame 14 move-thrust line' }
  else
  {
    Want 'frame 14 (cumulative)' 'thrust calls' (Val $x 'calls = (\d+)') 256
    Want 'frame 14 (cumulative)' 'aligned' (Val $x 'calls = \d+ \(aligned (\d+)') 256
    Want 'frame 14 (cumulative)' 'partial' (Val $x 'aligned \d+, partial (\d+)') 0
    Want 'frame 14 (cumulative)' 'against' (Val $x 'partial \d+, against (\d+)') 0
  }
  # The twelve-era half of the claim, when that log is present: the thrust still never aims AGAINST the
  # octant, but in the later eras a share of its calls is partial -- a step along fewer than three of the
  # octant's axes, which no 3-of-3 test can accept whatever the direction.  That is the first of the two
  # components the RESEARCH_LOG.md names.
  $long = Read-TraceLog 'thrustword_L7_72.out'
  if ($null -eq $long)
    { Report 'INFO' 'C. the thrust decision point: build\thrustword_L7_72.out is not built here, so the twelve-era half of the claim is not checked' }
  else
  {
    $lthr = Get-FrameLines $long 'move-thrust'
    if ($lthr.Count -ne 72) { $script:ruleBad += ("72 frames documented for the twelve-era log, it has " + $lthr.Count) }
    foreach ($f in ($lthr.Keys | Sort-Object))
    {
      Want ("twelve eras, frame " + $f) 'thrusts booked by a word other than the committed one' (Val $lthr[$f] 'word differs = (\d+)') 0
      $lc = Val $lthr[$f] 'calls = (\d+)'; $la = Val $lthr[$f] 'calls = \d+ \(aligned (\d+)'
      $lp = Val $lthr[$f] 'aligned \d+, partial (\d+)'; $lg = Val $lthr[$f] 'partial \d+, against (\d+)'
      if ($null -eq $lc -or $null -eq $la -or $null -eq $lp -or $null -eq $lg) { continue }
      if ($la + $lp + $lg -ne $lc)
        { $script:ruleBad += ("twelve eras, frame " + $f + ": aligned+partial+against is not the call count") }
      if ($lg -ne 0)
        { $script:ruleBad += ("twelve eras, frame " + $f + ": " + $lg + " bookings aimed against the octant, documented none") }
    }
    $z = $lthr[72]
    if ($null -eq $z) { $script:ruleBad += 'the twelve-era log has no frame 72 move-thrust line' }
    else
    {
      Want 'twelve eras (cumulative)' 'thrust calls' (Val $z 'calls = (\d+)') 2129
      Want 'twelve eras (cumulative)' 'aligned' (Val $z 'calls = \d+ \(aligned (\d+)') 1763
      Want 'twelve eras (cumulative)' 'partial' (Val $z 'aligned \d+, partial (\d+)') 366
      Want 'twelve eras (cumulative)' 'against' (Val $z 'partial \d+, against (\d+)') 0
    }
  }
}

# --- C17: the impulse is not the displacement --------------------------------------------
Report-Rule 'the impulse is not the displacement' 'RESEARCH_LOG.md' `
  '(83 %) are relocations** -- displacements that do not follow the impulse the layer was given at all -- and **58 follow a partial-axis impulse**' `
  'thrustvec_L7_20.out' {
  param($lines)
  $vec = Get-FrameLines $lines 'move-thrustvec'
  $split = Get-FrameLines $lines 'move-split'
  if ($vec.Count -ne 20) { $script:ruleBad += ("20 frames documented, the log has " + $vec.Count) }
  foreach ($f in ($vec.Keys | Sort-Object))
  {
    # Every mover that follows its impulse is aligned, every mover that does not is one of the
    # unaligned ones -- the claim, frame by frame.
    $same = Val $vec[$f] 'follows the impulse = (\d+)'
    $sameAl = Val $vec[$f] 'follows the impulse = \d+ \(octant-aligned (\d+)\)'
    $diff = Val $vec[$f] 'differs = (\d+)'
    $diffAl = Val $vec[$f] 'differs = \d+ \(octant-aligned (\d+)\)'
    if ($null -eq $same -or $null -eq $diff) { $script:ruleBad += ("frame " + $f + ": the move-thrustvec line does not parse"); continue }
    if ($same -ne $sameAl) { $script:ruleBad += ("frame " + $f + ": " + ($same - $sameAl) + " mover(s) follow their impulse and are still unaligned") }
    if ($diffAl -ne 0) { $script:ruleBad += ("frame " + $f + ": " + $diffAl + " mover(s) disagree with their impulse and are aligned after all") }
    # And the sharp cross-check: the movers that disagree with their impulse ARE the unaligned ones.
    $fl = Val $split[$f] 'flight=(\d+)'
    $flA = Val $split[$f] 'flight=\d+\s+0/1/2/3 = \d+/\d+/\d+/(\d+)'
    if ($diff -ne ($fl - $flA))
      { $script:ruleBad += ("frame " + $f + ": " + $diff + " mover(s) disagree with their impulse against " + ($fl - $flA) + " unaligned flight movers") }
  }
  # The frames the RESEARCH_LOG.md tabulates.
  foreach ($case in @(@(6, 32, 32, 0, 0), @(12, 3, 3, 3, 0), @(14, 41, 41, 7, 0), @(18, 19, 19, 11, 0)))
  {
    $x = $vec[$case[0]]
    if ($null -eq $x) { $script:ruleBad += ("the log has no frame " + $case[0] + " move-thrustvec line"); continue }
    Want ("frame " + $case[0]) 'movers following their impulse' (Val $x 'follows the impulse = (\d+)') $case[1]
    Want ("frame " + $case[0]) 'of those, aligned' (Val $x 'follows the impulse = \d+ \(octant-aligned (\d+)\)') $case[2]
    Want ("frame " + $case[0]) 'movers disagreeing with their impulse' (Val $x 'differs = (\d+)') $case[3]
    Want ("frame " + $case[0]) 'of those, aligned' (Val $x 'differs = \d+ \(octant-aligned (\d+)\)') $case[4]
  }
  # The twelve-era half, when that log is present: here the identity is the claim -- the unaligned movers
  # are exactly those following a partial-axis impulse plus the relocations -- and so is the split.
  $long = Read-TraceLog 'thrustvec_L7_72.out'
  if ($null -eq $long)
    { Report 'INFO' 'C. the impulse is not the displacement: build\thrustvec_L7_72.out is not built here, so the twelve-era half of the claim is not checked' }
  else
  {
    $lvec = Get-FrameLines $long 'move-thrustvec'
    $lsplit = Get-FrameLines $long 'move-split'
    if ($lvec.Count -ne 72) { $script:ruleBad += ("72 frames documented for the twelve-era log, it has " + $lvec.Count) }
    foreach ($f in ($lvec.Keys | Sort-Object))
    {
      $s = Val $lvec[$f] 'follows the impulse = (\d+)'; $sA = Val $lvec[$f] 'follows the impulse = \d+ \(octant-aligned (\d+)\)'
      $d = Val $lvec[$f] 'differs = (\d+)'; $dA = Val $lvec[$f] 'differs = \d+ \(octant-aligned (\d+)\)'
      $e = Val $lvec[$f] 'thrust bit with no impulse = (\d+)'
      if ($null -eq $s -or $null -eq $d -or $null -eq $e) { $script:ruleBad += ("twelve-era frame " + $f + ": the move-thrustvec line does not parse"); continue }
      if ($dA -ne 0) { $script:ruleBad += ("twelve-era frame " + $f + ": " + $dA + " mover(s) disagree with their impulse and are aligned") }
      if ($e -ne 0) { $script:ruleBad += ("twelve-era frame " + $f + ": " + $e + " mover(s) carry the thrust bit with an empty impulse") }
      $fl = Val $lsplit[$f] 'flight=(\d+)'; $flA = Val $lsplit[$f] 'flight=\d+\s+0/1/2/3 = \d+/\d+/\d+/(\d+)'
      if ((($s - $sA) + $d) -ne ($fl - $flA))
        { $script:ruleBad += ("twelve-era frame " + $f + ": partial-axis followers plus relocations (" + (($s - $sA) + $d) +
                              ") is not the unaligned flight count (" + ($fl - $flA) + ")") }
    }
    $vs = 0; $vsA = 0; $vd = 0; $va3 = 0; $va2 = 0; $va1 = 0
    foreach ($f in ($lvec.Keys | Sort-Object))
    {
      $vs += (Val $lvec[$f] 'follows the impulse = (\d+)')
      $vsA += (Val $lvec[$f] 'follows the impulse = \d+ \(octant-aligned (\d+)\)')
      $vd += (Val $lvec[$f] 'differs = (\d+)')
      $va3 += (Val $lvec[$f] 'impulse axes 3/2/1 = (\d+)')
      $va2 += (Val $lvec[$f] 'impulse axes 3/2/1 = \d+/(\d+)')
      $va1 += (Val $lvec[$f] 'impulse axes 3/2/1 = \d+/\d+/(\d+)')
    }
    Want 'twelve eras' 'movers following their impulse' $vs 386
    Want 'twelve eras' 'of those, aligned' $vsA 328
    Want 'twelve eras' 'movers disagreeing with their impulse' $vd 278
    Want 'twelve eras' 'impulse axes 3' $va3 562
    Want 'twelve eras' 'impulse axes 2' $va2 74
    Want 'twelve eras' 'impulse axes 1' $va1 28
    Want 'twelve eras' 'unaligned = partial-axis followers' ($vs - $vsA) 58
    Want 'twelve eras' 'unaligned = relocations' $vd 278
    Want 'twelve eras' 'unaligned total (the two components)' (($vs - $vsA) + $vd) 336
  }
}

# --- C18: the centre has one mover, and the census only reads (source-level, no log) ------
# This one checks the sources, not a log: the claim is structural.  It pins the three facts the
# RESEARCH_LOG.md's "Who can move a center" section rests on -- that trackCenter() is a definition with no
# callers, that the census still aborts on a duplicate source, and that the section is still written.
# (Top level, not a { } block: a bare script block is an expression, so it would never run.)
# The count is of CALL sites, not mentions: `(?<!void\s)trackCenter\s*\(` skips the definition and a
# comment that names the function without calling it (simulation.cpp:689 does exactly that).
$centreBad = @()
$centreClaim = 'writes it too and **has no callers at all**'
$centreDoc = Read-Text 'RESEARCH_LOG.md'
if ($null -eq $centreDoc -or -not (($centreDoc -replace '\s+', ' ').Contains($centreClaim)))
  { $centreBad += 'the claim "trackCenter ... has no callers at all" is no longer written in RESEARCH_LOG.md' }
$centreCalls = 0; $centreDefs = 0
foreach ($f in (Get-ChildItem (Join-Path $root 'src') -Recurse -File -Include *.cpp, *.h, *.inc, *.cu))
{
  $t = [System.IO.File]::ReadAllText($f.FullName)
  $centreCalls += ([regex]::Matches($t, '(?<!void\s)trackCenter\s*\(')).Count
  $centreDefs  += ([regex]::Matches($t, 'void\s+trackCenter\s*\(')).Count
}
if ($centreCalls -ne 0)
  { $centreBad += ("trackCenter has " + $centreCalls + " call site(s), documented as none") }
if ($centreDefs -ne 1)
  { $centreBad += ("trackCenter has " + $centreDefs + " definitions, documented as exactly one (simulation.cpp)") }
$centreAtt = Read-Text 'src\model\attractor.cpp'
if ($null -eq $centreAtt -or -not $centreAtt.Contains('census: invalid or duplicate source'))
  { $centreBad += 'the census no longer aborts on a duplicate source (the RESEARCH_LOG.md relies on that)' }
if ($centreBad.Count -eq 0) { Report 'OK' 'C. the center has one mover   [source-level: trackCenter, the census throw]' }
else { foreach ($b in $centreBad) { Report 'ERROR' ("C. the center has one mover " + $b) } }

# ======================================================================================
# verdict
# ======================================================================================

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

