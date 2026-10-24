@echo off
REM ---------------------------------------------------------------------------
REM build.bat -- Compiles the chess project on Windows.
REM
REM Run this from a plain cmd.exe. It expects MSYS2 MinGW-w64 to be installed
REM at C:\msys64 (the default location).
REM
REM If the compiler is not found, add C:\msys64\mingw64\bin to your PATH.
REM ---------------------------------------------------------------------------

setlocal

set MINGW=C:\msys64\mingw64

if not exist "%MINGW%\bin\g++.exe" (
    echo [build] ERROR: g++ not found at %MINGW%\bin\g++.exe
    echo [build] Install MSYS2 from https://www.msys2.org/ and try again.
    exit /b 1
)

if not exist "%MINGW%\lib\libglfw3.dll.a" (
    echo [build] ERROR: GLFW not found. Install it with:
    echo [build]   pacman -S mingw-w64-x86_64-glfw
    exit /b 1
)

if not exist "%MINGW%\lib\libglew32.dll.a" (
    echo [build] ERROR: GLEW not found. Install it with:
    echo [build]   pacman -S mingw-w64-x86_64-glew
    exit /b 1
)

echo [build] Compiling chess.exe ...

"%MINGW%\bin\g++.exe" ^
    main.cpp shader.cpp geometry.cpp board.cpp ai.cpp render.cpp ^
    -o chess.exe ^
    -I"%MINGW%\include" -L"%MINGW%\lib" ^
    -lglfw3 -lglew32 -lopengl32 ^
    -std=c++17 -Wall -Wextra ^
    -O2

if errorlevel 1 (
    echo [build] FAILED
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM Copy the runtime DLLs next to the executable.
REM
REM glfw3, glew32 and the C++ runtime are shared libraries, and MSYS2 does
REM not ship static builds for this toolchain. Windows looks for a DLL next
REM to the executable first, so copying them here means chess.exe runs by
REM double-clicking it, with no PATH changes needed.
REM ---------------------------------------------------------------------------
echo [build] Copying the runtime DLLs ...

copy /Y "%MINGW%\bin\glfw3.dll"            . >nul
copy /Y "%MINGW%\bin\glew32.dll"           . >nul
copy /Y "%MINGW%\bin\libgcc_s_seh-1.dll"   . >nul
copy /Y "%MINGW%\bin\libstdc++-6.dll"      . >nul
copy /Y "%MINGW%\bin\libwinpthread-1.dll"  . >nul

echo [build] Done. Run run.bat to play.
exit /b 0
