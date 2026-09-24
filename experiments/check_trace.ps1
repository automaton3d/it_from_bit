# check_trace.ps1 -- compares the logs produced by experiments\check_trace.bat with the
# expectations committed in experiments\golden\ (that script's header explains why a plain
# comparison is the right test: the model is deterministic, so a difference is a change in the
# dynamics or in the scenario, never noise).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File experiments\check_trace.ps1 [-L 5] [-Frames 8] [-Refresh]
#
# -Refresh writes what was produced over the expectations: use it when a difference is intended,
# and commit the result.  The file name carries the scenario (`<config>_L<L>_<frames>.out`), so a
# run at another size looks for its own expectation instead of comparing against the wrong one.
#
# Only the produced stdout is compared; the stderr stream of the harness carries the model's own
# banner and is not part of the recorded state.
#
# Exit code: 0 when every configuration matches (or, with -Refresh, when every expectation was
# written); 1 on a difference, a missing log or a missing expectation.

param([int]$L = 5, [int]$Frames = 8, [string]$Configs = 'ref,disp,base,bothab',
      [string]$Produced = 'obj\trace_check', [string]$Expectations = 'experiments\golden',
      [switch]$Refresh)

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot

$script:errors = 0; $script:ok = 0
function Report([string]$kind, [string]$text)
{
  switch ($kind)
  {
    'ERROR' { $script:errors++; Write-Host ("ERROR  " + $text) -ForegroundColor Red }
    'OK'    { $script:ok++;     Write-Host ("ok     " + $text) -ForegroundColor DarkGreen }
    'INFO'  {                   Write-Host ("       " + $text) }
  }
}

function Clip($line)
{
  if ($null -eq $line) { return "<no line: the log ends here>" }
  $t = ($line -replace '\s+$', '')
  if ($t.Length -gt 140) { $t = $t.Substring(0, 140) + " ..." }
  if (-not $t) { return "<blank line>" }
  return $t
}

# The frame a line belongs to, for the diagnostic: the trace prints "# frame N ..." blocks, so
# the nearest marker above the difference says which light frame moved.
function FrameOf($lines, [int]$lineNo)
{
  for ($i = $lineNo - 1; $i -ge 0; $i--)
  {
    if ($lines[$i] -match '^\s*#?\s*frame\s+(\d+)\b') { return [int]$Matches[1] }
  }
  return 0
}

# A log this small is a run that failed, not a state that happens to be simple.
$minBytes = 2000
$maxShown = 10

foreach ($cfg in ($Configs -split ','))
{
  $name = $cfg.Trim() + "_L" + $L + "_" + $Frames + ".out"
  $prod = Join-Path $root (Join-Path $Produced $name)
  $exp  = Join-Path $root (Join-Path $Expectations $name)

  if (-not (Test-Path $prod))
  {
    Report 'ERROR' ("missing produced log: " + $Produced + "\" + $name + " (run experiments\check_trace.bat first)")
    continue
  }
  $size = (Get-Item $prod).Length
  if ($size -lt $minBytes)
  {
    Report 'ERROR' ("produced log is only " + $size + " bytes: " + $name + " -- the run failed rather than produced a state")
    continue
  }

  if ($Refresh)
  {
    Copy-Item $prod $exp -Force
    Report 'OK' ("refreshed " + $Expectations + "\" + $name + " (" + $size + " bytes, " + $cfg.Trim() + ")")
    continue
  }

  if (-not (Test-Path $exp))
  {
    Report 'ERROR' ("no expectation for this scenario: " + $Expectations + "\" + $name +
                    " -- create it with experiments\check_trace.bat refresh, and commit it")
    continue
  }

  $a = @(Get-Content $prod)
  $b = @(Get-Content $exp)
  $n = [Math]::Max($a.Count, $b.Count)
  $bad = New-Object System.Collections.ArrayList
  for ($i = 0; $i -lt $n; $i++)
  {
    $x = if ($i -lt $a.Count) { $a[$i] } else { $null }
    $y = if ($i -lt $b.Count) { $b[$i] } else { $null }
    if ($x -ne $y) { [void]$bad.Add([pscustomobject]@{ Line = $i + 1; Prod = $x; Exp = $y }) }
  }

  if ($bad.Count -eq 0)
  {
    Report 'OK' ("matches " + $Expectations + "\" + $name + " (" + $size + " bytes, " + $a.Count + " lines)")
    continue
  }

  $first = $bad[0]
  $frame = FrameOf $a ($first.Line - 1)
  Report 'ERROR' ($bad.Count.ToString() + " line(s) differ in " + $name +
                  " (first at line " + $first.Line + ", light frame " + $frame + "; " +
                  $a.Count + " produced lines against " + $b.Count + " expected)")
  foreach ($d in ($bad | Select-Object -First $maxShown))
  {
    Report 'INFO' ("line " + $d.Line + " produced: " + (Clip $d.Prod))
    Report 'INFO' ("line " + $d.Line + " expected: " + (Clip $d.Exp))
  }
  if ($bad.Count -gt $maxShown) { Report 'INFO' ("... and " + ($bad.Count - $maxShown) + " more differing line(s)") }
  Report 'INFO' ("if the change is intended: experiments\check_trace.bat refresh (then commit " + $Expectations + ")")
}

Write-Host ""
if ($Refresh) { Write-Host ("check-trace: " + $script:ok + " expectation(s) written") -ForegroundColor Green; exit 0 }
$color = 'Green'
if ($script:errors -gt 0) { $color = 'Red' }
Write-Host ("check-trace: " + $script:ok + " configuration(s) match, " + $script:errors + " differ or are missing") -ForegroundColor $color
if ($script:errors -gt 0) { exit 1 }
exit 0
