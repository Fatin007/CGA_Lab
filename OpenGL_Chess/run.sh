#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# run.sh -- Starts the chess game from an MSYS2 / MinGW-w64 shell.
#
# The DLLs the game needs are copied into this folder by build.sh, so the
# game runs straight away. We still add the MinGW bin folder to the PATH so
# the newest copy of each DLL is the one that gets used.
# ---------------------------------------------------------------------------

set -e

if [ ! -f chess.exe ]; then
    echo "[run] chess.exe not found. Building it first..."
    ./build.sh
fi

# Work out the MinGW prefix from wherever g++ lives.
if command -v g++ >/dev/null 2>&1; then
    MINGW_PREFIX="$(dirname "$(dirname "$(command -v g++)")")"
    export PATH="${MINGW_PREFIX}/bin:${PATH}"
fi

./chess.exe
