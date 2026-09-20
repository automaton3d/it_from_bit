@echo off
rem build_gui.bat -- builds the automaton GUI (CPU-only) from E:\it_from_bit sources.
rem
rem Two things matter here:
rem  * VCPKG_ROOT must point at the vcpkg with the libraries (freetype, brotli, bz2, zlib,
rem    glfw3).  The environment may already define it pointing at the Visual Studio bundled
rem    vcpkg, which has no libs, so it is set explicitly below.
rem  * the ODR gate runs first: it caught a real regression once (isqrt lost its `inline` in a
rem    shared header and the GUI could not link), and it costs seconds instead of a full build.
where cl >nul 2>&1
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set VCPKG_ROOT=E:\vcpkg\installed\x64-windows
cd /d E:\it_from_bit
if not exist obj mkdir obj
if not exist build mkdir build

nmake check-odr > build_odr.log 2>&1
if errorlevel 1 goto :odr_fail

rem /MP parallelises the compilation across cores; drop it for a strictly serial build.
nmake EXTRA_CPPFLAGS=/MP > build_gui.log 2>&1
if errorlevel 1 goto :build_fail

rem `assets` copies the logos from the root; the font directory it looks for (bin\fonts)
rem does not exist in this tree, so the font is copied explicitly to keep the GUI runnable
rem when launched from build\.
if not exist build\fonts mkdir build\fonts
copy /Y fonts\arial.ttf build\fonts >nul

echo build_gui.bat: OK -- build\automaton.exe
exit /b 0

:odr_fail
echo build_gui.bat: ODR gate FAILED -- duplicate symbols in shared headers.
echo Full log: E:\it_from_bit\build_odr.log
exit /b 1

:build_fail
echo build_gui.bat: build FAILED.
echo Full log: E:\it_from_bit\build_gui.log
exit /b 1
