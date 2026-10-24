@echo off
REM ---------------------------------------------------------------------------
REM run.bat -- Runs the solar system.
REM
REM build.bat copies the runtime DLLs next to solar-system.exe, so in principle
REM you can just double-click the exe. This script does one more thing: it adds
REM the MSYS2 bin folder to the PATH as well, which is a safety net in case
REM the DLL copy was skipped.
REM ---------------------------------------------------------------------------

setlocal

set MINGW=C:\msys64\mingw64

if not exist solar-system.exe (
    echo [run] ERROR: solar-system.exe has not been built yet.
    echo [run] Run build.bat first.
    exit /b 1
)

set PATH=%MINGW%\bin;%PATH%

echo [run] Starting the solar system. Press ESC in the window to quit.
echo [run]

solar-system.exe

exit /b 0
