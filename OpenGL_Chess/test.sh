#!/usr/bin/env bash
# test.sh -- Build and run every test suite.
#
# The project has three separate test programs, because each one tests a
# different layer of the code:
#
#   _test_pick.cpp  the board layout and the mouse-picking maths
#   _test_gl.cpp    a smoke test: shaders, geometry and drawing actually work
#   _test_ai.cpp    the computer opponent: mates, a full game, and speed
#
# The chess rules themselves are tested inside chess.exe (menu option 3),
# because the rules engine has no OpenGL dependency and can be checked on its
# own.

set -e

cd "$(dirname "$0")"

if command -v g++ >/dev/null 2>&1; then
    GXX="$(command -v g++)"
else
    echo "[test] ERROR: g++ not found. Open MSYS2 and install mingw-w64-x86_64-gcc." >&2
    exit 1
fi

MINGW_PREFIX="$(dirname "$(dirname "$GXX")")"

CXXFLAGS="-std=c++17 -O2 -Wall -Wextra"
INCLUDES="-I${MINGW_PREFIX}/include"
LDFLAGS="-L${MINGW_PREFIX}/lib -lglfw3 -lglew32 -lopengl32"

echo "[test] Using compiler : $GXX"
echo

# --- 1. The chess rules self-test (runs inside chess.exe) ---
echo "======================================================"
echo " 1. Chess rules self-test"
echo "======================================================"
"$GXX" main.cpp shader.cpp geometry.cpp board.cpp ai.cpp render.cpp \
    -o chess.exe $CXXFLAGS $INCLUDES $LDFLAGS
printf '3\n' | ./chess.exe | grep -E '^\s*\[(PASS|FAIL)\]|passed, .* failed'

# --- 2. Board layout and mouse picking ---
echo
echo "======================================================"
echo " 2. Board layout and mouse picking"
echo "======================================================"
"$GXX" _test_pick.cpp board.cpp render.cpp geometry.cpp shader.cpp \
    -o _test_pick.exe $CXXFLAGS $INCLUDES $LDFLAGS
./_test_pick.exe

# --- 3. OpenGL smoke test ---
echo
echo "======================================================"
echo " 3. OpenGL smoke test"
echo "======================================================"
"$GXX" _test_gl.cpp board.cpp render.cpp geometry.cpp shader.cpp \
    -o _test_gl.exe $CXXFLAGS $INCLUDES $LDFLAGS
./_test_gl.exe

# --- 4. The computer opponent ---
echo
echo "======================================================"
echo " 4. Computer opponent"
echo "======================================================"
"$GXX" _test_ai.cpp board.cpp ai.cpp \
    -o _test_ai.exe $CXXFLAGS $INCLUDES $LDFLAGS
./_test_ai.exe

echo
echo "[test] All suites finished."
