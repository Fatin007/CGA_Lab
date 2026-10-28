#!/usr/bin/env bash
# build.sh -- Compiles the solar system project.
#
# Run this from MSYS2 bash. It finds g++ automatically, wherever it is
# installed.

set -e

cd "$(dirname "$0")"

# --- Find the compiler ---
if command -v g++ >/dev/null 2>&1; then
    GXX_PATH="$(command -v g++)"
    MINGW_PREFIX="$(dirname "$(dirname "$GXX_PATH")")"
else
    echo "[build] ERROR: g++ not found." >&2
    echo "[build] Install MSYS2 from https://www.msys2.org/ and then run:" >&2
    echo "[build]   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-glfw mingw-w64-x86_64-glew" >&2
    exit 1
fi

echo "[build] Using MinGW-w64 at ${MINGW_PREFIX}"

# --- Compile ---
echo "[build] Compiling solar-system.exe ..."

"$GXX_PATH" \
    main.cpp render.cpp \
    -o solar-system.exe \
    -I"${MINGW_PREFIX}/include" -L"${MINGW_PREFIX}/lib" \
    -lglfw3 -lglew32 -lopengl32 \
    -std=c++17 -Wall -Wextra \
    -O2

# ---------------------------------------------------------------------------
# Copy the runtime DLLs next to the executable.
#
# Windows looks for a DLL next to the executable first, so copying them here
# means the program runs by double-clicking it, with no PATH changes needed.
# ---------------------------------------------------------------------------
echo "[build] Copying the runtime DLLs ..."

for dll in glfw3.dll glew32.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
    if [ -f "${MINGW_PREFIX}/bin/${dll}" ]; then
        cp -f "${MINGW_PREFIX}/bin/${dll}" .
    else
        echo "[build] WARNING: ${dll} not found in ${MINGW_PREFIX}/bin" >&2
    fi
done

echo "[build] Done. Run ./run.sh to see the solar system."
