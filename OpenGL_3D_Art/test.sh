#!/usr/bin/env bash
# test.sh -- Builds and runs the project's test programs.
#
# There are two:
#
#   _smoke.exe  -- opens a real window, draws 120 frames and checks that
#                  something actually appeared on the screen.
#   _shot.exe   -- saves four screenshots (wide, inner system, Saturn, Earth)
#                  so the result can be looked at.
#
# Run this from MSYS2 bash.

set -e

cd "$(dirname "$0")"

if command -v g++ >/dev/null 2>&1; then
    GXX_PATH="$(command -v g++)"
    MINGW_PREFIX="$(dirname "$(dirname "$GXX_PATH")")"
else
    echo "[test] ERROR: g++ not found." >&2
    exit 1
fi

CXX_FLAGS=(
    -I"${MINGW_PREFIX}/include"
    -L"${MINGW_PREFIX}/lib"
    -lglfw3 -lglew32 -lopengl32
    -std=c++17 -Wall -Wextra
)

SOURCES="shader.cpp geometry.cpp solar_system.cpp render.cpp"

echo "[test] Building the smoke test ..."
"$GXX_PATH" _smoke.cpp $SOURCES -o _smoke.exe "${CXX_FLAGS[@]}"

echo "[test] Building the screenshot harness ..."
"$GXX_PATH" _shot.cpp $SOURCES -o _shot.exe "${CXX_FLAGS[@]}"

echo
echo "[test] ---- smoke test ----"
./_smoke.exe

echo
echo "[test] ---- screenshots ----"
./_shot.exe

echo
echo "[test] All tests passed."
