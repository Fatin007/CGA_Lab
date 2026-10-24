# 3D Solar System — the minimal version

This is the **simplest** version of the solar system project. It draws the
same picture as `OpenGL_3D_Art` and `OpenGL_3D_Art_V2`, but with as little
code around it as possible.

Everything is drawn with **GLSL shaders** on the modern **OpenGL 3.3 core
profile** — no legacy `glBegin`/`glEnd` immediate mode, and no texture files.

**Two deliberate restrictions:**

* **The camera can only be orbited and zoomed.** It always looks at the sun.
  There is no planet-following, no focus mode, no target point.
* **The animation speed is fixed.** There is no time scale and no speed
  slider. The simulation always runs at **1× real time** — the same as the
  default in `OpenGL_3D_Art` — no matter how fast the computer draws.

---

## Running it

The project needs **MSYS2 MinGW-w64** with GLFW and GLEW installed.

```bash
cd OpenGL_3D_Art_V3
./build.sh          # or: build.bat   (from cmd.exe)
./run.bat           # or: run.sh
```

`build.sh` / `build.bat` compile the program and copy the runtime DLLs next
to the executable, so it also runs by double-clicking `solar-system.exe`.

If the libraries are missing, install them with:

```bash
pacman -S mingw-w64-x86_64-glfw mingw-w64-x86_64-glew
```

### Controls

| Input | Action |
|---|---|
| Left drag | Rotate the camera around the sun |
| Right drag / mouse wheel | Zoom in and out |
| `SPACE` | Pause and un-pause |
| `R` | Restart the animation |
| `H` | Print the controls in the console |
| `ESC` | Quit |

---

## The whole project in one picture

```
OpenGL_3D_Art_V3/
├── main.cpp           # Entry point, input, the animation loop
├── render.hpp / .cpp  # The camera, the OpenGL objects, the draw calls
├── solar_system.hpp   # The planet data AND the simulation (header only)
├── geometry.hpp       # The four shape builders (header only)
├── shader.hpp         # Loads, compiles and links the GLSL programs
├── math3d.hpp         # Vec3 + Mat4 (column-major) -- header only
├── assets/shaders/
│   ├── planet.vert / .frag   # The planets: bands, terminator, atmosphere
│   ├── sun.vert / .frag      # The sun and its corona
│   ├── ring.vert / .frag     # The planetary rings
│   ├── line.vert / .frag     # The faint orbit lines
│   └── point.vert / .frag    # Every dot: the stars AND the asteroids
├── build.sh / build.bat
└── run.sh / run.bat
```

**Three `.cpp` files, five headers, five shader programs.** Four of the six
code files are header-only, so you can read the whole project top to bottom
without jumping between a header and its implementation.

---

## How it works

### 1. The shaders (5 programs)

| Program | What it draws |
|---|---|
| `planet` | The eight planets. Procedural bands, a day/night terminator, a specular highlight and an atmospheric rim. |
| `sun` | The sun's surface, and the corona (a billboard quad that always faces the camera). |
| `ring` | The rings of Saturn and Uranus, including the planet's shadow on them. |
| `line` | The faint orbit lines, as `GL_LINE_STRIP` chains. |
| `point` | Every dot in the scene — the 12,000 background stars *and* the 9,000 asteroids. |

The `point` program is worth pointing at: stars and asteroids are both
`GL_POINTS` with the same vertex layout, so a single `uIsStar` uniform tells
the shader which one it is drawing. That replaced two nearly identical
shader pairs.

### 2. The geometry (`geometry.hpp`)

There are no model files. Everything is built in code:

* `make_sphere()` — a UV sphere. We walk down it one ring at a time: the
  height is `radius * cos(phi)` and the circle radius is `radius * sin(phi)`,
  so a point on the ring is `x = circle_radius * cos(theta)`,
  `y = height`, `z = circle_radius * sin(theta)`. The normal of a sphere is
  simply the direction from the centre, so it is the position divided by the
  radius.
* `make_ring()` — Saturn's rings: a flat annulus in the XZ plane, with the UV
  running 0 → 1 from the inner edge to the outer one.
* `make_orbit_points()` — a circle of points for the orbit lines.
* `make_quad()` — the flat square used for the sun's corona.

All four use the same vertex layout: **position (3 floats), normal (3), UV (2)**.

### 3. The planet data (`solar_system.hpp`)

One table holds the **real** astronomical data — real radii, real distances,
real year lengths, real day lengths, real axial tilts. Venus and Uranus
really do spin backwards, and the code keeps the negative sign.

The table is easy to read, but the real solar system is a terrible shape for a
picture: Neptune is 30× further from the sun than Earth, yet only 4× bigger.
On a real scale the inner planets would be an invisible pile in the middle.

So the numbers are compressed in **one place**, in the constructor:

```cpp
radius       = pow(real_radius_ratio, 0.45)     // SIZE_POWER
orbit_radius = sqrt(real_distance_in_AU) * 9.0  // ORBIT_SCALE
```

The square root pulls the outer planets in while keeping the order the same.
The power makes the size differences smaller, so Jupiter comes out about three
times bigger than Earth on screen instead of eleven times, and Mercury is
still visible next to it. Both constants are file-local, so changing them and
seeing what happens is a two-second experiment.

### 4. The renderer (`render.cpp`)

The renderer owns all the GPU state. `initialise()` is deliberately a short
list of steps, with each step in its own function:

```cpp
bool Renderer::initialise()
{
    if (!load_shaders()) return false;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    const SolarSystem system;

    create_mesh(make_sphere(...), sphere_);
    create_mesh(make_quad(2.0f), corona_);

    build_planets(system);
    build_orbits(system);
    build_stars();
    build_asteroids(system);

    return true;
}
```

Its `draw()` function does this, in this order:

1. **Clear** the screen to a very dark blue.
2. **Stars** — depth test off, so they are always behind everything.
3. **Orbit lines** — faint blue-grey `GL_LINE_STRIP` chains.
4. **Asteroids** — 9,000 `GL_POINTS` of lit rock between Mars and Jupiter.
5. **Planets and their rings** — the planet first (it writes depth), then the
   rings (depth-tested, but not writing depth).
6. **The sun and its corona** — last, so the glow blends over everything.

### 5. The camera (`Camera` in `render.hpp`)

Three numbers and nothing else:

```cpp
float distance = 95.0f;   // how far from the sun
float yaw      = 35.0f;   // how far round, in degrees
float pitch    = 28.0f;   // how far up or down, in degrees
```

The position and the view matrix are rebuilt from them every frame. Because
the camera always looks at the origin (the sun), there is no `target` field
and no planet-following code at all.

The pitch is clamped to `[-18°, 85°]`. The lower limit is not -85° for a good
reason: at a shallow angle the orbits flatten into overlapping bands, and the
planets end up looking as if they are touching.

### 6. The animation

The speed is **fixed**: the animation always runs at **1× real time**, which
is the default speed in `OpenGL_3D_Art`. There is no time scale and no way to
speed it up or slow it down.

To do that, the main loop measures how long the last frame took:

```cpp
double previous_time = glfwGetTime();
...
const double now = glfwGetTime();
float delta_seconds = static_cast<float>(now - previous_time);
previous_time = now;

if (delta_seconds > MAX_STEP) delta_seconds = MAX_STEP;

app.time += delta_seconds;
app.system.update(delta_seconds);
```

`glfwGetTime()` is part of GLFW, so there is no `<chrono>` and no extra
library. The `MAX_STEP` clamp stops the planets from teleporting if the
program is paused in the background.

**Why measure the time at all?** Because this machine draws the scene at
about **1000 frames a second** (there is no vsync). If we advanced the
simulation by a fixed `1/60` of a second on every frame, the planets would
move roughly **17× too fast** — the animation would look frantic. Measuring
the real elapsed time means an Earth year takes the same **6 seconds** on
every computer, whether it renders at 30 FPS or 1000 FPS.

### 7. The maths (`math3d.hpp`)

GLM is not installed on the lab machine, so we wrote the handful of matrix
operations the project needs. It is under 200 lines and it is all commented.

Two conventions to remember:

1. `Mat4` holds 16 floats in **column-major** order, exactly as OpenGL wants.
   The element at row *r*, column *c* lives at `m[c * 4 + r]`.
2. A matrix is applied to a vector as `M * v` — the matrix is on the **left**.
   When we chain matrices we multiply them in the opposite order to how we
   read them.

---

## Points worth raising with faculty

1. **Everything is procedural.** No textures, no models, no asset pipeline.
   Every planet, band, star and ring is generated from maths inside the
   fragment shaders.

2. **The shaders are the artwork.** The lighting model — a point light at the
   sun, a soft terminator, an atmospheric rim, a specular highlight, and a
   planet's shadow falling across its own rings — is all done per-pixel in
   GLSL.

3. **Real data, adjusted for the screen.** The planet table holds real
   astronomical values. The compression is a separate, clearly-marked step,
   which is a nice example of separating *data* from *presentation*.

4. **Simplicity is a design decision.** This version throws away the
   planet-following camera, the speed control, the delta-time measurement and
   the split between headers and implementations — not because they were
   wrong, but because each one is a thing a reader has to hold in their head.
   The picture is identical.

5. **The code was verified by measuring, not by looking.** Throwaway test
   programs opened a real window, drew the real scene and counted the pixels:
   how many were lit, how many were stars, how many were rock in the belt,
   and how far apart the planets were on screen. That is how the "invisible
   stars", "missing rings" and "Venus is hitting Earth" bugs were found — all
   of them rendered perfectly happily while producing the wrong picture.

### Two bugs worth talking about

Both are good teaching examples, because the program **ran perfectly happily
while producing the wrong picture**.

**The Cassini division was inverted.** The ring shader cuts the famous gap
out of the rings like this:

```glsl
float in_gap = smoothstep(gap_width, gap_width * 2.5, abs(vUV.x - gap_centre));
```

The original version had a `1.0 -` in front of it, which made the ring
transparent everywhere *except* in the gap. The ring came out as two thin
arcs with the rest missing — it looked like the geometry had a hole in it,
but the mesh was perfect the whole time.

**Venus looked like it was hitting Earth.** Nothing was broken at all — the
geometry, the maths and the shaders were all correct. The planets were simply
*starting in the wrong places*. The original code used

```cpp
planet.orbit_angle = static_cast<float>(i) * 1.1f;
```

which puts Mercury, Venus, Earth, Mars and Jupiter within sixteen degrees of
each other. They were all on almost the same line of sight from the sun, so
from a low camera angle they collapsed into one vertical strip of pixels.

The fix was to **start the planets at the golden angle** (137.5° apart, which
gives the most even spread possible) and to **stop the camera from getting
too low** — a shallow pitch squashes the orbits into overlapping bands, so the
lowest pitch was raised from -85° to -18°.

---

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `solar-system.exe` will not start | The DLLs are missing. Run `build.bat` again, or add `C:\msys64\mingw64\bin` to your `PATH`. |
| "could not create the window" | The machine has no OpenGL 3.3 context. Check with `glxinfo` or on another machine. |
| "One or more shaders failed to load" | The `assets/shaders` folder is missing from next to the `.exe`. The paths are resolved relative to the executable, so the folder has to travel with it. |
| The window is black | The camera is probably inside the sun. Scroll out with the mouse wheel. |
| Everything is too dark | The day/night terminator is showing. Rotate the camera until the planet is on the sunlit side. |
