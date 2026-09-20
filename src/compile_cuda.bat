@echo off
setlocal enabledelayedexpansion

:: Set CUDA path
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2

:: Manually set environment variables for Visual Studio
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% neq 0 (
    echo Failed to set up Visual Studio environment
    exit /b 1
)

:: Create obj directory if it doesn't exist
if not exist "e:\alpha\src\obj" mkdir "e:\alpha\src\obj"

:: Compile with nvcc
nvcc -c -I"%CUDA_PATH%\include" -I"e:\alpha\src\include" -I"e:\alpha\src\include\zlib\cuda" e:\alpha\src\cuda\cuda_automaton.cu -o e:\alpha\src\obj\cuda_automaton.obj
if %ERRORLEVEL% neq 0 (
    echo Failed to compile cuda_automaton.cu
    exit /b 1
)

nvcc -c -I"%CUDA_PATH%\include" -I"e:\alpha\src\include" -I"e:\alpha\src\include\zlib\cuda" e:\alpha\src\cuda\bridge_cuda.cu -o e:\alpha\src\obj\bridge_cuda.obj
if %ERRORLEVEL% neq 0 (
    echo Failed to compile bridge_cuda.cu
    exit /b 1
)

:: Compile with cl.exe
cl /EHsc /std:c++17 /D_USE_CUDA /I"%CUDA_PATH%\include" /I"e:\alpha\src\include" e:\alpha\src\model\bridge.cpp /c /Foe:\alpha\src\obj\bridge.obj
if %ERRORLEVEL% neq 0 (
    echo Failed to compile bridge.cpp
    exit /b 1
)

:: Link the executable
cl /link /SUBSYSTEM:CONSOLE /OUT:e:\alpha\src\automaton.exe e:\alpha\src\obj\cuda_automaton.obj e:\alpha\src\obj\bridge_cuda.obj e:\alpha\src\obj\bridge.obj -L"%CUDA_PATH%\lib\x64" -lcudart -lcuda_rt
if %ERRORLEVEL% neq 0 (
    echo Failed to link the executable
    exit /b 1
)

echo Compilation successful!
endlocal