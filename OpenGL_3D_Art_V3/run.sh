#!/usr/bin/env bash
# run.sh -- Runs the solar system.
#
# build.sh copies the runtime DLLs next to solar-system.exe, so in principle
# you can just run the exe. This script does one more thing: it adds the
# MSYS2 bin folder to the PATH as well, which is a safety net in case the
# DLL copy was skipped.

set -e

cd "$(dirname "$0")"

if [ ! -f solar-system.exe ]; then
    echo "[run] ERROR: solar-system.exe has not been built yet." >&2
    echo "[run] Run ./build.sh first." >&2
    exit 1
fi

MINGW_PREFIX="/c/msys64/mingw64"
if [ -d "${MINGW_PREFIX}/bin" ]; then
    export PATH="${MINGW_PREFIX}/bin:${PATH}"
fi

echo "[run] Starting the solar system. Press ESC in the window to quit."
echo "[run]"

./solar-system.exe
