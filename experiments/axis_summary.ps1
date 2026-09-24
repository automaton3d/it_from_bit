# axis_summary.ps1 -- folds a log of the election probe (/D AXIS_ELECTION_TRACE) into one row
# per light frame, so a decay of the standing axis alignment can be read against the elections
# that caused it.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File experiments\axis_summary.ps1 -Logs <log>,...
#
# Columns, all from the harness's own lines:
#   frame        light frame
#   m0           layers with m == 0 (axis-align)
#   align        the axis-align buckets 0/1/2/3: how many of the three signs of m agree with the
#                octant of the layer's own charge word
#   ownC/ownP/ownU  layers whose standing m was installed by the classic field candidate, by the
#                placement rule (POLAR_SEED_FROM_PLACEMENT), or by nothing traced (m == 0)
#   misC/misP    of those, how many are NOT parallel to the octant (fewer than three signs)
#   fst/re       elections so far: with m == 0 before (first) against with m already set (re)
#   elecC/elecP  installations by path, cumulative
#   al3C/al3P    of those, how many landed with all three signs matching

param([Parameter(Mandatory = $true)][string[]]$Logs)

$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $PSScriptRoot

# The logs print decimal points and the cultures differ, as in the other folds.
[System.Threading.Thread]::CurrentThread.CurrentCulture = [System.Globalization.CultureInfo]::InvariantCulture

$list = @()
foreach ($e in $Logs) { foreach ($one in ($e -split ',')) { if ($one.Trim()) { $list += $one.Trim() } } }

foreach ($rel in $list)
{
  $path = if ([System.IO.Path]::IsPathRooted($rel)) { $rel } else { Join-Path $root $rel }
  if (-not (Test-Path $path)) { Write-Host ("missing log: " + $rel) -ForegroundColor Red; continue }

  $rows = New-Object System.Collections.ArrayList
  $head = $null
  foreach ($ln in (Get-Content $path))
  {
    if (-not $head -and $ln -match '^# first-era trace') { $head = $ln }
    if ($ln -match 'frame (\d+) axis-align: m==0=(\d+)  sign-match 0/1/2/3 = (\d+)/(\d+)/(\d+)/(\d+)')
    {
      [void]$rows.Add([pscustomobject]@{
        Frame = [int]$Matches[1]; M0 = [int]$Matches[2]
        A0 = [int]$Matches[3]; A1 = [int]$Matches[4]; A2 = [int]$Matches[5]; A3 = [int]$Matches[6]
        OwnC = ''; OwnP = ''; OwnU = ''; MisC = ''; MisP = ''
        Fst = ''; Re = ''; ElecC = ''; ElecP = ''; Al3C = ''; Al3P = ''
      })
    }
    elseif ($ln -match 'frame (\d+) axis-owner: classic=(\d+) \(misaligned (\d+)\)  placement=(\d+) \(misaligned (\d+)\)  bootstrap=\d+  octant=\d+  untraced/m==0=(\d+)')
    {
      $r = $rows | Where-Object { $_.Frame -eq [int]$Matches[1] } | Select-Object -First 1
      if ($r) { $r.OwnC = [int]$Matches[2]; $r.MisC = [int]$Matches[3]; $r.OwnP = [int]$Matches[4]
                $r.MisP = [int]$Matches[5]; $r.OwnU = [int]$Matches[6] }
    }
    elseif ($ln -match 'frame (\d+) axis-elect: total=\d+ first=(\d+) re=(\d+) \| classic=(\d+) \(aligned3=(\d+)\) \| placement=(\d+) \(aligned3=(\d+)\)')
    {
      $r = $rows | Where-Object { $_.Frame -eq [int]$Matches[1] } | Select-Object -First 1
      if ($r) { $r.Fst = [int]$Matches[2]; $r.Re = [int]$Matches[3]; $r.ElecC = [int]$Matches[4]
                $r.Al3C = [int]$Matches[5]; $r.ElecP = [int]$Matches[6]; $r.Al3P = [int]$Matches[7] }
    }
  }

  Write-Host ""
  Write-Host ("== " + [System.IO.Path]::GetFileName($rel) + "   " + $head)
  $rows | Format-Table Frame, M0, @{n='align 0/1/2/3';e={ "$($_.A0)/$($_.A1)/$($_.A2)/$($_.A3)" }},
    @{n='owner C/P/U';e={ "$($_.OwnC)/$($_.OwnP)/$($_.OwnU)" }},
    @{n='misaligned C/P';e={ "$($_.MisC)/$($_.MisP)" }},
    @{n='elect first/re';e={ "$($_.Fst)/$($_.Re)" }},
    @{n='byPath C/P';e={ "$($_.ElecC)/$($_.ElecP)" }},
    @{n='aligned3 C/P';e={ "$($_.Al3C)/$($_.Al3P)" }} -AutoSize
}
