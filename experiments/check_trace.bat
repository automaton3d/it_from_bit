@echo off
rem check_trace.bat -- the trace regression: build a small set of configurations, run each on
rem the same scenario, and compare the logs with the expectations committed in
rem experiments\golden\.
rem
rem   experiments\check_trace.bat            compare; exit 1 on any difference
rem   experiments\check_trace.bat refresh    overwrite the expectations with what is produced
rem
rem Run from the repository root (cl must be on the PATH of a VS developer prompt).  The
rem scenario is L = 7, sieve closed (s2b_target = 16384) and 6 frames = one era, ending on the
rem era-1 cascade frame, which is where the four configurations separate by what they do:
rem nothing (ref), the dispersal alone (disp, 147 steps at f2), the encounter's transport
rem (base, 94 steps at f6) and the same transport with the two pair rules (bothab, 46 steps).
rem So one short run covers the frozen reference, the dispersal, the election and the pair
rem rules.  The runs are the cost (about 70 s each); the builds are about 15 s.
rem
rem The set is the reference plus the three configurations of the promotion decision
rem (README, "Decision pack"): ref, disp, base, bothab.
rem
rem Why a byte comparison is the right test here: the model is deterministic -- no RNG, no
rem address, no scan order -- so the same sources, the same flags, the same scenario and the
rem same automaton.cfg produce an identical log.  Anything that changes the dynamics shows up
rem as a diff, and a diff that is intended is accepted with `refresh`.
setlocal
cd /d E:\it_from_bit
set INC=/I"src\include" /I"src\include\zlib" /I"src" /I"E:/vcpkg/installed/x64-windows/include" /I"E:/vcpkg/installed/x64-windows/include/freetype2"
set SRC=experiments\first_era_trace.cpp src\config.cpp src\model\initSim.cpp src\model\simulation.cpp src\model\interaction.cpp src\model\utils.cpp src\model\geometry.cpp src\model\polarization.cpp src\model\charges.cpp src\model\attractor.cpp src\model\wavefront.cpp
set BASE=/nologo /std:c++20 /O2 /EHsc /MD /D "NOMINMAX" /D "S2B_TRACE"
set DISP=/D "CHARGE_DISPERSION_FSM"
set PLC=/D "POLAR_SEED_FROM_PLACEMENT"
set PAIRS=/D "PAIR_SAME_OCTANT" /D "PAIR_OWN_AXIS_EXCHANGE"
set L=7
set S2B=16384
set FRAMES=6

if not exist obj\trace_check        mkdir obj\trace_check
if not exist experiments\golden     mkdir experiments\golden

rem The flags go through FLAGS, never through `call` arguments: cmd strips the quotes of quoted
rem arguments and keeps only %1..%9, which would silently drop the /D "..." macros.
set FLAGS=%BASE%
call :one ref
set FLAGS=%BASE% %DISP%
call :one disp
set FLAGS=%BASE% %DISP% %PLC%
call :one base
set FLAGS=%BASE% %DISP% %PAIRS% %PLC%
call :one bothab

if /i "%~1"=="refresh" goto :refresh
powershell -NoProfile -ExecutionPolicy Bypass -File experiments\check_trace.ps1 -L %L% -Frames %FRAMES%
exit /b %errorlevel%

:refresh
powershell -NoProfile -ExecutionPolicy Bypass -File experiments\check_trace.ps1 -L %L% -Frames %FRAMES% -Refresh
exit /b %errorlevel%

:one
rem %1 = configuration name only
if not exist obj\check_%1 mkdir obj\check_%1
echo [check_trace] building %1
cl %FLAGS% %INC% %SRC% /Fo"obj\check_%1\\" /Fe:build\check_%1.exe /link /SUBSYSTEM:CONSOLE > obj\trace_check\%1_build.txt 2>&1
if errorlevel 1 (echo [check_trace] %1 FAILED to build -- see obj\trace_check\%1_build.txt & exit /b 1)
echo [check_trace] running %1 on L=%L% s2b=%S2B% frames=%FRAMES%
build\check_%1.exe %L% %S2B% %FRAMES% > obj\trace_check\%1_L%L%_%FRAMES%.out 2> obj\trace_check\%1_L%L%_%FRAMES%.err
if errorlevel 1 (echo [check_trace] %1 FAILED to run -- see obj\trace_check\%1_L%L%_%FRAMES%.err & exit /b 1)
goto :eof
