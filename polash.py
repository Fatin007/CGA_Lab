import math
import sys

from OpenGL.GL import *
from OpenGL.GLU import *
from OpenGL.GLUT import *

WINDOW_W, WINDOW_H = 1200, 800
WINDOW_TITLE = b"Jagannath University"

SHOW_TREES = False
BLOCK_HALF_W = 9.0         # central block half width  (x = -9 .. 9)
WING_X0 = BLOCK_HALF_W     # wings start where the block ends
WING_X1 = 30.0             # wing outer end
WING_DEPTH = 12.0          # building depth behind the wing façade (z = 0 .. -12)
WALL_T = 0.8               # thickness of the façade wall layer around openings
PLINTH_H = 0.8             # stone base / entrance platform height
FLOOR1_TOP = 6.5           # top of ground floor
FLOOR2_BOT = 7.3           # bottom of upper-floor wall (above floor band)
WING_WALL_TOP = 13.0       # underside of wing cornice
WING_ROOF = 14.0           # top of wing cornice / roof
ARCH_SEGMENTS = 14         # polygons per semicircular arch

WALL = (0.93, 0.87, 0.68)          # pale cream / yellowish plaster
WALL_SHADE = (0.86, 0.80, 0.62)    # hidden core walls
IVORY = (0.97, 0.94, 0.84)         # columns, frames, cornices
CREAM_DARK = (0.82, 0.74, 0.54)    # subtle darker accents
STONE = (0.80, 0.76, 0.66)         # plinth, platform, steps
LOGGIA_WALL = (0.82, 0.70, 0.47)   # warmer, deeper tone of the recessed entrance
OPENING_DARK = (0.16, 0.11, 0.08)  # window interiors
GRILLE = (0.40, 0.30, 0.22)
SIGN_RED = (0.86, 0.31, 0.13)      # reddish-orange signboard
SIGN_TEXT = (0.98, 0.92, 0.80)
GOLD = (0.80, 0.64, 0.24)
EMBLEM_BLUE = (0.22, 0.48, 0.58)
GROUND_COURT = (0.74, 0.70, 0.62)
GROUND_LAWN = (0.45, 0.54, 0.34)
TRUNK = (0.35, 0.24, 0.15)
LEAF_COLORS = [(0.22, 0.45, 0.20), (0.30, 0.55, 0.25), (0.18, 0.38, 0.17)]
SKY = (0.66, 0.80, 0.93)

CAM_DEFAULT = {"dist": 72.0, "az": 24.0, "el": 12.0}
cam_dist = CAM_DEFAULT["dist"]
cam_az = CAM_DEFAULT["az"]        # degrees, 0 = looking straight at the façade
cam_el = CAM_DEFAULT["el"]        # degrees above the horizon
cam_target = [0.0, 9.0, -2.0]     # orbit centre (x, y, z)
mouse_dragging = False
last_mouse_x = 0
last_mouse_y = 0
win_w, win_h = WINDOW_W, WINDOW_H
show_help = True
scene_list = None



def set_color(c):
    glColor3f(c[0], c[1], c[2])


def draw_cuboid(x0, y0, z0, x1, y1, z1, color):
    """Axis-aligned box given by its min / max corners, with flat face normals."""
    set_color(color)
    glBegin(GL_QUADS)
    glNormal3f(0, 0, 1)      # front  (+z)
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1)
    glNormal3f(0, 0, -1)     # back
    glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0)
    glNormal3f(1, 0, 0)      # right
    glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1)
    glNormal3f(-1, 0, 0)     # left
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0)
    glNormal3f(0, 1, 0)      # top
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0)
    glNormal3f(0, -1, 0)     # bottom
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1)
    glEnd()


def draw_cylinder(cx, y0, cz, r_bot, r_top, height, slices, color,
                  cap_top=True, cap_bottom=True):
    """Vertical (Y-axis) cylinder / frustum with smooth side normals."""
    set_color(color)
    slope = (r_bot - r_top) / height if height else 0.0
    glBegin(GL_QUAD_STRIP)
    for i in range(slices + 1):
        a = 2.0 * math.pi * i / slices
        c, s = math.cos(a), math.sin(a)
        glNormal3f(c, slope, s)
        glVertex3f(cx + r_top * c, y0 + height, cz + r_top * s)
        glVertex3f(cx + r_bot * c, y0, cz + r_bot * s)
    glEnd()
    if cap_top:
        glNormal3f(0, 1, 0)
        glBegin(GL_TRIANGLE_FAN)
        glVertex3f(cx, y0 + height, cz)
        for i in range(slices + 1):
            a = 2.0 * math.pi * i / slices
            glVertex3f(cx + r_top * math.cos(a), y0 + height, cz + r_top * math.sin(a))
        glEnd()
    if cap_bottom:
        glNormal3f(0, -1, 0)
        glBegin(GL_TRIANGLE_FAN)
        glVertex3f(cx, y0, cz)
        for i in range(slices + 1):
            a = -2.0 * math.pi * i / slices
            glVertex3f(cx + r_bot * math.cos(a), y0, cz + r_bot * math.sin(a))
        glEnd()


def draw_cylinder_z(cx, cy, z0, radius, depth, slices, color):
    glPushMatrix()
    glTranslatef(cx, cy, z0)
    glRotatef(90.0, 1, 0, 0)        # local +Y  ->  world +Z
    draw_cylinder(0, 0, 0, radius, radius, depth, slices, color, True, True)
    glPopMatrix()


def draw_extruded_polygon(points, z0, z1, color):
    set_color(color)
    n = len(points)
    glBegin(GL_QUADS)
    for i in range(n):
        xa, ya = points[i]
        xb, yb = points[(i + 1) % n]
        glNormal3f(yb - ya, -(xb - xa), 0.0)       # outward edge normal
        glVertex3f(xa, ya, z0); glVertex3f(xb, yb, z0)
        glVertex3f(xb, yb, z1); glVertex3f(xa, ya, z1)
    glEnd()
    glNormal3f(0, 0, 1)
    glBegin(GL_POLYGON)
    for (x, y) in points:
        glVertex3f(x, y, z1)
    glEnd()
    glNormal3f(0, 0, -1)
    glBegin(GL_POLYGON)
    for (x, y) in reversed(points):
        glVertex3f(x, y, z0)
    glEnd()


def draw_triangle_prism(half_w, height, z0, z1, color):
    """Isosceles triangular prism, base on y = 0, apex at (0, height)."""
    draw_extruded_polygon([(-half_w, 0.0), (half_w, 0.0), (0.0, height)], z0, z1, color)

def draw_column(x, y, z, radius, height, color):
    """Classical column: plinth, base disc, tapered shaft, neck, capital, abacus."""
    r = radius
    top = y + height
    draw_cuboid(x - 1.3 * r, y, z - 1.3 * r, x + 1.3 * r, y + 0.35, z + 1.3 * r, color)        # plinth
    draw_cylinder(x, y + 0.35, z, 1.15 * r, 1.08 * r, 0.30, 20, CREAM_DARK)                       # base ring
    draw_cylinder(x, y + 0.65, z, r, 0.9 * r, height - 1.95, 24, color, False, False)             # shaft
    draw_cylinder(x, top - 1.30, z, 1.05 * r, 1.05 * r, 0.15, 20, CREAM_DARK)                     # neck band
    draw_cylinder(x, top - 1.15, z, 0.95 * r, 1.40 * r, 0.75, 24, color)                          # capital
    draw_cuboid(x - 1.6 * r, top - 0.40, z - 1.6 * r, x + 1.6 * r, top, z + 1.6 * r, color)       # abacus


def draw_arch(cx, cy, z, r_in, r_out, depth, color, segs=ARCH_SEGMENTS):
    """Semicircular arch ring (half annulus) centred on (cx, cy), projecting
    `depth` in front of plane z.  Used for window surrounds and decorative arches."""
    set_color(color)
    zf = z + depth
    glBegin(GL_QUADS)
    for i in range(segs):
        a0 = math.pi * i / segs
        a1 = math.pi * (i + 1) / segs
        c0, s0, c1, s1 = math.cos(a0), math.sin(a0), math.cos(a1), math.sin(a1)
        am = 0.5 * (a0 + a1)
        cm, sm = math.cos(am), math.sin(am)
        glNormal3f(0, 0, 1)                                    # front face
        glVertex3f(cx + r_in * c0, cy + r_in * s0, zf); glVertex3f(cx + r_out * c0, cy + r_out * s0, zf)
        glVertex3f(cx + r_out * c1, cy + r_out * s1, zf); glVertex3f(cx + r_in * c1, cy + r_in * s1, zf)
        glNormal3f(cm, sm, 0)                                  # outer surface
        glVertex3f(cx + r_out * c0, cy + r_out * s0, z); glVertex3f(cx + r_out * c1, cy + r_out * s1, z)
        glVertex3f(cx + r_out * c1, cy + r_out * s1, zf); glVertex3f(cx + r_out * c0, cy + r_out * s0, zf)
        glNormal3f(-cm, -sm, 0)                                # inner surface
        glVertex3f(cx + r_in * c0, cy + r_in * s0, z); glVertex3f(cx + r_in * c1, cy + r_in * s1, z)
        glVertex3f(cx + r_in * c1, cy + r_in * s1, zf); glVertex3f(cx + r_in * c0, cy + r_in * s0, zf)
    glNormal3f(0, -1, 0)                                       # springing faces
    for sgn in (1, -1):
        glVertex3f(cx + sgn * r_in, cy, z); glVertex3f(cx + sgn * r_out, cy, z)
        glVertex3f(cx + sgn * r_out, cy, zf); glVertex3f(cx + sgn * r_in, cy, zf)
    glEnd()


def draw_pilaster(x, y0, y1, z, color=IVORY):
    """Flat wall column with base block and capital block."""
    draw_cuboid(x - 0.32, y0, z, x + 0.32, y0 + 0.30, z + 0.30, color)
    draw_cuboid(x - 0.22, y0 + 0.30, z, x + 0.22, y1 - 0.40, z + 0.22, color)
    draw_cuboid(x - 0.32, y1 - 0.40, z, x + 0.32, y1, z + 0.30, color)

def draw_arched_window(x, y, z, width, height, depth, color,
                       bay_w, wall_bottom, wall_top, sill=True, frame_color=IVORY):
    """A genuinely recessed arched opening in a wall layer.

    x, y      : centre x and bottom y of the opening
    z         : plane of the wall's FRONT face
    width     : opening width (arch radius = width / 2)
    height    : total opening height (rectangle + semicircle)
    depth     : wall thickness = how deep the dark opening is recessed
    bay_w     : width of wall bay around the opening
    wall_bottom / wall_top : vertical extent of the wall bay

    The wall is built from separate pieces (sill wall, two jambs, and an
    arch-shaped spandrel strip) so the opening is a real hole, with reveals
    on the sides, a dark back and a thin 3D grille.
    """
    r = width / 2.0
    ys = y + height - r                  # springing line of the arch
    xl, xr = x - r, x + r
    x0, x1 = x - bay_w / 2.0, x + bay_w / 2.0
    zb = z - depth

    # --- wall pieces around the hole
    if y > wall_bottom:
        draw_cuboid(x0, wall_bottom, zb, x1, y, z, color)
    draw_cuboid(x0, y, zb, xl, wall_top, z, color)
    draw_cuboid(xr, y, zb, x1, wall_top, z, color)

    pts = []
    for i in range(ARCH_SEGMENTS + 1):
        a = math.pi - math.pi * i / ARCH_SEGMENTS
        pts.append((x + r * math.cos(a), ys + r * math.sin(a)))

    set_color(color)
    glBegin(GL_QUADS)
    for i in range(ARCH_SEGMENTS):
        (ax, ay), (bx, by) = pts[i], pts[i + 1]
        glNormal3f(0, 0, 1)                                   # spandrel front face
        glVertex3f(ax, ay, z); glVertex3f(bx, by, z)
        glVertex3f(bx, wall_top, z); glVertex3f(ax, wall_top, z)
        mx, my = 0.5 * (ax + bx) - x, 0.5 * (ay + by) - ys    # intrados (inner arch surface)
        ln = math.hypot(mx, my)
        glNormal3f(-mx / ln, -my / ln, 0)
        glVertex3f(ax, ay, zb); glVertex3f(bx, by, zb)
        glVertex3f(bx, by, z); glVertex3f(ax, ay, z)
    glNormal3f(1, 0, 0)                                       # left reveal
    glVertex3f(xl, y, zb); glVertex3f(xl, y, z); glVertex3f(xl, ys, z); glVertex3f(xl, ys, zb)
    glNormal3f(-1, 0, 0)                                      # right reveal
    glVertex3f(xr, y, zb); glVertex3f(xr, ys, zb); glVertex3f(xr, ys, z); glVertex3f(xr, y, z)
    if sill:
        glNormal3f(0, 1, 0)                                   # bottom reveal
        glVertex3f(xl, y, zb); glVertex3f(xr, y, zb); glVertex3f(xr, y, z); glVertex3f(xl, y, z)
    glEnd()

    # --- dark interior (just in front of the core wall to avoid z-fighting)
    set_color(OPENING_DARK)
    glNormal3f(0, 0, 1)
    glBegin(GL_QUADS)
    glVertex3f(xl, y, zb + 0.02); glVertex3f(xr, y, zb + 0.02)
    glVertex3f(xr, ys, zb + 0.02); glVertex3f(xl, ys, zb + 0.02)
    glEnd()
    glBegin(GL_TRIANGLE_FAN)
    glVertex3f(x, ys, zb + 0.02)
    for (px, py) in pts:
        glVertex3f(px, py, zb + 0.02)
    glEnd()

    # --- thin 3D grille bars inside the opening
    for k in (1, 2, 3):
        bx = xl + width * k / 4.0
        draw_cuboid(bx - 0.04, y, zb + 0.12, bx + 0.04, ys + (r * 0.9 if k == 2 else r * 0.7), zb + 0.20, GRILLE)
    draw_cuboid(xl, y + (ys - y) * 0.55, zb + 0.12, xr, y + (ys - y) * 0.55 + 0.07, zb + 0.20, GRILLE)

    # --- raised outer frame, arch surround, keystone, sill
    ring = 0.35
    draw_arch(x, ys, z, r, r + ring, 0.15, frame_color)
    draw_cuboid(xl - ring, y, z, xl, ys, z + 0.15, frame_color)
    draw_cuboid(xr, y, z, xr + ring, ys, z + 0.15, frame_color)
    draw_cuboid(x - 0.20, ys + r, z, x + 0.20, ys + r + ring + 0.12, z + 0.25, frame_color)
    if sill:
        draw_cuboid(xl - 0.5, y - 0.22, z, xr + 0.5, y, z + 0.40, frame_color)


def draw_baluster(x, y0, z, h, color):
    """Small vase-shaped baluster built from two cylinders."""
    draw_cylinder(x, y0, z, 0.08, 0.08, h, 6, color, False, False)
    draw_cylinder(x, y0 + h * 0.35, z, 0.14, 0.14, h * 0.22, 6, color)


def draw_railing(length, height, spacing, color=IVORY):
    """Balustrade running along local +X from 0 to `length`: bottom rail,
    top rail, evenly spaced balusters and a heavier post every 6th slot."""
    bot_h, top_h = 0.15, 0.22
    draw_cuboid(0, 0, -0.12, length, bot_h, 0.12, color)
    draw_cuboid(0, height - top_h, -0.17, length, height, 0.17, color)
    n = max(1, int(length / spacing))
    step = length / n
    for i in range(n + 1):
        bx = i * step
        if i % 6 == 0 or i == n:
            draw_cuboid(bx - 0.16, 0, -0.16, bx + 0.16, height + 0.12, 0.16, color)
        else:
            draw_baluster(bx, bot_h, 0.0, height - top_h - bot_h, color)


def place_railing(x0, z0, x1, z1, y, height, spacing=0.5, color=IVORY):
    """Place a railing between two points on the XZ plane at height y."""
    dx, dz = x1 - x0, z1 - z0
    length = math.hypot(dx, dz)
    ang = math.degrees(math.atan2(-dz, dx))
    glPushMatrix()
    glTranslatef(x0, y, z0)
    glRotatef(ang, 0, 1, 0)
    draw_railing(length, height, spacing, color)
    glPopMatrix()


def draw_panel_frame(x0, y0, x1, y1, z, color=IVORY, t=0.12, proj=0.10):
    """Raised rectangular moulding (a framed wall panel)."""
    draw_cuboid(x0, y0, z, x1, y0 + t, z + proj, color)
    draw_cuboid(x0, y1 - t, z, x1, y1, z + proj, color)
    draw_cuboid(x0, y0, z, x0 + t, y1, z + proj, color)
    draw_cuboid(x1 - t, y0, z, x1, y1, z + proj, color)

def draw_cornice(x0, y0, z0, x1, y1, z1, color, accent):
    """Stepped projecting cornice: inset lower band, main band, overhanging crown."""
    h = y1 - y0
    inset, over = 0.15, 0.12
    draw_cuboid(x0 + inset, y0, z0 + inset, x1 - inset, y0 + 0.30 * h, z1 - inset, accent)
    draw_cuboid(x0, y0 + 0.30 * h, z0, x1, y0 + 0.70 * h, z1, color)
    draw_cuboid(x0 - over, y0 + 0.70 * h, z0 - over, x1 + over, y1, z1 + over, accent)


def draw_dentils(x0, x1, y, z, proj, spacing, color=IVORY):
    """Row of small block ornaments under a cornice."""
    n = int((x1 - x0) / spacing)
    for i in range(n + 1):
        x = x0 + i * spacing
        draw_cuboid(x - 0.14, y, z, x + 0.14, y + 0.40, z + proj, color)


def draw_pediment():
    """Large triangular pediment: thick tympanum, raised raking border,
    corner finial pillars and a small gold/blue emblem."""
    hw, h = 8.4, 4.6
    y0 = 17.2                                  # sits on the top cornice
    z_back, z_front = -3.0, 3.9
    glPushMatrix()
    glTranslatef(0.0, y0, 0.0)
    draw_triangle_prism(hw, h, z_back, z_front, WALL)

    d = 0.45
    L = math.hypot(hw, h)
    xi = hw * (1.0 - d / h) - d * L / h        # inner half-width at y = d
    ha = h - d * L / hw                         # inner apex height
    zb, zf = z_front - 0.05, z_front + 0.30
    draw_extruded_polygon([(-hw, 0), (-xi, d), (0, ha), (0, h)], zb, zf, IVORY)
    draw_extruded_polygon([(hw, 0), (0, h), (0, ha), (xi, d)], zb, zf, IVORY)
    draw_extruded_polygon([(-hw, 0), (hw, 0), (xi, d), (-xi, d)], zb, zf, IVORY)

    draw_cylinder_z(0.0, 2.3, z_front, 1.25, 0.20, 28, GOLD)
    draw_cylinder_z(0.0, 2.3, z_front, 0.98, 0.26, 28, EMBLEM_BLUE)
    draw_cylinder_z(0.0, 2.3, z_front, 0.38, 0.32, 20, GOLD)
    glPopMatrix()

    for sx in (-1, 1):
        px = sx * 7.6
        draw_cuboid(px - 0.45, y0, -0.9, px + 0.45, y0 + 3.0, 0.0, IVORY)
        draw_cuboid(px - 0.65, y0 + 3.0, -1.1, px + 0.65, y0 + 3.35, 0.2, IVORY)
        draw_cuboid(px - 0.40, y0 + 3.35, -0.85, px + 0.40, y0 + 3.70, -0.05, IVORY)


def draw_signboard():
    """Reddish-orange signboard with a gold logo disc and pale text-like bars
    (real Bengali lettering is deliberately not attempted)."""
    z = 3.0
    y0, y1, hw = 13.45, 15.05, 5.6
    draw_cuboid(-hw, y0, z, hw, y1, z + 0.25, SIGN_RED)
    fr = 0.12
    draw_cuboid(-hw - fr, y1, z, hw + fr, y1 + fr, z + 0.33, IVORY)
    draw_cuboid(-hw - fr, y0 - fr, z, hw + fr, y0, z + 0.33, IVORY)
    draw_cuboid(-hw - fr, y0, z, -hw, y1, z + 0.33, IVORY)
    draw_cuboid(hw, y0, z, hw + fr, y1, z + 0.33, IVORY)
    draw_cylinder_z(-4.9, 14.25, z + 0.25, 0.46, 0.06, 16, GOLD)
    draw_cylinder_z(-4.9, 14.25, z + 0.31, 0.22, 0.04, 12, EMBLEM_BLUE)
    widths = [0.35, 0.25, 0.5, 0.3, 0.4, 0.25, 0.6, 0.3, 0.3, 0.45, 0.25, 0.5, 0.35, 0.3, 0.55, 0.25, 0.4, 0.3]
    x = -3.9
    for w in widths:
        draw_cuboid(x, 14.0, z + 0.25, x + w, 14.5, z + 0.30, SIGN_TEXT)
        x += w + 0.15

def draw_wing_face(x_start, n, bw, ext_start, ext_end):
    """One row of bays in local coordinates (front plane z = 0, +Z outward).
    Ground-floor arched windows, upper-floor arched windows behind a veranda
    balustrade, pilasters between bays and a dentil course under the cornice.
    ext_start / ext_end extend the balcony round building corners."""
    x_end = x_start + n * bw
    for i in range(n):
        cx = x_start + (i + 0.5) * bw
        draw_arched_window(cx, 1.4, 0.0, 2.0, 4.1, WALL_T, WALL, bw, PLINTH_H, FLOOR1_TOP)
        draw_arched_window(cx, 7.9, 0.0, 2.0, 4.0, WALL_T, WALL, bw, FLOOR2_BOT, WING_WALL_TOP)
    for i in range(n + 1):
        px = x_start + i * bw
        draw_pilaster(px, PLINTH_H, FLOOR1_TOP, 0.0)
        draw_pilaster(px, FLOOR2_BOT, WING_WALL_TOP, 0.0)
    # veranda: projecting slab + balustrade in front of the upper windows
    draw_cuboid(x_start - ext_start, 7.0, 0.0, x_end + ext_end, 7.4, 1.1, IVORY)
    rx0 = x_start - ext_start + 0.15
    rx1 = x_end + ext_end - 0.15
    glPushMatrix()
    glTranslatef(rx0, 7.4, 0.95)
    draw_railing(rx1 - rx0, 1.2, 0.5, IVORY)
    glPopMatrix()
    draw_dentils(x_start, x_end, WING_WALL_TOP - 0.40, 0.0, 0.45, 0.55)


def draw_wing():
    """Right-hand wing (x = 9 .. 30).  The left wing is this mirrored."""
    x0, x1 = WING_X0, WING_X1
    # hidden core volume (the wall layer in front of it carries the openings)
    draw_cuboid(x0, 0.0, -WING_DEPTH, x1 - WALL_T, WING_WALL_TOP, -WALL_T, WALL_SHADE)
    # stone plinth, floor division band, stepped cornice (these wrap the whole wing)
    draw_cuboid(x0, 0.0, -WING_DEPTH - 0.3, x1 + 0.3, PLINTH_H, 0.3, STONE)
    draw_cuboid(x0, FLOOR1_TOP, -WING_DEPTH - 0.25, x1 + 0.25, FLOOR2_BOT, 0.3, IVORY)
    draw_cornice(x0, WING_WALL_TOP, -WING_DEPTH - 0.9, x1 + 0.9, WING_ROOF, 0.9, WALL, IVORY)
    # front row of bays, and a row on the end wall (rotated 90 degrees)
    draw_wing_face(x0, 6, 3.5, 0.0, 1.1)
    glPushMatrix()
    glTranslatef(x1, 0.0, 0.0)
    glRotatef(90.0, 0, 1, 0)                 # local +Z -> world +X, local +X -> world -Z
    draw_wing_face(0.0, 3, 4.0, 1.1, 0.0)
    glPopMatrix()
    # roof parapet balustrade (front, end, back, inner return)
    ry, rh = WING_ROOF, 1.3
    place_railing(x0 + 0.3, 0.5, x1 + 0.5, 0.5, ry, rh)
    place_railing(x1 + 0.5, 0.5, x1 + 0.5, -WING_DEPTH - 0.5, ry, rh)
    place_railing(x1 + 0.5, -WING_DEPTH - 0.5, x0 + 0.3, -WING_DEPTH - 0.5, ry, rh)
    place_railing(x0 + 0.3, -WING_DEPTH - 0.5, x0 + 0.3, 0.5, ry, rh)


def draw_central_block():
    """Projecting central block: platform, steps, recessed two-storey loggia,
    four paired columns, entablature with signboard, cornice and pediment."""
    hw = BLOCK_HALF_W
    zf = -3.0                               # front plane of the recessed back wall
    inner = 8.2                             # loggia inner half-width

    # hidden core volume behind the loggia
    draw_cuboid(-hw, 0.0, -WING_DEPTH, hw, 16.0, zf - WALL_T, WALL_SHADE)
    # platform and steps
    draw_cuboid(-hw - 0.6, 0.0, zf - WALL_T, hw + 0.6, PLINTH_H, 4.6, STONE)
    draw_cuboid(-4.8, 0.0, 4.6, 4.8, 0.55, 5.2, STONE)
    draw_cuboid(-4.8, 0.0, 5.2, 4.8, 0.27, 5.9, STONE)

    # side "cheek" walls closing the loggia
    for sx in (-1, 1):
        draw_cuboid(min(sx * inner, sx * hw), PLINTH_H, zf - WALL_T,
                    max(sx * inner, sx * hw), 12.5, 3.0, WALL)

    # ----- recessed back wall: ground floor (entrance arch + two side windows)
    draw_arched_window(0.0, PLINTH_H, zf, 3.6, 5.0, WALL_T, LOGGIA_WALL, 6.0, PLINTH_H, FLOOR1_TOP, sill=False)
    draw_arched_window(-5.6, 1.6, zf, 2.2, 3.9, WALL_T, LOGGIA_WALL, 5.2, PLINTH_H, FLOOR1_TOP)
    draw_arched_window(5.6, 1.6, zf, 2.2, 3.9, WALL_T, LOGGIA_WALL, 5.2, PLINTH_H, FLOOR1_TOP)
    # first-floor balcony slab and railing
    draw_cuboid(-inner, FLOOR1_TOP, zf - WALL_T, inner, 6.9, 1.2, IVORY)
    place_railing(-inner + 0.15, 1.0, inner - 0.15, 1.0, 6.9, 1.2)
    # upper loggia: four arched openings, twin windows split by a colonnette
    for cx, bw in ((-5.7, 5.0), (-1.6, 3.2), (1.6, 3.2), (5.7, 5.0)):
        draw_arched_window(cx, 7.5, zf, 2.0, 4.1, WALL_T, LOGGIA_WALL, bw, 6.9, 12.5)
    draw_column(0.0, 6.9, zf + 0.35, 0.20, 4.6, IVORY)

    # ----- four big columns in two pairs
    for cx in (-7.4, -4.5, 4.5, 7.4):
        draw_column(cx, PLINTH_H, 2.0, 0.95, 12.5 - PLINTH_H, IVORY)

    # ----- entablature, signboard, cornice
    draw_cuboid(-hw, 12.5, zf - WALL_T, hw, 16.0, 3.0, WALL)
    draw_cuboid(-hw - 0.15, 12.5, zf - WALL_T, hw + 0.15, 13.0, 3.2, IVORY)       # architrave
    for sx in (-1, 1):                                                                # framed frieze panels
        a, b = sorted((sx * 5.95, sx * 8.7))
        draw_panel_frame(a, 13.45, b, 15.05, 3.0)
    draw_signboard()
    draw_dentils(-8.8, 8.8, 15.6, 3.0, 0.45, 0.55)
    draw_cornice(-9.7, 16.0, -WING_DEPTH - 0.7, 9.7, 17.2, 4.1, WALL, IVORY)

    draw_pediment()


def draw_main_building():
    draw_wing()
    glPushMatrix()
    glScalef(-1.0, 1.0, 1.0)                # mirror -> left wing (symmetry about x = 0)
    draw_wing()
    glPopMatrix()
    draw_central_block()

def draw_tree(x, z, s=1.0):
    draw_cylinder(x, 0.0, z, 0.55 * s, 0.30 * s, 5.0 * s, 10, TRUNK, True, False)
    blobs = [(0, 6.5, 0, 3.2), (-2.0, 5.5, 0.8, 2.3), (2.0, 5.8, -0.6, 2.4), (0.4, 8.2, 0.5, 2.0)]
    for i, (dx, dy, dz, r) in enumerate(blobs):
        glPushMatrix()
        glTranslatef(x + dx * s, dy * s, z + dz * s)
        set_color(LEAF_COLORS[i % len(LEAF_COLORS)])
        glutSolidSphere(r * s, 14, 10)
        glPopMatrix()


def draw_environment():
    """Lawn, paved forecourt (and optional trees)."""
    draw_cuboid(-400.0, -0.60, -400.0, 400.0, -0.25, 400.0, GROUND_LAWN)
    draw_cuboid(-46.0, -0.25, -20.0, 46.0, 0.0, 36.0, GROUND_COURT)
    if SHOW_TREES:
        for tx in (-40.0, -34.0, 36.0, 42.0):
            draw_tree(tx, 4.0, 1.1)


def draw_scene():
    draw_environment()
    draw_main_building()

def init_lighting():
    glEnable(GL_LIGHTING)
    glEnable(GL_LIGHT0)
    glEnable(GL_LIGHT1)
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, (0.32, 0.32, 0.34, 1.0))
    # key light: above, in front, slightly to the right
    glLightfv(GL_LIGHT0, GL_AMBIENT, (0.0, 0.0, 0.0, 1.0))
    glLightfv(GL_LIGHT0, GL_DIFFUSE, (0.74, 0.72, 0.66, 1.0))
    glLightfv(GL_LIGHT0, GL_SPECULAR, (0.0, 0.0, 0.0, 1.0))
    # soft fill from the opposite side so shaded walls and the back stay readable
    glLightfv(GL_LIGHT1, GL_AMBIENT, (0.0, 0.0, 0.0, 1.0))
    glLightfv(GL_LIGHT1, GL_DIFFUSE, (0.26, 0.28, 0.33, 1.0))
    glLightfv(GL_LIGHT1, GL_SPECULAR, (0.0, 0.0, 0.0, 1.0))
    glEnable(GL_COLOR_MATERIAL)
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE)
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (0.05, 0.05, 0.05, 1.0))
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 8.0)


def place_lights():
    """Called after gluLookAt so the lights stay fixed in the world."""
    glLightfv(GL_LIGHT0, GL_POSITION, (0.35, 0.55, 0.75, 0.0))
    glLightfv(GL_LIGHT1, GL_POSITION, (-0.60, 0.30, -0.70, 0.0))

def display():
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()
    update_camera()
    place_lights()
    glCallList(scene_list)
    glutSwapBuffers()

def reshape(w, h):
    global win_w, win_h
    h = max(h, 1)
    win_w, win_h = w, h
    glViewport(0, 0, w, h)
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(40.0, w / float(h), 2.0, 600.0)
    glMatrixMode(GL_MODELVIEW)

def keyboard(key, x, y):
    global show_help
    k = key.decode("latin-1") if isinstance(key, bytes) else str(key)
    if k == "\x1b":
        sys.exit(0)
    k = k.lower()
    if k == "w":
        zoom(0.90)
    elif k == "s":
        zoom(1.10)
    elif k == "a":
        orbit(-4.0, 0.0)
    elif k == "d":
        orbit(4.0, 0.0)
    elif k == "q":
        orbit(0.0, 3.0)
    elif k == "e":
        orbit(0.0, -3.0)
    elif k in ("+", "="):
        zoom(0.92)
    elif k in ("-", "_"):
        zoom(1.08)
    elif k == "r":
        reset_camera()
    elif k == "h":
        show_help = not show_help
    glutPostRedisplay()


def special_keys(key, x, y):
    if key == GLUT_KEY_LEFT:
        orbit(-4.0, 0.0)
    elif key == GLUT_KEY_RIGHT:
        orbit(4.0, 0.0)
    elif key == GLUT_KEY_UP:
        orbit(0.0, 3.0)
    elif key == GLUT_KEY_DOWN:
        orbit(0.0, -3.0)
    glutPostRedisplay()

def mouse(button, state, x, y):
    global mouse_dragging, last_mouse_x, last_mouse_y
    if button == GLUT_LEFT_BUTTON:
        mouse_dragging = (state == GLUT_DOWN)
        last_mouse_x, last_mouse_y = x, y
    elif state == GLUT_DOWN and button == 3:      # wheel up (older GLUT)
        zoom(0.92)
        glutPostRedisplay()
    elif state == GLUT_DOWN and button == 4:      # wheel down (older GLUT)
        zoom(1.08)
        glutPostRedisplay()


def mouse_wheel(wheel, direction, x, y):          # FreeGLUT
    zoom(0.92 if direction > 0 else 1.08)
    glutPostRedisplay()


def motion(x, y):
    global last_mouse_x, last_mouse_y
    if mouse_dragging:
        orbit(-(x - last_mouse_x) * 0.35, (y - last_mouse_y) * 0.30)
        last_mouse_x, last_mouse_y = x, y
        glutPostRedisplay()

def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def orbit(d_az, d_el):
    global cam_az, cam_el
    cam_az = (cam_az + d_az) % 360.0
    cam_el = clamp(cam_el + d_el, 1.0, 85.0)      # never flips over or goes underground


def zoom(factor):
    global cam_dist
    cam_dist = clamp(cam_dist * factor, 22.0, 220.0)


def reset_camera():
    global cam_dist, cam_az, cam_el
    cam_dist, cam_az, cam_el = CAM_DEFAULT["dist"], CAM_DEFAULT["az"], CAM_DEFAULT["el"]


def update_camera():
    """Spherical -> Cartesian camera position, then gluLookAt at the target."""
    az, el = math.radians(cam_az), math.radians(cam_el)
    ex = cam_target[0] + cam_dist * math.cos(el) * math.sin(az)
    ey = cam_target[1] + cam_dist * math.sin(el)
    ez = cam_target[2] + cam_dist * math.cos(el) * math.cos(az)
    gluLookAt(ex, ey, ez, cam_target[0], cam_target[1], cam_target[2], 0.0, 1.0, 0.0)

def init_gl():
    glClearColor(SKY[0], SKY[1], SKY[2], 1.0)
    glEnable(GL_DEPTH_TEST)
    glDepthFunc(GL_LEQUAL)
    glShadeModel(GL_SMOOTH)
    glEnable(GL_NORMALIZE)
    init_lighting()


def build_scene_list():
    """Compile the static geometry once; display() just calls the list."""
    global scene_list
    scene_list = glGenLists(1)
    glNewList(scene_list, GL_COMPILE)
    draw_scene()
    glEndList()

def main():
    glutInit(sys.argv)
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH)
    glutInitWindowSize(WINDOW_W, WINDOW_H)
    glutCreateWindow(WINDOW_TITLE)
    init_gl()
    build_scene_list()
    glutDisplayFunc(display)
    glutReshapeFunc(reshape)
    glutKeyboardFunc(keyboard)
    glutSpecialFunc(special_keys)
    glutMouseFunc(mouse)
    glutMotionFunc(motion)
    try:
        glutMouseWheelFunc(mouse_wheel)
    except Exception:
        pass                     
    glutMainLoop()


if __name__ == "__main__":
    main()