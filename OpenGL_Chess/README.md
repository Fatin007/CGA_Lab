# 3D Chess — OpenGL 3.3

A complete 3D chess game written in C++ with modern OpenGL (core profile 3.3)
and GLSL shaders. Play against the computer at three difficulty levels, or
against a friend on the same keyboard.

The project is deliberately written so that every part can be explained in a
presentation: no external maths library, no asset files for the pieces, no
GUI framework, and no hidden tricks.

---

## 1. Quick start

### Requirements

| Thing | Version used | How to get it |
|---|---|---|
| Compiler | g++ 16.2.0 (MSYS2 MinGW-w64) | `pacman -S mingw-w64-x86_64-toolchain` |
| GLFW | 3.x | `pacman -S mingw-w64-x86_64-glfw` |
| GLEW | 2.x | `pacman -S mingw-w64-x86_64-glew` |
| GPU driver | OpenGL 3.3 core profile | Any driver from 2013 onwards |

Everything is installed through **MSYS2** (`https://www.msys2.org/`), which
defaults to `C:\msys64`.

### Build and run

```
cd OpenGL_Chess
build.bat        REM Windows (cmd.exe)   -- or ./build.sh in MSYS2 bash
run.bat          REM Windows             -- or ./run.sh in MSYS2 bash
```

`build.bat` prints the compiler command it runs, so it is easy to show the
build step during a presentation.

> **About the DLLs.** `glfw3`, `glew32` and the C++ runtime are shared
> libraries, and MSYS2 does not ship static builds for this toolchain. The
> five DLLs the game needs are therefore copied into this folder:
>
> ```
> glfw3.dll   glew32.dll   libgcc_s_seh-1.dll
> libstdc++-6.dll   libwinpthread-1.dll
> ```
>
> Windows searches the executable's own folder first, so `chess.exe` runs by
> double-clicking it. `run.bat` is still the recommended way to start the
> game, because it also adds `C:\msys64\mingw64\bin` to the `PATH` — that way
> you always use the newest DLLs, even if the copies in this folder are out
> of date.

### The exact compile command

```
g++ main.cpp shader.cpp geometry.cpp board.cpp ai.cpp render.cpp -o chess.exe ^
    -IC:/msys64/mingw64/include -LC:/msys64/mingw64/lib ^
    -lglfw3 -lglew32 -lopengl32 -std=c++17 -Wall -O2
```

---

## 2. How to play

### The menu

The program asks in the console what to do *before* the window opens:

```
  1. Play against the Computer     -> then choose Easy / Medium / Hard
                                     -> then choose White or Black
  2. Play against a Friend (same computer)
  3. Run the chess rules self-test  -> 36 assertions, no window needed
  4. Quit
```

Choosing the menu in the console keeps the graphics code free of buttons and
dialogs, and it means the menu can be explained in one sentence.

### Controls

| Input | Action |
|---|---|
| **Left click** | Select a piece / play the move |
| **Left drag** | Rotate the camera around the board |
| **Right drag** | Zoom in and out |
| **R** | Restart the game |
| **F** | Flip the board (see it from the other side) |
| **M** | Print the board as ASCII in the console |
| **ESC** | Quit |

### Colours on the board

| Colour | Meaning |
|---|---|
| **Green square** | The piece you have selected |
| **Blue disc** | A legal destination for the selected piece |
| **Yellow squares** | The move that was played last |
| **Red square** | The king of the side to move is in check |

### The `M` key

`M` prints the position as text:

```
    +---+---+---+---+---+---+---+---+
  8 | r | n | b | q | k | b | n | r |
    +---+---+---+---+---+---+---+---+
  7 | p | p | p | p | p | p | p | p |
    +---+---+---+---+---+---+---+---+
  6 |   |   |   |   |   |   |   |   |
    +---+---+---+---+---+---+---+---+
  5 |   |   |   |   |   |   |   |   |
    +---+---+---+---+---+---+---+---+
  4 |   |   |   |   |   |   |   |   |
    +---+---+---+---+---+---+---+---+
  3 |   |   |   |   |   |   |   |   |
    +---+---+---+---+---+---+---+---+
  2 | P | P | P | P | P | P | P | P |
    +---+---+---+---+---+---+---+---+
  1 | R | N | B | Q | K | B | N | R |
    +---+---+---+---+---+---+---+---+
      a   b   c   d   e   f   g   h
  Side to move: White
```

Upper-case letters are White, lower-case are Black. This is a good thing to
show in a presentation: it proves the rules engine works completely
independently of the 3D view.

---

## 3. Project layout

```
OpenGL_Chess/
├── main.cpp                  Entry point: menu, game state, main loop
├── math3d.hpp                Vec3 / Mat4 — our own vector and matrix maths
├── shader.hpp / shader.cpp   GLSL loading, compiling and linking
├── geometry.hpp / geometry.cpp  Procedural shapes and the six chess pieces
├── board.hpp / board.cpp     The chess rules engine (no OpenGL at all)
├── ai.hpp / ai.cpp           Minimax + alpha-beta, running on its own thread
├── render.hpp / render.cpp   Camera, drawing, mouse picking
├── assets/shaders/
│   ├── basic.vert            Vertex shader
│   └── basic.frag            Fragment shader (Blinn-Phong lighting)
├── build.bat / build.sh      Compilation
├── run.bat / run.sh          Launch the game
├── test.sh                   Build and run every test suite
├── _test_pick.cpp            Test: board layout and mouse picking
├── _test_gl.cpp              Test: shaders, geometry, drawing
├── _test_ai.cpp              Test: the computer opponent
├── *.dll                     The runtime libraries the game needs
└── README.md
```

**The single most important structural decision:** `board.*` and `ai.*` contain
**no OpenGL code whatsoever**. The chess rules and the computer opponent are
plain C++ that can be compiled and tested on their own — which is exactly what
the self-test does. The graphics layer only ever *reads* a position and draws
it.

---

## 4. How it works

### 4.1 The rendering pipeline

```
model matrix  →  view matrix  →  projection matrix  →  clip space
 (piece →      (world →        (camera → clip)        → perspective
  world)        camera)                                divide → pixels
```

Every object is drawn with the same two shaders. The vertex shader sends
world-space position, normal and colour to the fragment shader, and the
fragment shader applies the lighting:

```
colour = ambient·colour + diffuse·light·colour + specular·light + rim·light
```

This is Blinn-Phong: one directional light plus ambient light, with a small
specular highlight on shiny surfaces and a rim term that makes the pieces
stand out against the board.

### 4.2 The geometry is generated in code

There are no model files. Each chess piece is a stack of primitives:

| Piece | Built from |
|---|---|
| Pawn | cylinder (base) + cone (body) + sphere (head) |
| Rook | cylinder + cylinder + 4 cubes (the battlements) |
| Knight | cone + cube + a slanted box (deliberately stylised) |
| Bishop | cylinder + cone + sphere |
| Queen | cylinder + cone + cylinder + sphere |
| King | cylinder + cone + cylinder + cross |

`geometry.cpp` has a generator for each primitive (`make_sphere`,
`make_cylinder`, `make_cone`, `make_cube`, `make_disc`, `make_flat_square`),
plus `transform_mesh` (move and scale a mesh) and `merge_meshes` (combine
several meshes into one). A piece builder is therefore just a short list:

```cpp
Mesh make_pawn(const math3d::Vec3& colour)
{
    std::vector<Mesh> parts;

    parts.push_back(transform_mesh(make_cylinder(0.16f, 0.04f, 20),
                                   math3d::Vec3(0.0f, 0.0f, 0.0f), 1.0f,
                                   shade(colour, 0.75f)));
    parts.push_back(transform_mesh(make_cone(0.13f, 0.26f, 20),
                                   math3d::Vec3(0.0f, 0.04f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_sphere(0.085f, 20, 14),
                                   math3d::Vec3(0.0f, 0.32f, 0.0f), 1.0f, colour));

    return merge_meshes(parts);
}
```

This is much easier to explain than a modelling pipeline, and it means the
whole project is text.

### 4.3 Draw calls

- The **64 board squares** are baked into **one** buffer, so the board is a
  single draw call. The light/dark pattern comes from a per-vertex colour.
- The **pieces** use **one buffer per piece type and colour** (6 types × 2
  colours = 12 buffers), each drawn with its own model matrix.
- The **markers and highlights** are two more flat meshes.

That is roughly 35 draw calls per frame — nothing for a modern GPU, and it
keeps the code understandable.

### 4.4 The camera

The camera orbits the board. We store three numbers — `yaw`, `pitch` and
`distance` — and rebuild the view matrix every frame with `look_at()`:

```cpp
math3d::Mat4 Camera::view_matrix() const
{
    return math3d::look_at(eye_position(), target, math3d::Vec3(0, 1, 0));
}
```

`pitch` is clamped so the camera can never look at the board edge-on, which
would make mouse picking impossible.

### 4.5 Mouse picking

Clicking a square has to turn a 2D cursor position into a 3D board square.
The classic approach is a **ray–plane intersection**:

1. Turn the cursor into normalised device coordinates (−1 … +1).
2. **Unproject** that point with the inverse of `projection × view`, once at
   the near plane and once at the far plane. The two points define a ray.
3. Find where that ray crosses the board plane `y = 0`.
4. Convert the world position back into a square.

The subtlety is in step 2: the inverse of a perspective matrix has to be
applied with the **perspective divide** (dividing by `w`). Forgetting that
divide is the classic picking bug — the ray then always points at the centre
of the screen, so every click selects the same square. `math3d.hpp` has a
helper, `unproject_from_screen`, that does the divide for us.

The matrix inverse is computed with **Gauss–Jordan elimination**: we build a
4×8 matrix with the matrix on the left and the identity on the right, then
use row operations to turn the left half into the identity. What is left on
the right is the inverse. It is slower than the closed-form cofactor formula,
but it is much easier to get right and much easier to explain.

### 4.6 The chess rules engine

The board is a plain 8×8 array of two-field structs:

```cpp
struct Piece { PieceType type = EMPTY; Color color = WHITE; };
```

Move generation follows the standard pattern:

1. **Pseudo-legal moves** — generate every destination each piece could
   reach, ignoring the king.
2. **Legal moves** — make each candidate move on a *copy* of the board and
   discard it if our own king is left in check.

That second step is the only place king safety is enforced, which is why
tricky cases such as an **en-passant capture that exposes the king along the
diagonal** come out right automatically: the same check catches them.

Rules implemented: movement, path blocking, capture, check, checkmate,
stalemate, castling (with rights, empty squares, and the king not being in
check or passing through an attacked square), en passant, promotion, the
50-move rule, and threefold repetition.

Deliberately **not** implemented, as a scoped decision: draw by insufficient
material.

### 4.7 Detecting a repetition

The threefold repetition rule turned out to matter more than expected, so it
is worth explaining.

The AI picks randomly among the moves that score exactly the same. In an
even position there are often several such moves — two knights that can both
go to the same square, a rook that can go to either side, and so on. Without
a repetition rule the two sides can shuffle the pieces back and forth for
ever, and the game never ends.

The detection is simple. Every position is reduced to a single number, a
**key**, by mixing together the piece placement, the side to move, the
castling rights and the en passant target. We keep a list of the keys of all
the positions reached so far, and the game is drawn when the current key has
appeared three times.

```cpp
// board.hpp
std::vector<std::uint64_t> position_keys_;
```

The list is pushed in `make_move` and popped in `undo_move`, so it always
describes the real line of play — and the AI's search, which makes and undoes
millions of moves, never pollutes it.

The AI's search turns the tracking **off** on its private copy of the board:

```cpp
board.set_track_repetitions(false);
```

This is not only a speed trick. The search tries moves and undoes them again
on a trial-and-error basis, so it never follows one real line of play. A
repetition it "found" would be an artefact of the search, not a real draw.
Switching the tracking off makes that impossible, and it also saves the
search the work of maintaining the list at every node — with the tracking on,
the Hard search took twice as long.

Moves are undone with a push/pop stack, so the rules are written **once** and
the AI search reuses them:

```cpp
board.make_move(move);   // pushes an undo record
board.undo_move(move);   // pops it
```

### 4.7 The computer opponent

A textbook **minimax search with alpha-beta pruning**:

```cpp
int minimax(board, depth, alpha, beta, white_to_move)
```

- `depth` — how many moves to look ahead (Easy 1, Medium 3, Hard 4)
- `alpha` — the best score the maximising player can already guarantee
- `beta` — the best score the minimising player can already guarantee

If we already know a move is worse than something the other side has, there
is no point exploring it further — that is what prunes the tree.

The evaluation function is **material + piece-square tables**. Rather than
six 64-number tables (384 magic numbers), we use a small 4×4 table per piece
type and mirror it, which captures the two ideas that matter — keep the king
at home, move your other pieces towards the centre — while staying short
enough to read on one screen.

Move ordering uses the **MVV-LVA** idea: try captures first, most valuable
victim first. Good moves are searched early, so bad branches get cut off
sooner, and the whole search gets faster.

#### Scoring a mate

A checkmate gets a huge score so the search always prefers it over any
ordinary position. But a *flat* mate score has a flaw: it cannot tell a mate
in one apart from a mate in two, so the computer would happily give up a
kill next move for one two moves later.

The fix is to reduce the mate score for every ply spent before the mate is
delivered:

```cpp
// in ai.cpp
const int MATE_SCORE   = 1000000;
const int MATE_PENALTY = 1000;

const int mate_score = MATE_SCORE - plies_from_root * MATE_PENALTY;
```

`plies_from_root` counts the moves played since the root move, so a mate
found right now is worth more than the same mate found one move later. The
penalty is far larger than any material score, so a mate always beats a
non-mate, and a mate in one always beats a mate in two.

> **A trap worth mentioning.** The penalty has to be measured from the
> *root*, not from the remaining search depth. The remaining depth counts
> *down* as the search goes deeper, so using it here would reward mates that
> are found *late* — exactly backwards.

On Easy the computer picks randomly among the moves that are within 150
centipawns of the best, which makes it beatable.

### 4.8 Threading

A depth-4 search takes a noticeable amount of time. If we ran it on the main
thread the window would freeze — which looks broken. Instead:

- The board is **copied** (about 200 bytes) and the worker thread searches
  its own copy, so the renderer never sees a half-finished position.
- The result is published under a `std::mutex`, and a `std::atomic<bool>`
  flag says when it is ready.
- The render loop **polls** that flag every frame.
- `cancel()` sets an `std::atomic<bool>` that the search checks at every node,
  so restarting the game interrupts a search immediately.
- The thread is **joined** before the board or the window is destroyed.

The result is that the window stays live and the camera can still be moved
while the computer thinks — which is a nice thing to show in a presentation.

---

## 5. The tests

There are four test suites, one per layer of the project. Run them all with:

```bash
./test.sh
```

### 5.1 The chess rules self-test

Option **3** in the menu runs 36 assertions on the rules engine. It needs no
window and no OpenGL context, because `board.cpp` has no graphics dependency
at all.

```
  [PASS] Starting position has exactly 20 legal moves
  [PASS] After 1.e4 Black also has exactly 20 legal moves
  [PASS] Fool's Mate position is checkmate
  [PASS] Stalemate position is detected correctly
  [PASS] White can castle king-side after clearing the path
  [PASS] En passant removes the black pawn that moved two squares
  [PASS] A pawn on the 7th rank has four promotion options
  [PASS] A pinned knight with no king escape is checkmate
  [PASS] The 50-move rule is detected
  [PASS] Two rounds of shuffling is a threefold repetition
  [PASS] Undo restores the exact same position
  ...
  === 36 passed, 0 failed ===
```

### 5.2 The picking test — `_test_pick.cpp`

Checks that the board layout round-trips for all 64 squares in both
orientations, and that every picked square really contains the point where
the ray hit the board.

### 5.3 The OpenGL smoke test — `_test_gl.cpp`

Opens a hidden window, compiles and links the real shader program, builds the
real scene geometry, draws a few frames and checks that no OpenGL error was
raised.

### 5.4 The AI test — `_test_ai.cpp`

Checks the three things that matter about the computer opponent:

```
  [PASS] the AI finds a mate in one
  [PASS] the AI always picks the quickest available mate
  [PASS] the AI plays a complete legal game that ends in a result
  [INFO] Easy: 0 ms, 20 positions searched
  [INFO] Medium: 20 ms, 1954 positions searched
  [INFO] Hard: 120 ms, 20377 positions searched
```

The middle one is the interesting one. The test position has three ways to
mate in one and several ways to mate in two; the AI is asked to pick eight
times and must choose a mate in one every single time. That is what proves
the depth-aware mate score works.

---

## 6. Points worth raising with faculty

1. **Modern OpenGL, not the legacy immediate mode.** The older examples in
   `OpenGL/` use `glBegin`/`glEnd`, which was removed from OpenGL 3.1 and
   later. This project uses a core profile, a vertex array object per mesh,
   and GLSL 3.30 shaders.
2. **Depth testing and face culling are enabled.** Without them you cannot
   draw a 3D scene — nearer objects would not hide farther ones.
3. **We wrote our own matrix maths** instead of downloading GLM. It is about
   300 lines, and it is a concrete artifact to explain.
4. **The rules engine has no graphics dependency.** That separation is what
   makes the chess logic testable, and the 36 self-test assertions are the
   proof.
5. **The AI runs on a separate thread** with a mutex-protected hand-off, so
   the UI never blocks.
6. **The mate score is depth-aware.** A flat mate score cannot tell a mate
   in one from a mate in two, so the AI would give up a kill next move for a
   slower one. Reducing the score by the number of plies already played fixes
   it, and there is a test that proves the AI always takes the quickest mate.
7. **A bug we found by testing, not by reading.** The AI was quietly giving
   up mate-in-ones. The cause was measuring the mate penalty from the
   *remaining search depth* instead of from the *root* — and the remaining
   depth counts down, so the penalty was rewarding mates that were found
   late. The threefold-repetition rule was added for a different reason, but
   it turned out to need the same kind of careful reasoning.
8. **Explicit scoping decisions** were made and are documented: no
   transposition table, no bitboards, no move animation, no texture loading,
   no networked multiplayer. Saying what you did *not* build is part of
   demonstrating engineering judgement.

---

## 7. Known limitations

- Promotions are chosen in the console rather than in a 3D dialog.
- The knight is a stylised shape, not a sculpted horse.
- There are no board coordinate labels in the window (no font renderer);
  the `M` key prints them to the console instead.
- Draw by insufficient material is not detected.
- Moves are not animated — pieces appear in their new square immediately.
