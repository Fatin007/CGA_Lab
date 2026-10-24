#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# build.sh -- Compiles the chess project from an MSYS2 / MinGW-w64 shell.
#
# Usage:  ./build.sh
# ---------------------------------------------------------------------------

set -e

# ---------------------------------------------------------------------------
# Find the MinGW-w64 prefix.
#
# Depending on how the shell was started the prefix can be /mingw64, or the
# full Windows path (C:/msys64/mingw64). We look for the g++ compiler and
# work out the prefix from wherever we find it.
# ---------------------------------------------------------------------------
if command -v g++ >/dev/null 2>&1; then
    GXX_PATH="$(command -v g++)"
    # .../mingw64/bin/g++.exe  ->  .../mingw64
    MINGW_PREFIX="$(dirname "$(dirname "$GXX_PATH")")"
else
    echo "[build] ERROR: g++ not found."
    echo "[build] Install MSYS2 and run: pacman -S mingw-w64-x86_64-toolchain"
    exit 1
fi

if [ ! -d "${MINGW_PREFIX}/include" ]; then
    echo "[build] ERROR: no include folder at ${MINGW_PREFIX}/include"
    exit 1
fi

echo "[build] Using MinGW-w64 at ${MINGW_PREFIX}"

echo "[build] Compiling chess.exe ..."

g++ \
    main.cpp shader.cpp geometry.cpp board.cpp ai.cpp render.cpp \
    -o chess.exe \
    -I"${MINGW_PREFIX}/include" -L"${MINGW_PREFIX}/lib" \
    -lglfw3 -lglew32 -lopengl32 \
    -std=c++17 -Wall -Wextra \
    -O2

# ---------------------------------------------------------------------------
# Copy the runtime DLLs next to the executable.
#
# glfw3, glew32 and the C++ runtime are shared libraries, and MSYS2 does not
# ship static builds for this toolchain. Windows looks for a DLL next to the
# executable first, so copying them here means chess.exe runs by double-
# clicking it, with no PATH changes needed.
# ---------------------------------------------------------------------------
echo "[build] Copying the runtime DLLs ..."

for dll in glfw3.dll glew32.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
    if [ -f "${MINGW_PREFIX}/bin/${dll}" ]; then
        cp -f "${MINGW_PREFIX}/bin/${dll}" .
    else
        echo "[build] WARNING: ${dll} not found in ${MINGW_PREFIX}/bin"
    fi
done

echo "[build] Done. Run ./run.sh to play."
