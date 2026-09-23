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
goto :eof

:build
rem %1 = variant name only
if not exist obj\%1 mkdir obj\%1
echo [trace_build_variants] building %1
echo cl %FLAGS% %INC% %SRC% > build\cmdline_%1.txt
cl %FLAGS% %INC% %SRC% /Fo"obj\%1\\" /Fe:build\trace_%1.exe /link /SUBSYSTEM:CONSOLE > build\%1_build.txt 2>&1
if errorlevel 1 echo [trace_build_variants] %1 FAILED (see build\%1_build.txt)
goto :eof
