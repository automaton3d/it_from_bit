@echo off
rem row_gauge_build.bat -- builds build\row_gauge.exe, the offscreen measurement of the
rem status row's geometry.  Mirrors the repository's flags (build_gui.bat): the vcpkg path
rem must be the one with the libraries, not the Visual Studio bundled one.  The log lands
rem in build\row_gauge_build.txt, which is what experiments\row_gauge.cpp points at.
set "VCPKG_ROOT=E:\vcpkg\installed\x64-windows"
cd /d E:\it_from_bit
if not exist obj\gauge mkdir obj\gauge
cl /nologo /std:c++20 /O2 /EHsc /MD /D "NOMINMAX" ^
   /I"src\include" /I"src\include\zlib" /I"src" ^
   /I"E:/vcpkg/installed/x64-windows/include" ^
   /I"E:/vcpkg/installed/x64-windows/include/freetype2" ^
   experiments\row_gauge.cpp glad\glad.c src\text_renderer.cpp src\draw_utils.cpp ^
   src\Renderer2D.cpp src\projection_manager.cpp src\shader.cpp ^
   /Fo"obj\gauge\\" /Fe:build\row_gauge.exe ^
   /link /SUBSYSTEM:CONSOLE /LIBPATH:"E:/vcpkg/installed/x64-windows/lib" ^
   opengl32.lib glfw3dll.lib freetype.lib brotlidec.lib brotlicommon.lib bz2.lib ^
   zlib.lib user32.lib gdi32.lib shell32.lib kernel32.lib ole32.lib comdlg32.lib ^
   > build\row_gauge_build.txt 2>&1
if errorlevel 1 echo row_gauge_build.bat: FAILED (see build\row_gauge_build.txt)
if not errorlevel 1 echo row_gauge_build.bat: OK -- build\row_gauge.exe
