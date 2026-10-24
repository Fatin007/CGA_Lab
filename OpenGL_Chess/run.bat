@echo off
REM ---------------------------------------------------------------------------
REM run.bat -- Starts the chess game.
REM
REM The project folder already contains the DLLs the game needs:
REM
REM       glfw3.dll            window creation and input (GLFW)
REM       glew32.dll           OpenGL function loading (GLEW)
REM       libgcc_s_seh-1.dll   C runtime support
REM       libstdc++-6.dll      the C++ standard library
REM       libwinpthread-1.dll  threading support
REM
REM Windows looks for a DLL next to the executable first, so double-clicking
REM chess.exe works as well. This script is still useful because it also
REM adds C:\msys64\mingw64\bin to the PATH, which covers the case where the
REM DLLs in this folder are out of date compared to the ones MSYS2 has
REM installed.
REM ---------------------------------------------------------------------------

setlocal

set MINGW=C:\msys64\mingw64

if not exist "chess.exe" (
    echo [run] chess.exe not found. Building it first...
    call build.bat
    if errorlevel 1 (
        echo [run] Build failed.
        exit /b 1
    )
)

REM Put the DLL folder at the front of the PATH for this session.
set "PATH=%MINGW%\bin;%PATH%"

chess.exe

endlocal
