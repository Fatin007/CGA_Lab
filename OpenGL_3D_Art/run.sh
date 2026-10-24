#!/usr/bin/env bash
# run.sh -- Runs the solar system.
#
# build.sh copies the runtime DLLs next to solar-system.exe, so in principle
# you can just run ./solar-system.exe. This script does one more thing: it
# adds the MSYS2 bin folder to the PATH as well, which is a safety net in
# case the DLL copy was skipped.

set -e

cd "$(dirname "$0")"

if [ ! -f solar-system.exe ]; then
    echo "[run] ERROR: solar-system.exe has not been built yet." >&2
    echo "[run] Run ./build.sh first." >&2
    exit 1
fi

# --- Find the MSYS2 prefix ---
if command -v g++ >/dev/null 2>&1; then
    GXX_PATH="$(command -v g++)"
    MINGW_PREFIX="$(dirname "$(dirname "$GXX_PATH")")"
    export PATH="${MINGW_PREFIX}/bin:${PATH}"
fi

echo "[run] Starting the solar system. Press ESC in the window to quit."
echo "[run]"

./solar-system.exe
