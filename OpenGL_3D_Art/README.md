# 3D Solar System — an OpenGL graphics project

A small, self-contained OpenGL 3.3 project that shows our solar system in 3D.
The sun sits in the middle with a glowing corona, eight planets travel along
their orbits, each planet spins on its own axis and leans over at its axial
tilt, Saturn and Uranus wear their rings, the asteroid belt turns slowly
between Mars and Jupiter, and a field of stars fills the sky.

Everything is drawn with **GLSL shaders** on the modern **OpenGL 3.3 core
profile** — no legacy `glBegin`/`glEnd` immediate mode, and no texture files.

![The whole solar system](shot_wide.png)

---

## Running it

The project needs **MSYS2 MinGW-w64** with GLFW and GLEW installed. If you
already have that (it is what the other projects in this repository use), you
are ready to go.

```bash
cd OpenGL_3D_Art
./build.sh          # or: build.bat   (from cmd.exe)
./run.bat           # or: run.sh
```

`build.sh` / `build.bat` compile the program and copy the runtime DLLs next
to the executable, so the program also runs by double-clicking
`solar-system.exe`.

If the libraries are missing, install them with:

```bash
pacman -S mingw-w64-x86_64-glfw mingw-w64-x86_64-glew
```

### Controls

| Input | Action |
|---|---|
| Left drag | Rotate the camera around the sun |
| Right drag / mouse wheel | Zoom in and out |
| `SPACE` | Pause and un-pause the animation |
| `←` / `→` | Slow down / speed up time |
| `F` | Follow the next planet (Mercury, Venus, … then back to the sun) |
| `1` | Look at the sun again |
| `R` | Restart the animation |
| `H` | Print the controls in the console |
| `ESC` | Quit |

The camera cannot be dragged below the plane of the orbits. At a very low
angle the orbits flatten into overlapping bands, and the planets end up
looking as if they are touching — see the third bug below.

---

## What is in the box

```
OpenGL_3D_Art/
├── main.cpp                  # Entry point, input handling, the animation loop
├── math3d.hpp                # Vec3 and Mat4 (column-major) -- header only
├── shader.hpp / shader.cpp   # Loads, compiles and links the GLSL programs
├── geometry.hpp / geometry.cpp # Builds the spheres, quads, orbits and rings
├── solar_system.hpp / .cpp   # The planet data and the simulation
├── render.hpp / render.cpp   # The camera, the OpenGL objects, the draw calls
├── assets/shaders/
│   ├── planet.vert / .frag   # The planets: bands, terminator, atmosphere
│   ├── sun.vert / .frag      # The sun and its corona
│   ├── star.vert / .frag     # The background starfield
│   ├── ring.vert / .frag     # The planetary rings
│   ├── line.vert / .frag     # The faint orbit lines
│   └── asteroid.vert / .frag # The rocky belt between Mars and Jupiter
├── build.sh / build.bat      # Build scripts
├── run.sh / run.bat          # Run scripts
└── shot_*.png                # Pictures of the finished project
```

The project has **no external asset files**. Every surface in the scene is
drawn from maths inside the fragment shaders, so the whole thing is one
repository with no download step.

---

## How it works

### The shaders

Six shader programs, each in its own `.vert` / `.frag` pair on disk:

| Program | What it draws |
|---|---|
| `planet` | The eight planets. Procedural bands, a day/night terminator, a specular highlight and an atmospheric rim. |
| `sun` | The sun's surface, and the corona (a billboard quad that always faces the camera). |
| `star` | The background stars, drawn as `GL_POINTS` with soft round dots. |
| `ring` | The rings of Saturn and Uranus, including the planet's shadow on them. |
| `line` | The faint orbit lines, as `GL_LINE_STRIP` chains. |
| `asteroid` | The asteroid belt, as 9,000 `GL_POINTS` of lit rock between Mars and Jupiter. |

Saturn's and Uranus's rings are quite different from each other, and the
project shows that: Saturn's are wide, bright and golden (water ice), while
Uranus's are narrow, dark and barely visible (rock), which is exactly how
they look through a real telescope. Each planet's ring colour, brightness and
inner and outer radii come from the same data table as everything else.

### The asteroid belt

The belt between Mars and Jupiter is a cloud of 9,000 points, and it is the
one piece of the scene that is not a sphere.

It is drawn the same way as the stars (`GL_POINTS`), but unlike the stars
the asteroids live in **real world space**, so they use the full view matrix
and they turn slowly around the sun as the animation runs.

Three details make it read as a belt rather than as a cloud of specks:

* **It is flat.** Each asteroid gets a random height above the plane of the
  orbits, and that height is *squared* before it is used. Squaring pushes
  the values towards zero, so most of the rocks stay very close to the
  plane — which is exactly what gives the belt its thin, disc-like shape
  when you look at it edge on.

* **It is spread evenly along its length.** The distance from the sun is
  taken as the square root of a random number, the same trick used for the
  planet orbits. Without it, every rock would pile up at the inner edge.

* **It is lit.** A point has no surface and therefore no normal, so a
  proper lighting calculation is not possible. Instead the vertex shader
  works out how much of the sun is in front of each rock as seen from the
  camera, and dims the ones on the far side of the belt. That is what stops
  the belt looking like a flat sheet of grey.

### The planet surfaces

There are no texture files. Instead, `planet.frag` builds the surface out of
three pieces of maths:

1. **A hash function** turns a 2D point into a number that looks random.
2. **Smooth noise** blends the four corners of a grid cell together, which
   gives soft cloudy patches instead of random dots.
3. **fBm (fractal Brownian motion)** stacks several octaves of that noise on
   top of each other, which is what gives the planets their rich, marbled look.

The gas giants use a sine wave of latitude for their stripes, sharpened with
`smoothstep` so the bands have defined edges.

> **A note on the hash function.** The usual trick is
> `fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123)`. We do **not** use
> it. On the Intel graphics card in the lab, `sin()` of a large number loses
> so much precision that the noise comes out in visible square blocks. We use
> an integer-based hash instead, which is exact.

### The scale problem, and how we solve it

The real solar system is a terrible shape for a picture. Neptune is 30 times
further from the sun than Earth, but its radius is only 4 times Earth's. If we
used real numbers, the inner planets would be an invisible pile in the middle
and the outer ones would be far away dots.

So the project stores the **real** numbers in a readable table, and compresses
them in one place:

```cpp
radius        = pow(real_radius_ratio, 0.45)     // SIZE_POWER
orbit_radius  = sqrt(real_distance_in_AU) * 9.0  // ORBIT_SCALE
```

The square root pulls the outer planets in while keeping the order the same.
The power makes the size differences smaller, so Jupiter comes out about three
times bigger than Earth on screen instead of eleven times, and Mercury is still
visible next to it. Both constants live in `solar_system.hpp`, so changing them
and seeing what happens is a two-second experiment.

The table in `solar_system.cpp` is the real data — real radii, real distances,
real year lengths, real day lengths, real axial tilts. Venus and Uranus really
do spin backwards, and the code keeps the negative sign.

### The animation

Two speeds per planet, both derived from the real data:

* `orbit_speed` — how fast it travels around the sun.
* `rotation_speed` — how fast it spins on its own axis.

One Earth year takes 6 seconds of simulated time, and one Earth day takes
1.6 seconds. The frame loop measures the time between frames with
`std::chrono::steady_clock` and passes it to `SolarSystem::update`, so the
animation runs at the same speed whatever the frame rate.

### The camera

The camera is described by three numbers — `distance`, `yaw` and `pitch` — and
the view matrix is rebuilt from them every frame. That is much easier to
control than moving a position and a look-at point separately, and it is the
same camera model as the 3D chess project.

---

## Points worth raising with faculty

1. **Everything is procedural.** No textures, no models, no asset pipeline.
   Every planet, band, star and ring is generated from maths. The whole
   project is about 3,200 lines of C++ and GLSL, and roughly a third of that
   is the hand-written matrix library.

2. **Hand-written matrix maths.** GLM is not installed on the lab machine, so
   `math3d.hpp` implements exactly the operations the project needs:
   `look_at`, `perspective`, `translate`, `scale`, `rotate_x/y/z`, `multiply`
   and a Gauss-Jordan `inverse`. It is under 400 lines and it is all
   explained in the comments.

3. **The shaders are the artwork.** The lighting model — a point light at the
   sun, a soft terminator, an atmospheric rim, a specular highlight, and a
   planet's shadow falling across its own rings — is all done per-pixel in
   GLSL.

4. **Real data, adjusted for the screen.** The planet table holds real
   astronomical values. The compression is a separate, clearly-marked step,
   which is a nice example of separating *data* from *presentation*.

5. **Everything was verified by measuring, not by looking.** The project was
   developed alongside throwaway test programs that opened a real window,
   drew the real scene and counted the pixels: how many were lit, how many
   were stars, how many were rock in the asteroid belt, and how far apart
   the planets were on screen. This is how the "invisible stars", "missing
   rings" and "Venus is hitting Earth" bugs were found — all of them
   rendered perfectly happily while producing the wrong picture, so looking
   at the window was never enough on its own.

![Saturn and its rings](shot_saturn.png)

### Three bugs worth talking about

All of them are good teaching examples because the program **ran perfectly
happily while producing the wrong picture**.

**The Cassini division was inverted.** The ring shader cuts the famous gap
out of the rings like this:

```glsl
float in_gap = smoothstep(gap_width, gap_width * 2.5, abs(vUV.x - gap_centre));
```

The original version had a `1.0 -` in front of it, which made the ring
transparent everywhere *except* in the gap. The ring came out as two thin
arcs with the rest missing — it looked like the geometry had a hole in it,
but the mesh was perfect the whole time. The fix was to check the maths
rather than the picture: a flat-shader test drew the same mesh in solid
green and showed a complete ellipse, which proved the problem was in the
shader.

**The ring was drawn in the wrong order.** The rings were drawn *before* the
planet, so the planet painted over the near half of the ring and it looked
like a chunk was missing. Swapping the order — planet first (writing depth),
then the rings (depth-tested, not writing depth) — lets the depth buffer sort
them properly, so the ring correctly passes both in front of and behind the
planet.

**Venus looked like it was hitting Earth.** This one is the most interesting,
because nothing was broken at all — the geometry, the maths and the shaders
were all correct. The planets were simply *starting in the wrong places*.

The original code spread them out like this:

```cpp
planet.orbit_angle = static_cast<float>(i) * 1.1f;
```

which puts Mercury, Venus, Earth, Mars and Jupiter within sixteen degrees
of each other. They were all on almost the same line of sight from the sun,
so from a low camera angle they collapsed into one vertical strip of pixels.
Measured properly, Venus and Earth were **3.9 pixels apart on screen when
their discs were 17.5 pixels across** — they really were overlapping.

The fix was two-fold:

* **Start the planets at the golden angle.** Stepping by 137.5° each time
  gives the most even spread possible, and after only eight steps no two
  planets are anywhere near each other. Venus and Earth are now 35 pixels
  apart, and the closest pair in the whole scene is 35 pixels apart.

* **Stop the camera from getting too low.** A shallow pitch squashes the
  orbits into overlapping bands, so the camera's lowest pitch was raised
  from -85° to -18°. At the old limit, Mars and Jupiter came within 8
  pixels of each other; at the new limit the closest they get is 27.

Then, because this is the kind of bug that is easy to argue about and hard
to eyeball, a small test program was written to measure it. It projects every
planet onto the screen, works out how big each disc is, and checks that no
two of them come within 10 pixels of each other.

Measuring the on-screen radius has one trap in it, which is worth knowing
about: you cannot get it by projecting the near side and the far side of the
planet. The line from the eye through the centre of a sphere is exactly the
line of sight, so those two points land on **the same pixel** — that is why
a sphere's silhouette is a cone from the eye rather than a line. The
silhouette is where the line from the eye just *grazes* the sphere, so the
correct measurement is to step one radius sideways, perpendicular to the
line of sight.

![Uranus and its faint rings](shot_uranus.png)

---

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| `solar-system.exe` will not start | The DLLs are missing. Run `build.bat` again, or add `C:\msys64\mingw64\bin` to your `PATH`. |
| "could not create the window" | The machine has no OpenGL 3.3 context. Check with `glxinfo` or on another machine. |
| "One or more shaders failed to load" | The `assets/shaders` folder is missing from next to the `.exe`. The paths are resolved relative to the executable, so the folder has to travel with it. |
| The window is black | The camera is probably inside the sun or pointing away. Press `1` to look at the sun and scroll to zoom out. |
| Everything is too dark | The day/night terminator is showing. Press `F` to follow a planet that is on the sunlit side, or rotate the camera. |
