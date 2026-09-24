@echo off
rem trace_build_variants.bat -- the flag-only builds behind the candidate measurements of
rem README ("Item 3 of the follow-up list" and the (a)/(b) sections) and of Sect. 8.4.
rem No source edit: every variant is the same harness and the same model with a different set
rem of /D macros.  Run from the repository root (cl must be on the PATH of a VS developer prompt):
rem
rem   experiments\trace_build_variants.bat
rem
rem Each variant produces build\trace_<name>.exe; run it as
rem   build\trace_<name>.exe <L> <s2b_target> <frames>
rem (the reference build of the paper is the GUI/automaton.exe with NO macro defined, i.e. the
rem "ref" variant plus the counters).
set INC=/I"src\include" /I"src\include\zlib" /I"src" /I"E:/vcpkg/installed/x64-windows/include" /I"E:/vcpkg/installed/x64-windows/include/freetype2"
set SRC=experiments\first_era_trace.cpp src\config.cpp src\model\initSim.cpp src\model\simulation.cpp src\model\interaction.cpp src\model\utils.cpp src\model\geometry.cpp src\model\polarization.cpp src\model\charges.cpp src\model\attractor.cpp src\model\wavefront.cpp
set BASE=/nologo /std:c++20 /O2 /EHsc /MD /D "NOMINMAX" /D "S2B_TRACE"
set DISP=/D "CHARGE_DISPERSION_FSM"
set PLC=/D "POLAR_SEED_FROM_PLACEMENT"

rem The flags go through the FLAGS variable, never through `call` arguments: cmd strips the quotes
rem of quoted arguments and keeps only %1..%9, which silently drops /D "..." macros.
set FLAGS=%BASE%
call :build ref
rem The dispersion ALONE, without the polar seed: the phase never closes (m == 0 on every
rem layer in every frame), so nothing ever stops the rule -- the latch re-arms at each era
rem edge and it fires once per ERA instead of once per run.  This is the variant that
rem separates "the dispersion" from "the dispersion as the seed of an election"; its logs
rem are build\disp_L7.out / build\disp_L9.out (README, "The dispersion alone, measured").
set FLAGS=%BASE% %DISP%
call :build disp
set FLAGS=%BASE% %PLC%
call :build polar
set FLAGS=%BASE% %DISP% %PLC%
call :build base
set FLAGS=%BASE% %DISP% /D "PAIR_SAME_OCTANT" %PLC%
call :build aonly
set FLAGS=%BASE% %DISP% /D "PAIR_OWN_AXIS_EXCHANGE" %PLC%
call :build own
set FLAGS=%BASE% %DISP% /D "PAIR_SAME_OCTANT" /D "PAIR_OWN_AXIS_EXCHANGE" %PLC%
call :build bothab
rem The election probe: the bothab configuration plus /D AXIS_ELECTION_TRACE, which records
rem which path installed each layer's m -- the classic field candidate, the placement rule, the
rem address bootstrap or the charge-octant install -- and how many signs of the installed axis
rem agree with the octant of the layer's own charge word.  Measurement only, no rule change;
rem folded by experiments\axis_summary.ps1 (README, "Who owns the standing axis").
set FLAGS=%BASE% %DISP% %PAIRS% %PLC% /D "AXIS_ELECTION_TRACE"
call :build axis
rem The fifth candidate configuration: the four macros plus POLAR_AXIS_FROM_CHARGE, i.e. what the
rem promotion would build if the anchored axis joined them.  Measured over the twelve eras against
rem the bothab run (README, "The counterfactual").
set FLAGS=%BASE% %DISP% %PAIRS% %PLC% /D "POLAR_AXIS_FROM_CHARGE"
call :build octaxis
goto :eof

:build
rem %1 = variant name only
if not exist obj\%1 mkdir obj\%1
echo [trace_build_variants] building %1
echo cl %FLAGS% %INC% %SRC% > build\cmdline_%1.txt
cl %FLAGS% %INC% %SRC% /Fo"obj\%1\\" /Fe:build\trace_%1.exe /link /SUBSYSTEM:CONSOLE > build\%1_build.txt 2>&1
if errorlevel 1 echo [trace_build_variants] %1 FAILED (see build\%1_build.txt)
goto :eof
