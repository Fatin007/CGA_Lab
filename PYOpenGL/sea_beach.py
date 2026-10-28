#!/usr/bin/env python3
# -*- coding: utf-8 -*-


import math
import random
import sys
import time

import numpy as np
from OpenGL.GL import *
from OpenGL.GLU import *
from OpenGL.GLUT import *

# ----------------------------------------------------------------------------
# Global settings
# ----------------------------------------------------------------------------
WINDOW_TITLE = "Tropical Beach - PyOpenGL"
WIN_W, WIN_H = 1280, 720
FOV_Y = 55.0
Z_NEAR, Z_FAR = 0.5, 4000.0
FRAME_MS = 10                      # timer period (the real rate is vsync / CPU bound)

HAZE = (0.80, 0.89, 0.96)          # horizon colour == fog colour (seamless horizon)
FOG_START, FOG_END = 120.0, 2100.0

# Sun direction (unit vector pointing from the scene towards the sun)
SUN_ELEV = math.radians(13.0)
SUN_AZIM = math.radians(-22.0)     # 0 = straight ahead (-Z), negative = to the left
SUN_DIR = np.array([math.sin(SUN_AZIM) * math.cos(SUN_ELEV),
                    math.sin(SUN_ELEV),
                    -math.cos(SUN_AZIM) * math.cos(SUN_ELEV)])

# Camera: orbit around a point out on the water in front of the beach
CAM_TARGET = np.array([0.0, 1.5, -28.0])
CAM_DEFAULT = {"yaw": 0.0, "pitch": 5.5, "dist": 56.0}
CAM_LIMITS = {"yaw": (-85.0, 85.0), "pitch": (1.0, 55.0), "dist": (12.0, 140.0)}

# Water colours
SHALLOW = np.array((0.30, 0.83, 0.80))
MID = np.array((0.06, 0.52, 0.70))
DEEP = np.array((0.015, 0.20, 0.44))
SKY_REFLECT = np.array((0.66, 0.82, 0.95))
FOAM_WHITE = np.array((0.98, 1.00, 1.00))

OCEAN_HALF_WIDTH = 900.0
OCEAN_Z_NEAR = 40.0
OCEAN_Z_FAR = -2000.0


class State(object):
    """Mutable run-time state (kept in one object instead of many globals)."""

    def __init__(self):
        self.t = 0.0                         # animation clock (seconds)
        self.last = None                     # last perf_counter() value
        self.paused = False
        self.width, self.height = WIN_W, WIN_H
        self.fullscreen = False
        self.drag = None
        self.cam = dict(CAM_DEFAULT)         # smoothed camera
        self.goal = dict(CAM_DEFAULT)        # target camera (set by input)
        self.eye = np.array([0.0, 7.0, 28.0])
        self.frames = 0
        self.fps_clock = 0.0


S = State()

# Objects created in init_opengl()
QUAD = None
SPHERE_LIST = SPHERE_LOW_LIST = 0
SKY_LIST = BEACH_LIST = ISLAND_LIST = PROPS_LIST = 0
CLOUDS = []
BIRDS = []
OCEAN = {}
FOAM = {}
GLITTER = {}


# ----------------------------------------------------------------------------
# Small maths helpers (NumPy based, work on scalars and arrays)
# ----------------------------------------------------------------------------
def smoothstep(e0, e1, x):
    t = np.clip((np.asarray(x, dtype=np.float64) - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _hash2(ix, iy, seed=0.0):
    n = np.sin(ix * 127.1 + iy * 311.7 + seed * 74.7) * 43758.5453123
    return n - np.floor(n)


def value_noise(x, y, seed=0.0):
    """Smooth 2-D value noise in [0, 1]."""
    xi, yi = np.floor(x), np.floor(y)
    xf, yf = x - xi, y - yi
    u = xf * xf * (3.0 - 2.0 * xf)
    v = yf * yf * (3.0 - 2.0 * yf)
    a = _hash2(xi, yi, seed)
    b = _hash2(xi + 1.0, yi, seed)
    c = _hash2(xi, yi + 1.0, seed)
    d = _hash2(xi + 1.0, yi + 1.0, seed)
    return (a * (1.0 - u) + b * u) * (1.0 - v) + (c * (1.0 - u) + d * u) * v


def fbm(x, y, octaves=4, seed=0.0):
    total, amp, freq, norm = 0.0, 0.5, 1.0, 0.0
    for o in range(octaves):
        total = total + amp * value_noise(x * freq, y * freq, seed + o * 3.1)
        norm += amp
        amp *= 0.5
        freq *= 2.03
    return total / norm


def normalize(v):
    v = np.asarray(v, dtype=np.float64)
    return v / (np.linalg.norm(v) + 1e-12)


# ----------------------------------------------------------------------------
# Terrain model shared by every element (beach, sea, foam, props, camera)
#   x : along the shore,  z : towards the viewer,  y : up
#   The shoreline is a gently curved, slightly irregular bay.
# ----------------------------------------------------------------------------
def shore_z(x):
    return (-0.0009 * x * x
            + 2.5 * np.sin(0.021 * x + 0.4)
            + 1.2 * np.sin(0.053 * x + 2.0)
            + 0.5 * np.sin(0.13 * x + 1.0))


def beach_height(x, z):
    """Height of sand / sea bed. d = distance inland from the mean waterline."""
    d = z - shore_z(x)
    dp = np.maximum(d, 0.0)
    under = np.maximum(-0.18 + 0.30 * d, -4.0)                  # steep sea bed
    dunes = (0.40 * np.sin(0.040 * x + 0.07 * dp + 1.0)
             + 0.28 * np.sin(0.105 * x - 0.045 * dp + 2.0)) * smoothstep(10.0, 50.0, dp)
    berm = 0.10 * np.exp(-((dp - 11.0) / 3.0) ** 2)             # small storm berm
    over = -0.18 + 0.065 * dp + 0.0006 * dp * dp + dunes + berm
    h = np.where(d < 0.0, under, over)
    return h - 5.0 * smoothstep(220.0, 300.0, np.abs(x))        # beach tips sink at the ends


# ----------------------------------------------------------------------------
# Ocean wave model (sum of sine waves, wave fronts follow the shoreline)
# ----------------------------------------------------------------------------
WAVES = (  # amplitude, wavelength, direction (deg), phase
    (0.200, 26.0, 0.0, 0.0),
    (0.120, 15.5, 12.0, 1.7),
    (0.080, 9.8, -16.0, 3.1),
    (0.050, 6.6, 27.0, 4.4),
    (0.032, 4.6, -31.0, 0.9),
)
WAVE_SPEED = 0.55
GRAVITY = 9.81


def wave_envelope(s):
    """Amplitude envelope, s = distance seaward of the shoreline."""
    s_pos = np.maximum(s, 0.0)
    env = 0.40 + 0.50 * np.exp(-s_pos / 90.0) + 0.45 * np.exp(-((s - 6.0) / 4.5) ** 2)
    return env * np.where(s < 0.0, 0.35 + 0.65 * np.exp(np.minimum(s, 0.0) / 4.0), 1.0)


def wave_setup(X, Z):
    """Pre-compute everything that does not depend on time."""
    sz = shore_z(X)
    s = sz - Z
    q = Z - 0.85 * sz                       # wave-front coordinate hugging the shore
    s_pos = np.maximum(s, 0.0)
    comps = []
    for amp, wl, ang, ph in WAVES:
        k = 2.0 * math.pi / wl
        w = WAVE_SPEED * math.sqrt(GRAVITY * k)
        a = math.radians(ang)
        fade = amp / (1.0 + (s_pos / (60.0 + 25.0 * wl)) ** 2)   # small waves fade with distance
        phase0 = k * (q * math.cos(a) + X * math.sin(a)) + ph
        comps.append((fade, phase0, w))
    return wave_envelope(s), comps


def slice_setup(setup, j0):
    env, comps = setup
    return env[j0:], [(f[j0:], p[j0:], w) for f, p, w in comps]


def wave_eval(setup, t):
    env, comps = setup
    h = 0.0
    for fade, phase0, w in comps:
        th = phase0 - w * t
        h = h + fade * (np.sin(th) - 0.18 * np.cos(2.0 * th))   # sharper crests, flatter troughs
    return h * env


def wave_height(X, Z, t):
    return wave_eval(wave_setup(X, Z), t)


def swash_edge(xs, t):
    """Inland distance d where the moving water surface meets the sand."""
    sz = shore_z(xs)
    d = np.full_like(xs, 2.0, dtype=np.float64)
    for _ in range(3):
        a = wave_height(xs, sz + d, t) + 0.18
        d = np.where(a >= 0.0, a / (0.065 + 0.0012 * np.maximum(d, 0.0)), a / 0.30)
    return np.clip(d, -3.0, 14.0)


# ----------------------------------------------------------------------------
# Generic mesh / GLU helpers
# ----------------------------------------------------------------------------
def grid_indices(rows, cols):
    j, i = np.meshgrid(np.arange(rows - 1), np.arange(cols - 1), indexing="ij")
    a = (j * cols + i)
    b, c = a + 1, a + cols
    d = c + 1
    return np.stack([a, c, b, b, c, d], axis=-1).reshape(-1).astype(np.uint32)


def grid_normals(P):
    """Smooth normals for a (rows, cols, 3) grid whose rows run +z and cols +x."""
    Pi = np.gradient(P, axis=1)
    Pj = np.gradient(P, axis=0)
    N = np.cross(Pj, Pi)
    N /= np.linalg.norm(N, axis=2, keepdims=True) + 1e-12
    N[N[..., 1] < 0.0] *= -1.0
    return N


def make_mesh_list(P, N, C):
    """Compile a static indexed mesh (vertex arrays) into a display list."""
    rows, cols = P.shape[:2]
    idx = grid_indices(rows, cols)
    pv = np.ascontiguousarray(P.reshape(-1, 3), dtype=np.float32)
    nv = np.ascontiguousarray(N.reshape(-1, 3), dtype=np.float32)
    cv = np.ascontiguousarray(C.reshape(-1, 3), dtype=np.float32)
    glEnableClientState(GL_VERTEX_ARRAY)
    glEnableClientState(GL_NORMAL_ARRAY)
    glEnableClientState(GL_COLOR_ARRAY)
    glVertexPointer(3, GL_FLOAT, 0, pv)
    glNormalPointer(GL_FLOAT, 0, nv)
    glColorPointer(3, GL_FLOAT, 0, cv)
    lst = glGenLists(1)
    glNewList(lst, GL_COMPILE)
    glDrawElements(GL_TRIANGLES, int(idx.size), GL_UNSIGNED_INT, idx)
    glEndList()
    glDisableClientState(GL_COLOR_ARRAY)
    glDisableClientState(GL_NORMAL_ARRAY)
    glDisableClientState(GL_VERTEX_ARRAY)
    return lst


def draw_cylinder_between(p0, p1, r0, r1, slices=8):
    d = np.array(p1, dtype=np.float64) - np.array(p0, dtype=np.float64)
    length = float(np.linalg.norm(d))
    if length < 1e-6:
        return
    glPushMatrix()
    glTranslatef(float(p0[0]), float(p0[1]), float(p0[2]))
    ax = np.cross((0.0, 0.0, 1.0), d / length)
    if np.linalg.norm(ax) < 1e-6:
        if d[2] < 0.0:
            glRotatef(180.0, 1.0, 0.0, 0.0)
    else:
        angle = math.degrees(math.acos(max(-1.0, min(1.0, d[2] / length))))
        glRotatef(angle, float(ax[0]), float(ax[1]), float(ax[2]))
    gluCylinder(QUAD, r0, r1, length, slices, 1)
    glPopMatrix()


def build_sphere_lists():
    global SPHERE_LIST, SPHERE_LOW_LIST
    SPHERE_LIST = glGenLists(1)
    glNewList(SPHERE_LIST, GL_COMPILE)
    gluSphere(QUAD, 1.0, 14, 10)
    glEndList()
    SPHERE_LOW_LIST = glGenLists(1)
    glNewList(SPHERE_LOW_LIST, GL_COMPILE)
    gluSphere(QUAD, 1.0, 10, 7)
    glEndList()


# ----------------------------------------------------------------------------
# Sky dome, sun
# ----------------------------------------------------------------------------
SKY_STOPS_Y = [0.0, 0.08, 0.25, 0.55, 1.0]
SKY_STOPS_C = np.array([(0.80, 0.89, 0.96), (0.66, 0.83, 0.96), (0.44, 0.69, 0.93),
                        (0.24, 0.52, 0.86), (0.10, 0.34, 0.74)])


def sky_color(v):
    y = max(float(v[1]), 0.0)
    c = np.array([np.interp(y, SKY_STOPS_Y, SKY_STOPS_C[:, k]) for k in range(3)])
    cosang = float(np.dot(v, SUN_DIR))
    glow = math.exp(-(1.0 - cosang) * 16.0) * 0.45 + math.exp(-(1.0 - cosang) * 150.0) * 0.55
    warm = np.array((1.0, 0.80, 0.52))
    c = c * (1.0 - 0.55 * glow) + warm * 0.55 * glow
    return np.clip(c, 0.0, 1.0)


def build_sky_list():
    elevs = [-14, -4, 0, 2, 4, 7, 10, 14, 19, 25, 32, 40, 50, 62, 75, 90]
    n_az, radius = 96, 2400.0
    rings = []
    for e in elevs:
        el = math.radians(e)
        ring = []
        for i in range(n_az + 1):
            az = 2.0 * math.pi * i / n_az
            v = np.array((math.cos(el) * math.sin(az), math.sin(el), -math.cos(el) * math.cos(az)))
            ring.append((v * radius, sky_color(v)))
        rings.append(ring)
    lst = glGenLists(1)
    glNewList(lst, GL_COMPILE)
    for r in range(len(rings) - 1):
        glBegin(GL_QUAD_STRIP)
        for i in range(n_az + 1):
            for ring in (rings[r], rings[r + 1]):
                p, c = ring[i]
                glColor3f(float(c[0]), float(c[1]), float(c[2]))
                glVertex3f(float(p[0]), float(p[1]), float(p[2]))
        glEnd()
    glEndList()
    return lst


def draw_sky():
    """Gradient dome centred on the camera (never moves relative to the viewer)."""
    glDisable(GL_LIGHTING)
    glDisable(GL_FOG)
    glDisable(GL_DEPTH_TEST)
    glDepthMask(GL_FALSE)
    glPushMatrix()
    glTranslatef(float(S.eye[0]), float(S.eye[1]), float(S.eye[2]))
    glCallList(SKY_LIST)
    glPopMatrix()
    glDepthMask(GL_TRUE)
    glEnable(GL_DEPTH_TEST)


SUN_LAYERS = (  # angular radius (deg), centre RGBA, edge alpha
    (1.35, (1.00, 0.98, 0.88, 1.00), 0.85),
    (4.00, (1.00, 0.92, 0.70, 0.55), 0.0),
    (11.0, (1.00, 0.86, 0.58, 0.28), 0.0),
    (28.0, (1.00, 0.80, 0.50, 0.12), 0.0),
)


def draw_sun(t):
    """Sun disc plus layered translucent glow, drawn additively at infinity."""
    glDisable(GL_LIGHTING)
    glDisable(GL_FOG)
    glDisable(GL_DEPTH_TEST)
    glDepthMask(GL_FALSE)
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE)
    right = normalize(np.cross(SUN_DIR, (0.0, 1.0, 0.0)))
    up = np.cross(right, SUN_DIR)
    dist = 2200.0
    centre = SUN_DIR * dist
    shimmer = 1.0 + 0.04 * math.sin(1.3 * t) + 0.02 * math.sin(3.1 * t + 1.0)
    glPushMatrix()
    glTranslatef(float(S.eye[0]), float(S.eye[1]), float(S.eye[2]))
    for deg, col, edge_a in SUN_LAYERS:
        rad = dist * math.tan(math.radians(deg))
        glBegin(GL_TRIANGLE_FAN)
        glColor4f(col[0], col[1], col[2], min(1.0, col[3] * shimmer))
        glVertex3f(float(centre[0]), float(centre[1]), float(centre[2]))
        for i in range(49):
            a = 2.0 * math.pi * i / 48.0
            p = centre + (right * math.cos(a) + up * math.sin(a)) * rad
            glColor4f(col[0], col[1], col[2], edge_a)
            glVertex3f(float(p[0]), float(p[1]), float(p[2]))
        glEnd()
    glPopMatrix()
    glDisable(GL_BLEND)
    glDepthMask(GL_TRUE)
    glEnable(GL_DEPTH_TEST)


# ----------------------------------------------------------------------------
# Beach (static geometry compiled once)
# ----------------------------------------------------------------------------
def beach_colors(X, D):
    dry_a = np.array((0.94, 0.82, 0.61))
    dry_b = np.array((0.88, 0.74, 0.52))
    wet = np.array((0.60, 0.48, 0.35))
    patch = fbm(X * 0.05, D * 0.07, 4, 2.0)[..., None]
    c = dry_a * (1.0 - patch) + dry_b * patch
    c = c + (value_noise(X * 0.9, D * 0.9, 5.0) - 0.5)[..., None] * 0.07        # grain
    dpos = np.maximum(D, 0.0)
    w = (0.9 * (1.0 - smoothstep(1.0, 11.0, dpos)))[..., None]                   # wet -> dry
    c = c * (1.0 - w) + wet * w
    wrack = (np.exp(-((D - 11.5) / 0.9) ** 2) * (0.4 + 0.6 * value_noise(X * 0.5, 3.0)))[..., None]
    c = c * (1.0 - 0.55 * wrack) + np.array((0.30, 0.26, 0.20)) * 0.55 * wrack   # seaweed line
    f = smoothstep(0.0, 14.0, -D)[..., None]                                     # sea bed fades to water
    under = wet * (1.0 - f) + np.array((0.08, 0.50, 0.66)) * f
    c = np.where((D < 0.0)[..., None], under, c)
    return np.clip(c, 0.0, 1.0)


def build_beach_list():
    xs = np.linspace(-300.0, 300.0, 241)
    ds = np.concatenate([np.linspace(-22.0, -8.0, 8, endpoint=False),
                         np.linspace(-8.0, 0.0, 10, endpoint=False),
                         np.linspace(0.0, 12.0, 40, endpoint=False),
                         np.linspace(12.0, 40.0, 28, endpoint=False),
                         np.linspace(40.0, 100.0, 21)])
    X, D = np.meshgrid(xs, ds)
    Z = shore_z(X) + D
    H = beach_height(X, Z)
    P = np.stack([X, H, Z], axis=-1)
    return make_mesh_list(P, grid_normals(P), beach_colors(X, D))


# ----------------------------------------------------------------------------
# Distant islands (depth layers on the horizon)
# ----------------------------------------------------------------------------
def build_island_list():
    specs = [(-330.0, -780.0, 170.0, 70.0, 1.0),
             (380.0, -560.0, 95.0, 30.0, 4.0),
             (60.0, -1150.0, 120.0, 38.0, 9.0)]
    lists = glGenLists(len(specs))
    for n, (cx, cz, R, Hh, seed) in enumerate(specs):
        nr, nth = 36, 72
        r = np.linspace(0.0, 1.0, nr + 1)[:, None]
        th = np.linspace(0.0, 2.0 * math.pi, nth + 1)[None, :]
        wob = 1.0 + 0.30 * (fbm(np.cos(th) * 2.0 + seed, np.sin(th) * 2.0 + seed, 3, seed) - 0.5)
        rr = r / wob
        prof = np.exp(-(rr / 0.55) ** 2.4)
        X = cx + r * R * np.cos(th)
        Z = cz + r * R * np.sin(th)
        hills = 0.65 + 0.7 * fbm(X * 0.02, Z * 0.02, 4, seed)
        H = Hh * prof * hills - 1.5
        P = np.stack([X, H, Z], axis=-1)
        Pi = np.gradient(P, axis=1)
        Pj = np.gradient(P, axis=0)
        N = np.cross(Pi, Pj)
        N /= np.linalg.norm(N, axis=2, keepdims=True) + 1e-12
        N[N[..., 1] < 0.0] *= -1.0
        t = smoothstep(2.0, Hh * 0.8, H)[..., None]
        green = np.array((0.16, 0.45, 0.17)) * (1.0 - t) + np.array((0.06, 0.26, 0.11)) * t
        green = green * (0.8 + 0.4 * value_noise(X * 0.05, Z * 0.05, seed))[..., None]
        sand = np.array((0.88, 0.78, 0.58))
        ms = smoothstep(0.2, 1.8, H)[..., None]
        C = np.clip(sand * (1.0 - ms) + green * ms, 0.0, 1.0)
        lst = make_mesh_list(P, N, C)
        glNewList(lists + n, GL_COMPILE)
        glCallList(lst)
        glEndList()
    out = glGenLists(1)
    glNewList(out, GL_COMPILE)
    for n in range(len(specs)):
        glCallList(lists + n)
    glEndList()
    return out


# ----------------------------------------------------------------------------
# Beach details: rocks, shells, driftwood (static display list)
# ----------------------------------------------------------------------------
def build_rock_list(seed):
    rng = random.Random(seed)
    nlat, nlon = 7, 10
    grid = []
    for i in range(nlat + 1):
        phi = math.pi * i / nlat
        pole_r = 1.0 + rng.uniform(-0.12, 0.12)
        row = []
        for j in range(nlon):
            th = 2.0 * math.pi * j / nlon
            r = pole_r if i in (0, nlat) else 1.0 + rng.uniform(-0.22, 0.22)
            x = r * math.sin(phi) * math.cos(th)
            y = max(r * math.cos(phi), -0.55)              # flat bottom
            z = r * math.sin(phi) * math.sin(th)
            row.append(np.array((x, y, z)))
        grid.append(row)
    base = rng.uniform(0.38, 0.52)
    lst = glGenLists(1)
    glNewList(lst, GL_COMPILE)
    glBegin(GL_QUADS)
    for i in range(nlat):
        for j in range(nlon):
            v0, v1 = grid[i][j], grid[i][(j + 1) % nlon]
            v2, v3 = grid[i + 1][(j + 1) % nlon], grid[i + 1][j]
            n = np.cross(v2 - v0, v3 - v1)
            ln = np.linalg.norm(n)
            n = n / ln if ln > 1e-9 else np.array((0.0, 1.0, 0.0))
            if np.dot(n, (v0 + v1 + v2 + v3)) < 0.0:
                n = -n
            shade = base * rng.uniform(0.88, 1.12)
            glColor3f(shade * 1.02, shade * 0.97, shade * 0.90)
            glNormal3f(float(n[0]), float(n[1]), float(n[2]))
            for v in (v0, v1, v2, v3):
                glVertex3f(float(v[0]), float(v[1]), float(v[2]))
    glEnd()
    glEndList()
    return lst


def build_scallop_list():
    lst = glGenLists(1)
    glNewList(lst, GL_COMPILE)
    n = 11
    glBegin(GL_TRIANGLE_FAN)
    glNormal3f(0.0, 1.0, 0.0)
    glColor3f(0.98, 0.90, 0.86)
    glVertex3f(0.0, 0.07, 0.0)
    for k in range(n + 1):
        a = math.radians(-72.0 + 144.0 * k / n)
        r = 0.5 * (1.0 - 0.18 * abs(a) / 1.2566)
        if k % 2:
            glColor3f(0.92, 0.76, 0.74)
        else:
            glColor3f(0.98, 0.90, 0.86)
        glVertex3f(r * math.cos(a), 0.03 + 0.05 * (k % 2), r * math.sin(a))
    glEnd()
    glEndList()
    return lst


def build_cone_shell_list():
    lst = glGenLists(1)
    glNewList(lst, GL_COMPILE)
    glColor3f(0.95, 0.90, 0.80)
    glPushMatrix()
    glTranslatef(-0.2, 0.10, 0.0)
    glRotatef(90.0, 0.0, 1.0, 0.0)
    gluCylinder(QUAD, 0.14, 0.0, 0.45, 10, 1)
    glPopMatrix()
    glEndList()
    return lst


def put_driftwood(rng, x0, d0, yaw_deg, length, radius):
    ang = math.radians(yaw_deg)
    n = 7
    pts, rads = [], []
    for i in range(n + 1):
        u = i / n - 0.5
        px = x0 + math.cos(ang) * length * u + rng.uniform(-0.12, 0.12)
        pz = shore_z(x0) + d0 - math.sin(ang) * length * u + rng.uniform(-0.12, 0.12)
        py = float(beach_height(px, pz)) + radius * 0.55
        pts.append((px, py, pz))
        rads.append(radius * (0.80 + 0.35 * rng.random()) * (1.0 - 0.35 * abs(2.0 * u)))
    glColor3f(0.55, 0.48, 0.40)
    for i in range(n):
        draw_cylinder_between(pts[i], pts[i + 1], rads[i], rads[i + 1], 8)
    for p, r in zip(pts, rads):
        glPushMatrix()
        glTranslatef(*p)
        glScalef(r, r, r)
        glCallList(SPHERE_LOW_LIST)
        glPopMatrix()
    mid = pts[3]
    tip = (mid[0] + 0.5, mid[1] + 0.55, mid[2] - 0.2)
    draw_cylinder_between(mid, tip, radius * 0.45, radius * 0.18, 6)


def build_props_list():
    rng = random.Random(21)
    rocks = [build_rock_list(100 + i) for i in range(4)]
    scallop = build_scallop_list()
    cone = build_cone_shell_list()

    def put_rock(x, d, size):
        z = float(shore_z(x)) + d
        y = float(beach_height(x, z))
        glPushMatrix()
        glTranslatef(x, y + 0.25 * size, z)
        glRotatef(rng.uniform(0.0, 360.0), 0.0, 1.0, 0.0)
        glScalef(size * rng.uniform(0.9, 1.5), size * rng.uniform(0.6, 0.85), size * rng.uniform(0.8, 1.3))
        glCallList(rng.choice(rocks))
        glPopMatrix()

    def put_shell(x, d, size, lst):
        z = float(shore_z(x)) + d
        y = float(beach_height(x, z))
        glPushMatrix()
        glTranslatef(x, y + 0.01, z)
        glRotatef(rng.uniform(0.0, 360.0), 0.0, 1.0, 0.0)
        glRotatef(rng.uniform(-8.0, 8.0), 0.0, 0.0, 1.0)
        glScalef(size, size, size)
        glCallList(lst)
        glPopMatrix()

    out = glGenLists(1)
    glNewList(out, GL_COMPILE)
    for cx, cd, n, smin, smax in ((-21.0, 11.5, 6, 0.6, 1.7), (26.0, 7.0, 5, 0.5, 1.4),
                                  (-4.0, 19.0, 2, 0.4, 0.9)):
        for _ in range(n):
            put_rock(cx + rng.uniform(-4.0, 4.0), cd + rng.uniform(-3.0, 3.0), rng.uniform(smin, smax))
    put_rock(-9.0, -1.2, 1.3)       # rocks standing in the surf
    put_rock(15.0, -2.5, 0.9)
    put_rock(38.0, -1.0, 1.1)
    for _ in range(45):             # pebbles
        put_rock(rng.uniform(-55.0, 55.0), rng.uniform(0.5, 28.0), rng.uniform(0.07, 0.2))
    for _ in range(40):             # scallop shells, concentrated near the waterline
        put_shell(rng.uniform(-45.0, 45.0), rng.uniform(1.5, 8.0) if rng.random() < 0.6 else rng.uniform(8.0, 26.0),
                  rng.uniform(0.35, 0.7), scallop)
    for _ in range(20):
        put_shell(rng.uniform(-45.0, 45.0), rng.uniform(1.5, 20.0), rng.uniform(0.5, 0.9), cone)
    put_driftwood(rng, -8.0, 13.0, 25.0, 6.0, 0.22)
    put_driftwood(rng, 17.0, 18.0, -40.0, 4.5, 0.18)
    put_driftwood(rng, -35.0, 22.0, 70.0, 7.5, 0.25)
    glEndList()
    return out


def draw_beach():
    glEnable(GL_LIGHTING)
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (0.0, 0.0, 0.0, 1.0))
    glCallList(BEACH_LIST)
    glCallList(PROPS_LIST)


def draw_islands():
    glEnable(GL_LIGHTING)
    glCallList(ISLAND_LIST)


# ----------------------------------------------------------------------------
# Ocean: animated grid drawn with client vertex arrays (re-uploaded every frame)
# ----------------------------------------------------------------------------
def ocean_base_color(X, Z):
    s = shore_z(X) - Z
    t1 = smoothstep(0.0, 28.0, s)[..., None]
    t2 = smoothstep(28.0, 260.0, s)[..., None]
    c = SHALLOW * (1.0 - t1) + MID * t1
    c = c * (1.0 - t2) + DEEP * t2
    n = fbm(X * 0.035, Z * 0.035, 3, 7.0)
    near = np.exp(-np.maximum(s, 0.0) / 140.0)
    c = c * (1.0 + ((n - 0.5) * 0.35 * near)[..., None])
    return np.clip(c, 0.0, 1.0)


def build_ocean():
    nx, nz = 141, 220
    t = np.linspace(-1.0, 1.0, nx)
    xs = OCEAN_HALF_WIDTH * (t + 3.0 * t ** 3) / 4.0                 # dense in the middle
    u = np.linspace(0.0, 1.0, nz)
    zs = (OCEAN_Z_NEAR - (OCEAN_Z_NEAR - OCEAN_Z_FAR) * (0.12 * u + 0.88 * u ** 3))[::-1]
    X, Z = np.meshgrid(xs, zs)
    o = OCEAN
    o["nx"], o["nz"], o["X"], o["Z"] = nx, nz, X, Z
    o["S"] = shore_z(X) - Z
    o["setup"] = wave_setup(X, Z)
    o["base"] = ocean_base_color(X, Z)
    o["seabed"] = beach_height(X, Z)
    o["verts"] = np.zeros((nz * nx, 3), np.float32)
    o["norms"] = np.zeros((nz * nx, 3), np.float32)
    o["cols"] = np.zeros((nz * nx, 4), np.float32)
    o["V3"] = o["verts"].reshape(nz, nx, 3)
    o["N3"] = o["norms"].reshape(nz, nx, 3)
    o["C4"] = o["cols"].reshape(nz, nx, 4)
    o["V3"][..., 0] = X
    o["V3"][..., 2] = Z
    o["idx"] = grid_indices(nz, nx)
    o["j0"] = int(np.searchsorted(zs, -380.0))       # rows nearer than this are animated
    o["setup_near"] = slice_setup(o["setup"], o["j0"])
    update_ocean(0.0, S.eye, 0)                       # fill every row once (far rows stay static)


def update_ocean(t, eye, j0):
    """Recompute heights, normals and colours for rows j0.. of the ocean grid."""
    o = OCEAN
    setup = o["setup"] if j0 == 0 else o["setup_near"]
    s = o["S"][j0:]
    X = o["X"][j0:]
    H = wave_eval(setup, t)
    V = o["V3"][j0:]
    V[:, :, 1] = H

    Pi = np.gradient(V, axis=1)
    Pj = np.gradient(V, axis=0)
    N = np.cross(Pj, Pi)
    N /= np.linalg.norm(N, axis=2, keepdims=True) + 1e-12
    o["N3"][j0:] = N

    # Fresnel-like sky reflection: stronger at grazing view angles
    E = np.asarray(eye, dtype=np.float32) - V
    E /= np.linalg.norm(E, axis=2, keepdims=True) + 1e-9
    cosv = np.clip((N * E).sum(axis=2), 0.0, 1.0)
    fres = 0.04 + 0.96 * (1.0 - cosv) ** 5
    k = (0.78 * fres)[..., None]
    col = o["base"][j0:] * (1.0 - k) + SKY_REFLECT * k

    # Transparency depends on water depth (sand shows through the shallows)
    depth = np.maximum(H - o["seabed"][j0:], 0.0)
    alpha = 0.22 + 0.76 * (1.0 - np.exp(-depth / 1.4))
    alpha = np.maximum(alpha, smoothstep(30.0, 160.0, s))
    alpha = np.maximum(alpha, k[..., 0])

    # Foam on wave crests: strongest in the breaker zone, plus selected whitecaps further out
    env = setup[0]
    hn = np.clip(H / (0.5 * env + 1e-6), -1.5, 1.5)
    crest = smoothstep(0.50, 0.95, hn)
    breaker = np.exp(-((s - 5.5) / 8.0) ** 2) * (s > -3.0)
    patch = smoothstep(0.55, 0.85, value_noise(X * 0.045, o["Z"][j0:] * 0.012, 3.0))
    white = 0.9 * breaker + 0.55 * patch * np.exp(-np.maximum(s, 0.0) / 160.0)
    lace = 0.55 + 0.9 * value_noise(X * 0.7 + t * 0.2, o["Z"][j0:] * 0.7 - t * 0.5, 1.0)
    foam = np.clip(crest * white * lace, 0.0, 1.0)
    col = col * (1.0 - foam[..., None]) + FOAM_WHITE * foam[..., None]
    alpha = np.maximum(alpha, foam * 0.95)

    C = o["C4"][j0:]
    C[:, :, :3] = col
    C[:, :, 3] = alpha


def draw_ocean():
    o = OCEAN
    glEnable(GL_LIGHTING)
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (0.75, 0.82, 0.90, 1.0))
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 70.0)
    glEnableClientState(GL_VERTEX_ARRAY)
    glEnableClientState(GL_NORMAL_ARRAY)
    glEnableClientState(GL_COLOR_ARRAY)
    glVertexPointer(3, GL_FLOAT, 0, o["verts"])
    glNormalPointer(GL_FLOAT, 0, o["norms"])
    glColorPointer(4, GL_FLOAT, 0, o["cols"])
    glDrawElements(GL_TRIANGLES, int(o["idx"].size), GL_UNSIGNED_INT, o["idx"])
    glDisableClientState(GL_COLOR_ARRAY)
    glDisableClientState(GL_NORMAL_ARRAY)
    glDisableClientState(GL_VERTEX_ARRAY)
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (0.0, 0.0, 0.0, 1.0))
    glDisable(GL_BLEND)


# ----------------------------------------------------------------------------
# Shoreline foam and wet-sand sheen (follows the moving swash line)
# ----------------------------------------------------------------------------
# rows measured inland (+) / seaward (-) from the instantaneous waterline
FOAM_OFFS = np.array([5.0, 3.2, 1.8, 0.7, 0.0, -0.45, -1.2, -2.3, -3.8])
FOAM_ALPHA = np.array([0.0, 0.05, 0.12, 0.20, 0.70, 0.80, 0.55, 0.28, 0.0])
FOAM_COLS = np.array([(0.80, 0.90, 0.97), (0.80, 0.90, 0.97), (0.82, 0.91, 0.97),
                      (0.88, 0.95, 0.99), (0.98, 1.0, 1.0), (0.98, 1.0, 1.0),
                      (0.97, 0.99, 1.0), (0.96, 0.99, 1.0), (0.95, 0.98, 1.0)])


def build_foam_mesh():
    xs = np.linspace(-260.0, 260.0, 521)
    rows, cols = FOAM_OFFS.size, xs.size
    FOAM["xs"] = xs
    FOAM["rows"], FOAM["cols_n"] = rows, cols
    FOAM["verts"] = np.zeros((rows * cols, 3), np.float32)
    FOAM["cols"] = np.zeros((rows * cols, 4), np.float32)
    FOAM["idx"] = grid_indices(rows, cols)
    FOAM["edge_fade"] = 1.0 - smoothstep(170.0, 255.0, np.abs(xs))
    update_foam(0.0)


def update_foam(t):
    xs = FOAM["xs"]
    rows, cols = FOAM["rows"], FOAM["cols_n"]
    d_sw = swash_edge(xs, t)
    D = d_sw[None, :] + FOAM_OFFS[:, None]
    X = np.broadcast_to(xs, D.shape)
    Z = shore_z(X) + D
    Y = np.maximum(beach_height(X, Z), wave_height(X, Z, t)) + 0.04
    V = FOAM["verts"].reshape(rows, cols, 3)
    V[..., 0], V[..., 1], V[..., 2] = X, Y, Z
    lace = np.clip(0.30 + 1.05 * value_noise(X * 0.42 + t * 0.12, D * 1.15 - t * 0.35, 9.0), 0.0, 1.0)
    pulse = 0.85 + 0.15 * np.sin(0.9 * t + X * 0.03)
    C = FOAM["cols"].reshape(rows, cols, 4)
    C[..., :3] = FOAM_COLS[:, None, :]
    C[..., 3] = FOAM_ALPHA[:, None] * lace * pulse * FOAM["edge_fade"][None, :]


def draw_foam():
    """Translucent foam lace + wet sheen at the waterline (no depth writes)."""
    f = FOAM
    glDisable(GL_LIGHTING)
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    glDepthMask(GL_FALSE)
    glEnable(GL_POLYGON_OFFSET_FILL)
    glPolygonOffset(-2.0, -2.0)
    glEnableClientState(GL_VERTEX_ARRAY)
    glEnableClientState(GL_COLOR_ARRAY)
    glVertexPointer(3, GL_FLOAT, 0, f["verts"])
    glColorPointer(4, GL_FLOAT, 0, f["cols"])
    glDrawElements(GL_TRIANGLES, int(f["idx"].size), GL_UNSIGNED_INT, f["idx"])
    glDisableClientState(GL_COLOR_ARRAY)
    glDisableClientState(GL_VERTEX_ARRAY)
    glDisable(GL_POLYGON_OFFSET_FILL)
    glDepthMask(GL_TRUE)
    glDisable(GL_BLEND)


# ----------------------------------------------------------------------------
# Sun glitter on the water: twinkling points + a soft shimmering reflection path
# ----------------------------------------------------------------------------
def build_glitter():
    rng = np.random.default_rng(5)
    n = 520
    D = 25.0 + 1500.0 * rng.random(n) ** 2.2
    GLITTER["D"] = D
    GLITTER["lat"] = rng.normal(0.0, 1.0, n) * (2.0 + 0.05 * D)
    GLITTER["phase"] = rng.uniform(0.0, 2.0 * math.pi, n)
    GLITTER["freq"] = rng.uniform(1.5, 4.5, n)


def draw_sun_glitter(t):
    sh = normalize((SUN_DIR[0], SUN_DIR[2]))
    perp = np.array((-sh[1], sh[0]))
    ex, ez = float(S.eye[0]), float(S.eye[2])

    glDisable(GL_LIGHTING)
    glDisable(GL_FOG)
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE)
    glDepthMask(GL_FALSE)

    # soft reflection path (quad strips fading to the sides)
    n = 36
    Ds = 70.0 * (30.0 ** (np.arange(n) / (n - 1.0)))
    cx, cz = ex + sh[0] * Ds, ez + sh[1] * Ds
    cy = wave_height(cx, cz, t) + 0.06
    w = 2.0 + 0.055 * Ds
    over_water = smoothstep(2.0, 10.0, shore_z(cx) - cz)
    flick = 0.78 + 0.22 * np.sin(1.9 * t + np.arange(n) * 2.3)
    a = 0.30 * (0.35 + 0.65 * np.exp(-Ds / 900.0)) * flick * over_water
    for side in (-1.0, 1.0):
        glBegin(GL_QUAD_STRIP)
        for i in range(n):
            glColor4f(1.0, 0.88, 0.60, 0.0)
            glVertex3f(float(cx[i] + side * perp[0] * w[i]), float(cy[i]), float(cz[i] + side * perp[1] * w[i]))
            glColor4f(1.0, 0.88, 0.60, float(a[i]))
            glVertex3f(float(cx[i]), float(cy[i]), float(cz[i]))
        glEnd()

    # twinkling sparkles
    D, lat = GLITTER["D"], GLITTER["lat"]
    px = ex + sh[0] * D + perp[0] * lat
    pz = ez + sh[1] * D + perp[1] * lat
    py = wave_height(px, pz, t) + 0.05
    tw = np.clip(np.sin(t * GLITTER["freq"] + GLITTER["phase"]), 0.0, 1.0) ** 6
    alpha = tw * (0.40 + 0.60 * np.exp(-D / 1200.0))
    alpha = alpha * smoothstep(3.0, 12.0, shore_z(px) - pz)
    keep = alpha > 0.02
    glEnableClientState(GL_VERTEX_ARRAY)
    glEnableClientState(GL_COLOR_ARRAY)
    for near, size in ((True, 3.4), (False, 2.0)):
        m = keep & ((D < 180.0) if near else (D >= 180.0))
        cnt = int(m.sum())
        if cnt == 0:
            continue
        pts = np.ascontiguousarray(np.column_stack((px[m], py[m], pz[m])), dtype=np.float32)
        cols = np.ascontiguousarray(np.column_stack((np.ones(cnt), np.full(cnt, 0.96),
                                                     np.full(cnt, 0.82), alpha[m])), dtype=np.float32)
        glPointSize(size)
        glVertexPointer(3, GL_FLOAT, 0, pts)
        glColorPointer(4, GL_FLOAT, 0, cols)
        glDrawArrays(GL_POINTS, 0, cnt)
    glDisableClientState(GL_COLOR_ARRAY)
    glDisableClientState(GL_VERTEX_ARRAY)
    glDepthMask(GL_TRUE)
    glDisable(GL_BLEND)
    glEnable(GL_FOG)


# ----------------------------------------------------------------------------
# Clouds: clusters of flattened, shaded spheres
# ----------------------------------------------------------------------------
def make_cloud_puffs(rng, width, kind):
    height = width * (0.20 if kind == "cumulus" else 0.07)
    n = rng.randint(18, 30) if kind == "cumulus" else rng.randint(12, 20)
    puffs = []
    for _ in range(max(4, n // 4)):                      # flat-ish base layer
        u = rng.uniform(-0.85, 0.85)
        puffs.append((u * width * 0.5, rng.uniform(0.0, 0.08) * height, rng.uniform(-0.5, 0.5) * width * 0.18,
                      width * rng.uniform(0.07, 0.11) * (1.0 - 0.4 * abs(u))))
    for _ in range(n):                                   # billowing top
        u = rng.uniform(-1.0, 1.0)
        lift = (1.0 - u * u) ** 0.9
        v = rng.random() ** 0.7
        r = width * rng.uniform(0.06, 0.12) * (0.55 + 0.6 * lift) * (1.0 if kind == "cumulus" else 0.8)
        puffs.append((u * width * 0.5, v * height * lift, rng.uniform(-1.0, 1.0) * width * 0.2, r))
    return puffs, height


def build_clouds():
    rng = random.Random(3)
    specs = []
    for _ in range(20):                                  # visible band near the horizon
        specs.append((rng.uniform(350.0, 1500.0), rng.uniform(-85.0, 85.0), rng.uniform(3.0, 16.0)))
    for _ in range(8):                                   # high clouds, visible when tilting up / turning
        specs.append((rng.uniform(200.0, 700.0), rng.uniform(-180.0, 180.0), rng.uniform(25.0, 55.0)))
    clouds = []
    for dist, bearing, elev in specs:
        kind = "cumulus" if rng.random() < 0.7 else "stratus"
        width = dist * rng.uniform(0.13, 0.25) * (1.0 if kind == "cumulus" else 1.4)
        puffs, height = make_cloud_puffs(rng, width, kind)
        lst = glGenLists(1)
        glNewList(lst, GL_COMPILE)
        for (x, y, z, r) in puffs:
            shade = min(1.0, 0.80 + 0.20 * (y / (height + 1e-6)))
            glColor3f(shade * 0.98, shade * 0.99, shade)
            glPushMatrix()
            glTranslatef(x, y, z)
            glScalef(r, r * 0.6, r)
            glCallList(SPHERE_LIST)
            glPopMatrix()
        glEndList()
        b, e = math.radians(bearing), math.radians(elev)
        clouds.append({"list": lst,
                       "x0": dist * math.sin(b), "y": dist * math.tan(e), "z": -dist * math.cos(b),
                       "speed": rng.uniform(1.5, 4.0)})
    return clouds


def draw_clouds(t):
    glEnable(GL_LIGHTING)
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, (0.26, 0.28, 0.32, 1.0))   # soft self-illumination
    for c in CLOUDS:
        x = ((c["x0"] + c["speed"] * t + 1900.0) % 3800.0) - 1900.0
        glPushMatrix()
        glTranslatef(x, c["y"], c["z"])
        glCallList(c["list"])
        glPopMatrix()
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, (0.0, 0.0, 0.0, 1.0))


# ----------------------------------------------------------------------------
# Seabirds: simple 3-D gulls with flapping, gliding and banking
# ----------------------------------------------------------------------------
def make_birds():
    rng = random.Random(7)
    birds = []

    def add(cx, cz, cy, rx, rz, speed, scale, cy_amp):
        birds.append({
            "cx": cx, "cz": cz, "cy": cy, "rx": rx, "rz": rz, "scale": scale, "cy_amp": cy_amp,
            "omega": speed / (0.5 * (rx + rz)), "dir": rng.choice((-1.0, 1.0)),
            "theta0": rng.uniform(0.0, 2.0 * math.pi),
            "p1": rng.uniform(0.0, 6.28), "p2": rng.uniform(0.0, 6.28), "p3": rng.uniform(0.0, 6.28),
            "rate": rng.uniform(8.0, 11.0) / scale ** 0.3, "phase": rng.uniform(0.0, 6.28),
            "pos": (0.0, 0.0, 0.0), "yaw": 0.0, "pitch": 0.0, "bank": 0.0, "fin": 0.0, "fout": 0.0})

    for _ in range(4):      # foreground: large, detailed
        add(rng.uniform(-30, 30), rng.uniform(-25, 5), rng.uniform(9, 17), rng.uniform(16, 32),
            rng.uniform(9, 16), rng.uniform(6, 9), rng.uniform(1.2, 1.7), rng.uniform(0.8, 1.8))
    for _ in range(4):      # middle distance
        add(rng.uniform(-90, 90), rng.uniform(-120, -50), rng.uniform(18, 34), rng.uniform(40, 70),
            rng.uniform(20, 40), rng.uniform(10, 14), rng.uniform(2.2, 3.2), rng.uniform(1.5, 3.0))
    for _ in range(5):      # far away: small silhouettes
        add(rng.uniform(-250, 250), rng.uniform(-420, -200), rng.uniform(35, 75), rng.uniform(90, 160),
            rng.uniform(40, 90), rng.uniform(16, 22), rng.uniform(4.5, 7.5), rng.uniform(2.0, 5.0))
    return birds


def bird_position(b, t):
    th = b["theta0"] + b["dir"] * b["omega"] * t
    wob = 1.0 + 0.10 * math.sin(0.31 * t + b["p1"])
    return (b["cx"] + b["rx"] * wob * math.cos(th),
            b["cy"] + b["cy_amp"] * math.sin(0.53 * t + b["p2"]),
            b["cz"] + b["rz"] * wob * math.sin(th))


def update_birds(t, dt):
    for b in BIRDS:
        # glide / flap cycle: g in [0, 1] (0 = pure glide)
        g = max(0.0, min(1.0, 0.55 + 0.8 * math.sin(0.42 * t + b["p3"])))
        b["phase"] += dt * b["rate"] * (0.30 + 0.70 * g)
        ph, amp = b["phase"], 8.0 + 34.0 * g
        b["fin"] = 5.0 + 0.45 * amp * (math.sin(ph) + 0.2 * math.sin(2.0 * ph + 0.8))
        b["fout"] = -4.0 + 0.85 * amp * math.sin(ph - 0.9)

        p0, p1, p2 = bird_position(b, t), bird_position(b, t + 0.05), bird_position(b, t + 0.10)
        v0 = (p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2])
        v1 = (p2[0] - p1[0], p2[2] - p1[2])
        hs = math.hypot(v0[0], v0[2]) / 0.05
        yaw0 = math.atan2(-v0[2], v0[0])
        yaw1 = math.atan2(-v1[1], v1[0])
        dyaw = (yaw1 - yaw0 + math.pi) % (2.0 * math.pi) - math.pi
        b["pos"] = p0
        b["yaw"] = math.degrees(yaw0)
        b["pitch"] = math.degrees(math.atan2(v0[1] / 0.05, hs + 1e-6))
        b["bank"] = max(-40.0, min(40.0, math.degrees(math.atan(hs * (dyaw / 0.05) / GRAVITY))))


def draw_wing(side, inner_a, outer_a, flat):
    """One wing: inner panel + outer panel with a dark tip. side = +1 / -1."""
    glPushMatrix()
    glRotatef(-side * inner_a, 1.0, 0.0, 0.0)
    if not flat:
        glColor3f(0.84, 0.86, 0.90)
    glBegin(GL_QUADS)
    glNormal3f(0.0, 1.0, 0.0)
    for x, z in ((-0.28, 0.0), (-0.22, 0.55), (0.22, 0.55), (0.25, 0.0)):
        glVertex3f(x, 0.0, side * z)
    glEnd()
    glTranslatef(0.0, 0.0, side * 0.55)
    glRotatef(-side * outer_a, 1.0, 0.0, 0.0)
    if not flat:
        glColor3f(0.94, 0.95, 0.96)
    glBegin(GL_POLYGON)
    glNormal3f(0.0, 1.0, 0.0)
    for x, z in ((-0.22, 0.0), (-0.38, 0.62), (0.05, 0.42), (0.22, 0.0)):
        glVertex3f(x, 0.0, side * z)
    glEnd()
    if not flat:
        glColor3f(0.10, 0.10, 0.12)                     # black wing tips
        glBegin(GL_TRIANGLES)
        glNormal3f(0.0, 1.0, 0.0)
        for x, z in ((-0.323, 0.40), (-0.38, 0.62), (-0.122, 0.50)):
            glVertex3f(x, 0.004, side * z)
        glEnd()
    glPopMatrix()


def draw_bird_detailed(b):
    glColor3f(0.95, 0.96, 0.97)
    glPushMatrix()                                       # body
    glScalef(0.52, 0.17, 0.17)
    glCallList(SPHERE_LOW_LIST)
    glPopMatrix()
    glPushMatrix()                                       # head
    glTranslatef(0.50, 0.06, 0.0)
    glScalef(0.13, 0.12, 0.12)
    glCallList(SPHERE_LOW_LIST)
    glPopMatrix()
    glColor3f(1.0, 0.75, 0.20)                           # beak
    glPushMatrix()
    glTranslatef(0.60, 0.04, 0.0)
    glRotatef(90.0, 0.0, 1.0, 0.0)
    gluCylinder(QUAD, 0.04, 0.0, 0.20, 8, 1)
    glPopMatrix()
    glColor3f(0.93, 0.94, 0.95)                          # tail
    glBegin(GL_QUADS)
    glNormal3f(0.0, 1.0, 0.0)
    for x, z in ((-0.45, -0.11), (-0.85, -0.14), (-0.85, 0.14), (-0.45, 0.11)):
        glVertex3f(x, 0.02, z)
    glEnd()
    draw_wing(1.0, b["fin"], b["fout"], False)
    draw_wing(-1.0, b["fin"], b["fout"], False)


def draw_bird_distant(b):
    glDisable(GL_LIGHTING)
    glColor3f(0.20, 0.22, 0.27)
    glBegin(GL_TRIANGLES)
    glVertex3f(0.55, 0.0, 0.0)
    glVertex3f(-0.7, 0.0, 0.10)
    glVertex3f(-0.7, 0.0, -0.10)
    glEnd()
    draw_wing(1.0, b["fin"], b["fout"], True)
    draw_wing(-1.0, b["fin"], b["fout"], True)
    glEnable(GL_LIGHTING)


def draw_birds():
    glEnable(GL_LIGHTING)
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (0.0, 0.0, 0.0, 1.0))
    for b in BIRDS:
        x, y, z = b["pos"]
        dist = math.sqrt((x - S.eye[0]) ** 2 + (y - S.eye[1]) ** 2 + (z - S.eye[2]) ** 2)
        glPushMatrix()
        glTranslatef(x, y, z)
        glRotatef(b["yaw"], 0.0, 1.0, 0.0)
        glRotatef(b["pitch"], 0.0, 0.0, 1.0)
        glRotatef(-b["bank"], 1.0, 0.0, 0.0)
        glScalef(b["scale"], b["scale"], b["scale"])
        if dist < 150.0:
            draw_bird_detailed(b)
        else:
            draw_bird_distant(b)
        glPopMatrix()


# ----------------------------------------------------------------------------
# Animation / camera
# ----------------------------------------------------------------------------
def update_camera(dt):
    k = 1.0 - math.exp(-7.0 * dt)
    for key in S.cam:
        S.cam[key] += (S.goal[key] - S.cam[key]) * k
    yaw, pit, dist = math.radians(S.cam["yaw"]), math.radians(S.cam["pitch"]), S.cam["dist"]
    ex = CAM_TARGET[0] + dist * math.sin(yaw) * math.cos(pit)
    ey = CAM_TARGET[1] + dist * math.sin(pit)
    ez = CAM_TARGET[2] + dist * math.cos(yaw) * math.cos(pit)
    ground = max(float(beach_height(ex, ez)), 0.0)
    S.eye[:] = (ex, max(ey, ground + 1.6), ez)          # never dive into the sand


def update_animation():
    now = time.perf_counter()
    if S.last is None:
        S.last = now
    dt = min(now - S.last, 0.1)
    S.last = now
    if not S.paused:
        S.t += dt
    update_camera(dt)
    update_birds(S.t, 0.0 if S.paused else dt)
    update_ocean(S.t, S.eye, OCEAN["j0"])
    update_foam(S.t)

    S.frames += 1
    S.fps_clock += dt
    if S.fps_clock >= 0.5:
        glutSetWindowTitle(("%s  -  %.0f FPS" % (WINDOW_TITLE, S.frames / S.fps_clock)).encode("ascii"))
        S.frames, S.fps_clock = 0, 0.0


def clamp_goal():
    for key, (lo, hi) in CAM_LIMITS.items():
        S.goal[key] = max(lo, min(hi, S.goal[key]))


# ----------------------------------------------------------------------------
# GLUT callbacks
# ----------------------------------------------------------------------------
def init_opengl():
    global QUAD, SKY_LIST, BEACH_LIST, ISLAND_LIST, PROPS_LIST, CLOUDS, BIRDS
    glClearColor(HAZE[0], HAZE[1], HAZE[2], 1.0)
    glEnable(GL_DEPTH_TEST)
    glDepthFunc(GL_LEQUAL)
    glShadeModel(GL_SMOOTH)
    glEnable(GL_NORMALIZE)
    try:
        glEnable(GL_MULTISAMPLE)
    except Exception:
        pass
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST)
    glEnable(GL_POINT_SMOOTH)
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST)

    # Lighting: warm low sun + cool sky ambient
    glEnable(GL_LIGHTING)
    glEnable(GL_LIGHT0)
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, (0.50, 0.56, 0.64, 1.0))
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE)
    try:
        glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR)
    except Exception:
        pass
    glLightfv(GL_LIGHT0, GL_AMBIENT, (0.0, 0.0, 0.0, 1.0))
    glLightfv(GL_LIGHT0, GL_DIFFUSE, (1.45, 1.22, 0.92, 1.0))
    glLightfv(GL_LIGHT0, GL_SPECULAR, (1.0, 0.92, 0.78, 1.0))
    glEnable(GL_COLOR_MATERIAL)
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE)
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (0.0, 0.0, 0.0, 1.0))
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, (0.0, 0.0, 0.0, 1.0))

    # Atmospheric haze
    glEnable(GL_FOG)
    glFogi(GL_FOG_MODE, GL_LINEAR)
    glFogfv(GL_FOG_COLOR, (HAZE[0], HAZE[1], HAZE[2], 1.0))
    glFogf(GL_FOG_START, FOG_START)
    glFogf(GL_FOG_END, FOG_END)
    glHint(GL_FOG_HINT, GL_NICEST)

    # Build everything that is static (once)
    QUAD = gluNewQuadric()
    gluQuadricNormals(QUAD, GLU_SMOOTH)
    build_sphere_lists()
    SKY_LIST = build_sky_list()
    BEACH_LIST = build_beach_list()
    ISLAND_LIST = build_island_list()
    PROPS_LIST = build_props_list()
    CLOUDS = build_clouds()
    BIRDS = make_birds()
    build_glitter()
    update_camera(0.0)
    build_ocean()
    build_foam_mesh()


def display():
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()
    gluLookAt(float(S.eye[0]), float(S.eye[1]), float(S.eye[2]),
              float(CAM_TARGET[0]), float(CAM_TARGET[1]), float(CAM_TARGET[2]),
              0.0, 1.0, 0.0)
    glLightfv(GL_LIGHT0, GL_POSITION, (float(SUN_DIR[0]), float(SUN_DIR[1]), float(SUN_DIR[2]), 0.0))

    glEnable(GL_FOG)
    draw_sky()                      # background
    draw_sun(S.t)
    glEnable(GL_FOG)
    draw_islands()                  # opaque scenery, far to near
    draw_beach()
    draw_clouds(S.t)
    draw_birds()
    draw_ocean()                    # translucent layers last
    draw_foam()
    draw_sun_glitter(S.t)
    glutSwapBuffers()


def reshape(w, h):
    h = max(h, 1)
    S.width, S.height = w, h
    glViewport(0, 0, w, h)
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(FOV_Y, w / float(h), Z_NEAR, Z_FAR)
    glMatrixMode(GL_MODELVIEW)


def quit_app():
    try:
        glutLeaveMainLoop()          # freeglut: returns from glutMainLoop() cleanly
    except Exception:
        sys.exit(0)


def keyboard(key, x, y):
    k = key.decode("latin-1").lower() if isinstance(key, bytes) else str(key).lower()
    if k in ("\x1b", "q"):
        quit_app()
        return
    if k in ("a",):
        S.goal["yaw"] -= 4.0
    elif k in ("d",):
        S.goal["yaw"] += 4.0
    elif k in ("w", "+", "="):
        S.goal["dist"] *= 0.93
    elif k in ("s", "-", "_"):
        S.goal["dist"] /= 0.93
    elif k == "r":
        S.goal.update(CAM_DEFAULT)
    elif k == "p":
        S.paused = not S.paused
    elif k == "f":
        S.fullscreen = not S.fullscreen
        if S.fullscreen:
            glutFullScreen()
        else:
            glutReshapeWindow(WIN_W, WIN_H)
            glutPositionWindow(80, 60)
    clamp_goal()


def special(key, x, y):
    if key == GLUT_KEY_LEFT:
        S.goal["yaw"] -= 4.0
    elif key == GLUT_KEY_RIGHT:
        S.goal["yaw"] += 4.0
    elif key == GLUT_KEY_UP:
        S.goal["pitch"] += 2.5
    elif key == GLUT_KEY_DOWN:
        S.goal["pitch"] -= 2.5
    elif key == GLUT_KEY_PAGE_UP:
        S.goal["dist"] *= 0.93
    elif key == GLUT_KEY_PAGE_DOWN:
        S.goal["dist"] /= 0.93
    clamp_goal()


def mouse(button, state, x, y):
    if button == GLUT_LEFT_BUTTON:
        S.drag = (x, y) if state == GLUT_DOWN else None
    elif state == GLUT_DOWN and button == 3:       # wheel up
        S.goal["dist"] *= 0.92
    elif state == GLUT_DOWN and button == 4:       # wheel down
        S.goal["dist"] /= 0.92
    clamp_goal()


def motion(x, y):
    if S.drag is None:
        return
    dx, dy = x - S.drag[0], y - S.drag[1]
    S.drag = (x, y)
    S.goal["yaw"] -= dx * 0.20
    S.goal["pitch"] += dy * 0.15
    clamp_goal()


def timer(_value):
    update_animation()
    glutPostRedisplay()
    glutTimerFunc(FRAME_MS, timer, 0)


def main():
    glutInit()
    mode = GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH
    if "--no-msaa" not in sys.argv:
        mode |= GLUT_MULTISAMPLE
    glutInitDisplayMode(mode)
    glutInitWindowSize(WIN_W, WIN_H)
    glutInitWindowPosition(80, 60)
    glutCreateWindow(WINDOW_TITLE.encode("ascii"))
    try:                                   # closing the window ends glutMainLoop() instead of exit()
        glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS)
    except Exception:
        pass

    init_opengl()

    glutDisplayFunc(display)
    glutReshapeFunc(reshape)
    glutKeyboardFunc(keyboard)
    glutSpecialFunc(special)
    glutMouseFunc(mouse)
    glutMotionFunc(motion)
    glutTimerFunc(FRAME_MS, timer, 0)
    glutMainLoop()
    print("Goodbye!")


if __name__ == "__main__":
    main()
