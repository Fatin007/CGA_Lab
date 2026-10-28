// =============================================================================
//  g++ AhsanManzil.cpp -o AhsanManzil.exe -lfreeglut -lopengl32 -lglu32
//
//  COORDINATE SYSTEM (1 unit ~ 1 metre-ish, derived from the reference image)
//     X : across the building (left/right)     Y : up     Z : depth
//     Front facade faces -Z, building centred on the origin, ground at y = 0.
//     Overall: width 47.2, wings 10.5 high, centre block 14.3, dome tip ~20.
// =============================================================================

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <GL/freeglut.h>
#include <GL/glu.h>

// ----------------------------------------------------------------------------
// 1. Constants
// ----------------------------------------------------------------------------
static const float PI = 3.14159265358979f;

static const float WING_HALF_W   = 23.5f;   // main block half width
static const float PAV_HALF_W    = 23.6f;   // end pavilions half width
static const float PAV_INNER_X   = 16.4f;   // where pavilions start
static const float CENTER_HALF_W = 4.1f;    // central block half width
static const float WING_FRONT_Z  = -6.0f;   // front wall of wings
static const float PAV_FRONT_Z   = -6.6f;   // pavilions project forward
static const float CENTER_FRONT_Z= -7.0f;   // centre block projects further
static const float FLOOR_H       = 10.3f;   // wall height up to cornice cap
static const float ROOF_Y        = 10.55f;  // roof slab top

static const int   ARCH_SEG = 12;           // segments in a semicircular arch

// ----------------------------------------------------------------------------
// 2. Camera state
// ----------------------------------------------------------------------------
static float cameraDistance  = 72.0f;
static float cameraAzimuth   = 28.0f;   // degrees, 0 = looking at front facade
static float cameraElevation = 16.0f;   // degrees above the horizon
static float cameraTargetX   = 0.0f;
static float cameraTargetY   = 8.0f;
static float cameraTargetZ   = 0.0f;
static bool  mouseDragging   = false;
static int   lastMouseX = 0, lastMouseY = 0;
static int   winW = 1200, winH = 800;
static bool  showHelp = true;

static GLUquadric* gQuad = NULL;

// ----------------------------------------------------------------------------
// 3. Colours (fixed-function material colours)
// ----------------------------------------------------------------------------
struct Col { float r, g, b; };
static const Col C_WALL       = {0.94f, 0.64f, 0.58f};
static const Col C_WALL_BAND  = {0.97f, 0.72f, 0.66f};
static const Col C_LIGHT      = {1.00f, 0.80f, 0.74f};
static const Col C_PALE       = {1.00f, 0.88f, 0.82f};
static const Col C_DARK       = {0.28f, 0.07f, 0.08f};
static const Col C_REVEAL     = {0.68f, 0.35f, 0.32f};
static const Col C_MAROON     = {0.48f, 0.18f, 0.18f};
static const Col C_DOME       = {0.94f, 0.59f, 0.53f};
static const Col C_RIB        = {0.73f, 0.34f, 0.30f};
static const Col C_STONE      = {0.85f, 0.61f, 0.56f};
static const Col C_STONE_LT   = {0.96f, 0.73f, 0.67f};
static const Col C_STAIR_SIDE = {0.72f, 0.42f, 0.39f};
static const Col C_GROUND     = {0.58f, 0.49f, 0.34f};
static const Col C_PLAZA      = {0.72f, 0.59f, 0.44f};
static const Col C_CLOUD      = {0.99f, 0.99f, 1.00f};

static void setCol(const Col& c) { glColor3f(c.r, c.g, c.b); }

// ----------------------------------------------------------------------------
// 4. Basic geometry helpers
// ----------------------------------------------------------------------------

// Axis aligned box with per-face normals. Bottom face is skipped for objects
// resting on the ground (y0 ~ 0).
static void drawCuboid(float x0, float y0, float z0, float x1, float y1, float z1)
{
    glBegin(GL_QUADS);
    glNormal3f(0, 0, -1);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glNormal3f(0, 0, 1);
    glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y0, z1);
    glNormal3f(-1, 0, 0);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1); glVertex3f(x0, y0, z1);
    glNormal3f(1, 0, 0);
    glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
    glNormal3f(0, 1, 0);
    glVertex3f(x0, y1, z0); glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0);
    if (y0 > 0.001f) {
        glNormal3f(0, -1, 0);
        glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    }
    glEnd();
}

// Cylinder / cone frustum along +Y starting at the local origin.
// r0 = bottom radius, r1 = top radius. Smooth side normals, flat top cap.
static void drawFrustum(float r0, float r1, float h, int n)
{
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= n; ++i) {
        float a = 2.0f * PI * i / n, c = cosf(a), s = sinf(a);
        glNormal3f(c * h, r0 - r1, s * h);
        glVertex3f(r0 * c, 0.0f, r0 * s);
        glVertex3f(r1 * c, h, r1 * s);
    }
    glEnd();
    if (r1 > 0.0001f) {
        glNormal3f(0, 1, 0);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0, h, 0);
        for (int i = 0; i <= n; ++i) {
            float a = 2.0f * PI * i / n;
            glVertex3f(r1 * cosf(a), h, r1 * sinf(a));
        }
        glEnd();
    }
}

static void drawCylinder(float r, float h, int n) { drawFrustum(r, r, h, n); }

static void drawBall(float r)
{
    gluSphere(gQuad, r, 14, 10);
}

// A patch of an ellipsoidal dome: radius R at the base, height H at the pole.
//   phi : elevation angle (0 = base ring, PI/2 = pole)
//   th  : angle around the Y axis
// 'sc' scales the patch slightly (used to lift ribs off the surface).
static void domePatch(float R, float H, float phi0, float phi1,
                      float th0, float th1, int ns, int nt, float sc)
{
    for (int i = 0; i < ns; ++i) {
        float p0 = phi0 + (phi1 - phi0) * i / ns;
        float p1 = phi0 + (phi1 - phi0) * (i + 1) / ns;
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= nt; ++j) {
            float t = th0 + (th1 - th0) * j / nt;
            float ct = cosf(t), st = sinf(t);
            for (int k = 0; k < 2; ++k) {
                float p = k ? p1 : p0;
                float cp = cosf(p), sp = sinf(p);
                // gradient of the ellipsoid equation = outward normal
                glNormal3f(cp * ct / R, sp / H, cp * st / R);
                glVertex3f(sc * R * cp * ct, sc * H * sp, sc * R * cp * st);
            }
        }
        glEnd();
    }
}

// ----------------------------------------------------------------------------
// 5. Dome, drum and finial
// ----------------------------------------------------------------------------
static void drawBulb(float R, float H)       // small turret dome
{
    setCol(C_DOME);
    domePatch(R, H, 0.0f, PI * 0.5f, 0.0f, 2.0f * PI, 6, 16, 1.0f);
}

static void drawArchedWindow(float cx, float yb, float w, float hRect, float zb,
                             float frameW, float frameD, bool rails);

static void drawDome()
{
    const float baseY   = 14.3f;     // top of centre block
    const float drumR   = 3.2f;
    const float drumH   = 1.3f;
    const float domeR   = 2.85f;
    const float domeH   = 3.1f;

    glPushMatrix();
    glTranslatef(0.0f, baseY, 0.0f);

    // --- drum -----------------------------------------------------------
    setCol(C_WALL);
    drawCylinder(drumR, drumH, 40);

    // decorative rings around the drum
    setCol(C_LIGHT);  drawCylinder(drumR + 0.15f, 0.25f, 40);
    glPushMatrix(); glTranslatef(0, 0.95f, 0); setCol(C_MAROON); drawCylinder(drumR + 0.16f, 0.23f, 40); glPopMatrix();
    glPushMatrix(); glTranslatef(0, 1.18f, 0); setCol(C_LIGHT);  drawCylinder(drumR + 0.25f, 0.16f, 40); glPopMatrix();

    // eight small arched windows around the drum
    for (int k = 0; k < 8; ++k) {
        glPushMatrix();
        glRotatef(k * 45.0f, 0, 1, 0);
        drawArchedWindow(0.0f, 0.22f, 0.7f, 0.30f, -drumR, 0.1f, 0.12f, false);
        glPopMatrix();
    }

    // --- dome with ribs -------------------------------------------------
    glTranslatef(0.0f, drumH + 0.04f, 0.0f);
    const int   N   = 16;
    const float sec = 2.0f * PI / N;
    const float rib = 0.05f;
    for (int i = 0; i < N; ++i) {
        float a0 = i * sec, a1 = (i + 1) * sec;
        setCol(C_DOME);
        domePatch(domeR, domeH, 0.0f, PI * 0.5f, a0 + rib, a1 - rib, 12, 2, 1.0f);
        setCol(C_RIB);
        domePatch(domeR, domeH, 0.0f, PI * 0.5f, a1 - rib, a1 + rib, 12, 1, 1.012f);
    }

    // --- pedestal, ball and finial spike --------------------------------
    glPushMatrix();
    glTranslatef(0.0f, domeH - 0.12f, 0.0f);
    setCol(C_LIGHT); drawCylinder(0.26f, 0.3f, 16);
    glTranslatef(0.0f, 0.3f, 0.0f);
    setCol(C_WALL);  drawBall(0.2f);
    setCol(C_MAROON); drawFrustum(0.1f, 0.0f, 1.2f, 10);
    glPopMatrix();

    glPopMatrix();
}

// ----------------------------------------------------------------------------
// 6. Arches, windows, columns, cornices
// ----------------------------------------------------------------------------

// Outline of an arched opening: left-bottom, up the left side, over the
// semicircle, down the right side.  Returns the number of points.
static int archPoints(float cx, float yb, float hw, float hRect, float* px, float* py)
{
    int n = 0;
    px[n] = cx - hw; py[n] = yb;         ++n;
    px[n] = cx - hw; py[n] = yb + hRect; ++n;
    for (int i = 1; i < ARCH_SEG; ++i) {
        float a = PI - PI * i / ARCH_SEG;
        px[n] = cx + hw * cosf(a);
        py[n] = yb + hRect + hw * sinf(a);
        ++n;
    }
    px[n] = cx + hw; py[n] = yb + hRect; ++n;
    px[n] = cx + hw; py[n] = yb;         ++n;
    return n;
}

// Arched window in a wall facing -Z whose surface is at z = zb.
//   cx,yb  : centre x and bottom y of the opening
//   w      : opening width,  hRect : height of the straight part
//   frameW : width of the raised frame,  frameD : how far it projects (-Z)
// The dark opening sits on the wall surface while the frame stands proud of
// it, so the opening reads as deeply recessed.
static void drawArchedWindow(float cx, float yb, float w, float hRect, float zb,
                             float frameW, float frameD, bool rails)
{
    const float hw = w * 0.5f;
    float ix[ARCH_SEG + 4], iy[ARCH_SEG + 4], ox[ARCH_SEG + 4], oy[ARCH_SEG + 4];
    int n = archPoints(cx, yb, hw, hRect, ix, iy);
    archPoints(cx, yb, hw + frameW, hRect, ox, oy);
    const float zf = zb - frameD;     // front of the frame
    const float zd = zb - 0.03f;      // dark panel (just off the wall)

    // dark recessed panel
    setCol(C_DARK);
    glNormal3f(0, 0, -1);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(cx, yb + hRect * 0.5f, zd);
    for (int i = 0; i < n; ++i) glVertex3f(ix[i], iy[i], zd);
    glVertex3f(ix[0], iy[0], zd);
    glEnd();

    // front face of the frame
    setCol(C_LIGHT);
    glNormal3f(0, 0, -1);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i < n; ++i) {
        glVertex3f(ox[i], oy[i], zf);
        glVertex3f(ix[i], iy[i], zf);
    }
    glEnd();

    // inner reveal (inside of the frame) and outer side of the frame
    for (int pass = 0; pass < 2; ++pass) {
        const float* px = pass ? ox : ix;
        const float* py = pass ? oy : iy;
        setCol(pass ? C_WALL_BAND : C_REVEAL);
        glBegin(GL_QUADS);
        for (int i = 0; i < n - 1; ++i) {
            float dx = px[i + 1] - px[i], dy = py[i + 1] - py[i];
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1e-5f) continue;
            if (pass == 0) glNormal3f(dy / len, -dx / len, 0);    // inward
            else           glNormal3f(-dy / len, dx / len, 0);    // outward
            glVertex3f(px[i],     py[i],     zf);
            glVertex3f(px[i + 1], py[i + 1], zf);
            glVertex3f(px[i + 1], py[i + 1], zd);
            glVertex3f(px[i],     py[i],     zd);
        }
        glEnd();
    }

    // projecting sill
    setCol(C_LIGHT);
    drawCuboid(cx - hw - frameW - 0.12f, yb - 0.16f, zf - 0.1f,
               cx + hw + frameW + 0.12f, yb, zb);

    // horizontal rails inside the opening
    if (rails) {
        setCol(C_PALE);
        for (int k = 1; k <= 3; ++k) {
            float y = yb + hRect * k / 4.0f - 0.04f;
            drawCuboid(cx - hw, y, zb - 0.14f, cx + hw, y + 0.08f, zd);
        }
    }
}

// Rectangular recessed window with a maroon hood (used on the end pavilions).
static void drawRectWindow(float cx, float yb, float w, float h, float zb)
{
    const float hw = w * 0.5f;
    setCol(C_DARK);
    glNormal3f(0, 0, -1);
    glBegin(GL_QUADS);
    glVertex3f(cx - hw, yb, zb - 0.03f); glVertex3f(cx + hw, yb, zb - 0.03f);
    glVertex3f(cx + hw, yb + h, zb - 0.03f); glVertex3f(cx - hw, yb + h, zb - 0.03f);
    glEnd();
    setCol(C_LIGHT);
    drawCuboid(cx - hw - 0.22f, yb, zb - 0.3f, cx - hw, yb + h, zb);
    drawCuboid(cx + hw, yb, zb - 0.3f, cx + hw + 0.22f, yb + h, zb);
    drawCuboid(cx - hw - 0.3f, yb - 0.18f, zb - 0.4f, cx + hw + 0.3f, yb, zb);
    drawCuboid(cx - hw, yb + h * 0.68f, zb - 0.12f, cx + hw, yb + h * 0.68f + 0.08f, zb - 0.03f);
    setCol(C_MAROON);
    drawCuboid(cx - hw - 0.32f, yb + h, zb - 0.6f, cx + hw + 0.32f, yb + h + 0.38f, zb);
}

// Flat pilaster with base and capital.
static void drawPilaster(float x, float y0, float y1, float w, float zWall, float proj)
{
    setCol(C_LIGHT);
    drawCuboid(x - w * 0.5f, y0 + 0.2f, zWall - proj, x + w * 0.5f, y1 - 0.25f, zWall);
    setCol(C_PALE);
    drawCuboid(x - w * 0.5f - 0.08f, y1 - 0.25f, zWall - proj - 0.08f, x + w * 0.5f + 0.08f, y1, zWall);
    drawCuboid(x - w * 0.5f - 0.06f, y0, zWall - proj - 0.06f, x + w * 0.5f + 0.06f, y0 + 0.2f, zWall);
}

// Round column with square base and capital.
static void drawColumn(float x, float y0, float z, float r, float h)
{
    setCol(C_LIGHT);
    drawCuboid(x - r * 1.5f, y0, z - r * 1.5f, x + r * 1.5f, y0 + 0.2f, z + r * 1.5f);
    glPushMatrix();
    glTranslatef(x, y0 + 0.2f, z);
    drawFrustum(r, r * 0.88f, h - 0.5f, 16);
    glPopMatrix();
    setCol(C_PALE);
    drawCuboid(x - r * 1.7f, y0 + h - 0.3f, z - r * 1.7f, x + r * 1.7f, y0 + h, z + r * 1.7f);
}

// Horizontal moulding.
static void drawCornice(float x0, float x1, float y, float h, float zWall, float proj, const Col& c)
{
    setCol(c);
    drawCuboid(x0, y, zWall - proj, x1, y + h, zWall);
}

// Balustrade: dark backing, rails and small balusters.
static void drawBalustrade(float x0, float x1, float z, float y0, float h)
{
    setCol(C_MAROON);
    drawCuboid(x0, y0 + 0.15f, z - 0.05f, x1, y0 + h - 0.15f, z + 0.25f);
    setCol(C_LIGHT);
    drawCuboid(x0, y0, z - 0.25f, x1, y0 + 0.2f, z + 0.25f);
    drawCuboid(x0, y0 + h - 0.2f, z - 0.28f, x1, y0 + h, z + 0.28f);
    int n = (int)((x1 - x0) / 0.5f);
    if (n < 1) n = 1;
    float step = (x1 - x0) / n;
    setCol(C_PALE);
    for (int i = 0; i < n; ++i) {
        float bx = x0 + step * (i + 0.5f);
        drawCuboid(bx - 0.07f, y0 + 0.2f, z - 0.2f, bx + 0.07f, y0 + h - 0.2f, z - 0.06f);
    }
    // end posts
    setCol(C_LIGHT);
    drawCuboid(x0, y0, z - 0.3f, x0 + 0.22f, y0 + h + 0.15f, z + 0.3f);
    drawCuboid(x1 - 0.22f, y0, z - 0.3f, x1, y0 + h + 0.15f, z + 0.3f);
}

// Rooftop turret with an arched opening and a bulb dome.
static void drawTurret(float cx, float cz)
{
    setCol(C_WALL);
    drawCuboid(cx - 0.65f, ROOF_Y, cz - 0.65f, cx + 0.65f, 12.3f, cz + 0.65f);
    setCol(C_LIGHT);
    drawCuboid(cx - 0.75f, ROOF_Y, cz - 0.75f, cx + 0.75f, ROOF_Y + 0.3f, cz + 0.75f);
    drawArchedWindow(cx, 11.0f, 0.55f, 0.55f, cz - 0.65f, 0.07f, 0.1f, false);
    setCol(C_MAROON);
    drawCuboid(cx - 0.8f, 12.3f, cz - 0.8f, cx + 0.8f, 12.55f, cz + 0.8f);
    glPushMatrix();
    glTranslatef(cx, 12.55f, cz);
    drawBulb(0.62f, 0.95f);
    glTranslatef(0, 0.9f, 0);
    setCol(C_MAROON); drawFrustum(0.12f, 0.0f, 0.75f, 8);
    glPopMatrix();
}

// Stepped crown ornament in the middle of each pavilion balustrade.
static void drawCrownOrnament(float cx, float z)
{
    setCol(C_LIGHT);  drawCuboid(cx - 1.0f, 11.45f, z - 0.28f, cx + 1.0f, 11.75f, z + 0.28f);
    setCol(C_PALE);   drawCuboid(cx - 0.7f, 11.75f, z - 0.25f, cx + 0.7f, 12.05f, z + 0.25f);
    setCol(C_LIGHT);  drawCuboid(cx - 0.4f, 12.05f, z - 0.22f, cx + 0.4f, 12.3f, z + 0.22f);
    glPushMatrix();
    glTranslatef(cx, 12.3f, z);
    setCol(C_WALL); drawFrustum(0.2f, 0.04f, 0.45f, 10);
    glPopMatrix();
}

// Corner tower of the central block (pilaster tower with pinnacle).
static void drawCenterTower(float cx)
{
    setCol(C_LIGHT);
    drawCuboid(cx - 0.6f, 10.85f, -7.5f, cx + 0.6f, 13.6f, -6.9f);
    drawArchedWindow(cx, 11.3f, 0.5f, 0.9f, -7.5f, 0.07f, 0.08f, false);
    setCol(C_PALE);
    drawCuboid(cx - 0.75f, 13.6f, -7.65f, cx + 0.75f, 13.95f, -6.75f);
    glPushMatrix();
    glTranslatef(cx, 13.95f, -7.2f);
    setCol(C_WALL);  drawFrustum(0.3f, 0.1f, 0.8f, 12);
    glTranslatef(0, 0.9f, 0);
    setCol(C_LIGHT); drawBall(0.17f);
    glPopMatrix();
}

// ----------------------------------------------------------------------------
// 7. Staircase
// ----------------------------------------------------------------------------
static void drawStaircase()
{
    const int   N    = 15;               // number of steps
    const float rise = 5.5f / N;         // total height 5.5 = entrance floor
    const float run  = 0.6f;
    const float zEnd = CENTER_FRONT_Z;   // top step touches the centre block

    for (int i = 0; i < N; ++i) {
        float t   = (float)i / (N - 1);
        float hw  = 6.4f - (6.4f - 4.1f) * t;      // wide at the bottom
        float z0  = zEnd - (N - i) * run;
        float z1  = z0 + run;
        float top = (i + 1) * rise;

        // riser body
        setCol(C_STONE);
        drawCuboid(-hw, 0.0f, z0, hw, top - 0.05f, z1);
        // tread slab with a small nosing
        setCol(C_STONE_LT);
        drawCuboid(-hw - 0.02f, top - 0.05f, z0 - 0.05f, hw + 0.02f, top, z1);
        // low side walls
        setCol(C_STAIR_SIDE);
        drawCuboid(-hw - 0.6f, 0.0f, z0, -hw, top + 0.4f, z1);
        drawCuboid( hw,        0.0f, z0,  hw + 0.6f, top + 0.4f, z1);
    }
}

// ----------------------------------------------------------------------------
// 8. Facade detail (written for the FRONT, local -Z; mirrored for the back)
// ----------------------------------------------------------------------------
struct Seg { float x0, x1, z; };
static const Seg FRONT_SEGS[5] = {
    {-PAV_HALF_W, -PAV_INNER_X, PAV_FRONT_Z},
    {-PAV_INNER_X, -CENTER_HALF_W, WING_FRONT_Z},
    {-CENTER_HALF_W, CENTER_HALF_W, CENTER_FRONT_Z},
    { CENTER_HALF_W, PAV_INNER_X, WING_FRONT_Z},
    { PAV_INNER_X, PAV_HALF_W, PAV_FRONT_Z}
};

static float zFront(float x)
{
    float ax = fabsf(x);
    if (ax > PAV_INNER_X)   return PAV_FRONT_Z;
    if (ax > CENTER_HALF_W) return WING_FRONT_Z;
    return CENTER_FRONT_Z;
}

static void drawCenterFront()
{
    const float zb = CENTER_FRONT_Z;

    // three tall recessed arches: grand central entrance + two side arches
    drawArchedWindow(0.0f, 5.5f, 1.9f, 3.55f, zb, 0.45f, 0.55f, false);
    for (int s = -1; s <= 1; s += 2) {
        drawArchedWindow(s * 2.5f, 5.5f, 1.1f, 3.3f, zb, 0.2f, 0.3f, false);
        drawColumn(s * 1.58f, 5.5f, zb - 0.12f, 0.12f, 4.4f);
        drawPilaster(s * 3.75f, 5.35f, 10.0f, 0.5f, zb, 0.25f);
    }

    // mouldings above the arches
    setCol(C_MAROON);
    drawCuboid(-4.55f, 10.3f, -7.35f, 4.55f, 10.45f, -6.9f);
    drawCornice(-4.7f, 4.7f, 10.45f, 0.4f, -6.9f, 0.6f, C_LIGHT);

    // oculus-style arched window
    drawArchedWindow(0.0f, 12.7f, 0.9f, 0.6f, zb, 0.18f, 0.25f, false);

    // louvred panel
    setCol(C_DARK);
    glNormal3f(0, 0, -1);
    glBegin(GL_QUADS);
    glVertex3f(-1.05f, 11.15f, zb - 0.03f); glVertex3f(1.05f, 11.15f, zb - 0.03f);
    glVertex3f(1.05f, 12.15f, zb - 0.03f);  glVertex3f(-1.05f, 12.15f, zb - 0.03f);
    glEnd();
    setCol(C_LIGHT);
    drawCuboid(-1.25f, 11.0f, zb - 0.2f, 1.25f, 11.15f, zb);
    drawCuboid(-1.25f, 12.15f, zb - 0.2f, 1.25f, 12.32f, zb);
    drawCuboid(-1.25f, 11.15f, zb - 0.2f, -1.05f, 12.15f, zb);
    drawCuboid(1.05f, 11.15f, zb - 0.2f, 1.25f, 12.15f, zb);
    setCol(C_PALE);
    for (int i = 0; i < 9; ++i) {
        float x = -0.95f + i * 0.2375f;
        drawCuboid(x, 11.15f, zb - 0.12f, x + 0.07f, 12.15f, zb - 0.03f);
    }

    // small balustraded openings on both sides, towers at the corners
    for (int s = -1; s <= 1; s += 2) {
        float x = s * 2.4f;
        drawBalustrade(x - 0.6f, x + 0.6f, zb - 0.2f, 11.0f, 0.9f);
        drawCenterTower(s * 3.7f);
    }
}

static void drawFacadeDetails()
{
    // --- rusticated ground floor bands and string course ------------------
    for (int sgi = 0; sgi < 5; ++sgi) {
        const Seg& sg = FRONT_SEGS[sgi];
        setCol(C_WALL_BAND);
        for (float y = 0.25f; y < 4.9f; y += 0.5f)
            drawCuboid(sg.x0, y, sg.z - 0.06f, sg.x1, y + 0.22f, sg.z);
        drawCornice(sg.x0, sg.x1, 5.0f, 0.35f, sg.z, 0.35f, C_LIGHT);
        drawCornice(sg.x0, sg.x1, 5.35f, 0.08f, sg.z, 0.2f, C_MAROON);
    }

    // --- ground floor arched openings ------------------------------------
    static const float gfX[6] = {21.2f, 18.7f, 15.5f, 12.8f, 10.1f, 7.5f};
    for (int s = -1; s <= 1; s += 2)
        for (int i = 0; i < 6; ++i) {
            float x = s * gfX[i];
            drawArchedWindow(x, 0.25f, 1.4f, 2.55f, zFront(x) - 0.06f, 0.2f, 0.3f, false);
        }

    // --- upper floor arched windows (wings), generated by loop -----------
    for (int s = -1; s <= 1; s += 2)
        for (int k = 0; k < 5; ++k) {
            float x = s * (5.1f + 2.5f * k);
            drawArchedWindow(x, 5.7f, 1.5f, 2.3f, zFront(x), 0.2f, 0.3f, true);
        }

    // --- pilasters between the upper windows -----------------------------
    for (int s = -1; s <= 1; s += 2) {
        for (int k = 0; k < 4; ++k)
            drawPilaster(s * (6.35f + 2.5f * k), 5.43f, 10.0f, 0.45f, WING_FRONT_Z, 0.3f);
        drawPilaster(s * 16.9f, 5.43f, 10.0f, 0.5f, PAV_FRONT_Z, 0.3f);
        drawPilaster(s * 19.95f, 5.43f, 10.0f, 0.5f, PAV_FRONT_Z, 0.3f);
        drawPilaster(s * 23.2f, 5.43f, 10.0f, 0.7f, PAV_FRONT_Z, 0.3f);
    }

    // --- end pavilions: rectangular hooded windows + vents ---------------
    for (int s = -1; s <= 1; s += 2) {
        drawRectWindow(s * 18.8f, 5.7f, 1.6f, 2.55f, PAV_FRONT_Z);
        drawRectWindow(s * 21.1f, 5.7f, 1.6f, 2.55f, PAV_FRONT_Z);
        setCol(C_DARK);
        drawCuboid(s * 18.8f - 0.55f, 9.0f, PAV_FRONT_Z - 0.1f, s * 18.8f + 0.55f, 9.3f, PAV_FRONT_Z);
        drawCuboid(s * 21.1f - 0.55f, 9.0f, PAV_FRONT_Z - 0.1f, s * 21.1f + 0.55f, 9.3f, PAV_FRONT_Z);
    }

    // --- centre block ---------------------------------------------------
    drawCenterFront();

    // --- roofline: balustrades, turrets, crown ornaments ------------------
    for (int s = -1; s <= 1; s += 2) {
        float a = s * 4.9f, b = s * 16.4f;
        drawBalustrade(fminf(a, b), fmaxf(a, b), -6.6f, ROOF_Y, 0.9f);
        a = s * 17.6f; b = s * 22.0f;
        drawBalustrade(fminf(a, b), fmaxf(a, b), -6.6f, ROOF_Y, 0.9f);
        drawTurret(s * 22.6f, -6.2f);
        drawTurret(s * 17.0f, -6.2f);
        drawCrownOrnament(s * 19.8f, -6.6f);
    }
}

// Simplified details for the two side walls. Local frame: wall at z = -23.6.
static void drawSideDetails()
{
    const float zw = -PAV_HALF_W;
    setCol(C_WALL_BAND);
    for (float y = 0.25f; y < 4.9f; y += 0.5f)
        drawCuboid(-6.6f, y, zw - 0.06f, 6.6f, y + 0.22f, zw);
    drawCornice(-6.6f, 6.6f, 5.0f, 0.35f, zw, 0.35f, C_LIGHT);
    drawCornice(-6.6f, 6.6f, 5.35f, 0.08f, zw, 0.2f, C_MAROON);

    static const float wx[4] = {-4.5f, -1.5f, 1.5f, 4.5f};
    for (int i = 0; i < 4; ++i) {
        drawArchedWindow(wx[i], 0.25f, 1.4f, 2.55f, zw - 0.06f, 0.2f, 0.3f, false);
        drawArchedWindow(wx[i], 5.7f, 1.5f, 2.3f, zw, 0.2f, 0.3f, true);
    }
    static const float px[5] = {-6.2f, -3.0f, 0.0f, 3.0f, 6.2f};
    for (int i = 0; i < 5; ++i) drawPilaster(px[i], 5.43f, 10.0f, 0.45f, zw, 0.3f);

    // windows on the sides of the raised centre block (local wall z = -4.1)
    for (int s = -1; s <= 1; s += 2) {
        drawArchedWindow(s * 2.5f, 11.0f, 1.1f, 1.4f, -CENTER_HALF_W, 0.15f, 0.2f, false);
    }
}

// ----------------------------------------------------------------------------
// 9. Main building
// ----------------------------------------------------------------------------
static void drawBaseBlocks()
{
    // main wall masses
    setCol(C_WALL);
    drawCuboid(-WING_HALF_W, 0.0f, WING_FRONT_Z, WING_HALF_W, FLOOR_H, -WING_FRONT_Z);
    drawCuboid(-PAV_HALF_W, 0.0f, PAV_FRONT_Z, -PAV_INNER_X, FLOOR_H, -PAV_FRONT_Z);
    drawCuboid( PAV_INNER_X, 0.0f, PAV_FRONT_Z,  PAV_HALF_W, FLOOR_H, -PAV_FRONT_Z);
    drawCuboid(-CENTER_HALF_W, 0.0f, CENTER_FRONT_Z, CENTER_HALF_W, 14.25f, -CENTER_FRONT_Z);

    // main cornice (dark band) and roof slab
    setCol(C_MAROON);
    drawCuboid(-24.1f, 10.0f, -6.95f, 24.1f, FLOOR_H, 6.95f);
    setCol(C_LIGHT);
    drawCuboid(-24.0f, FLOOR_H, -6.85f, 24.0f, ROOF_Y, 6.85f);

    // crown cornice of the centre block (between the corner towers)
    setCol(C_MAROON);
    drawCuboid(-2.9f, 13.7f, -7.3f, 2.9f, 13.95f, 7.3f);
    setCol(C_LIGHT);
    drawCuboid(-2.9f, 13.95f, -7.4f, 2.9f, 14.35f, 7.4f);

    // low parapet walls along both side edges of the roof
    for (int s = -1; s <= 1; s += 2) {
        float xa = s * 23.0f, xb = s * 23.6f;
        setCol(C_WALL_BAND);
        drawCuboid(fminf(xa, xb), ROOF_Y, -6.6f, fmaxf(xa, xb), 11.35f, 6.6f);
        setCol(C_LIGHT);
        float xc = s * 22.9f, xd = s * 23.7f;
        drawCuboid(fminf(xc, xd), 11.35f, -6.7f, fmaxf(xc, xd), 11.5f, 6.7f);
    }
}

static void drawMainBuilding()
{
    drawBaseBlocks();

    // front and back facades (back = front mirrored in Z)
    glPushMatrix();
    drawFacadeDetails();
    glPopMatrix();
    glPushMatrix();
    glScalef(1.0f, 1.0f, -1.0f);
    drawFacadeDetails();
    glPopMatrix();

    // left and right side walls
    glPushMatrix(); glRotatef( 90.0f, 0, 1, 0); drawSideDetails(); glPopMatrix();
    glPushMatrix(); glRotatef(-90.0f, 0, 1, 0); drawSideDetails(); glPopMatrix();

    drawDome();
    drawStaircase();
}

// ----------------------------------------------------------------------------
// 10. Environment
// ----------------------------------------------------------------------------
static void drawCloud(float x, float y, float z, float s)
{
    static const float off[5][4] = {
        {0.0f, 0.0f, 0.0f, 1.0f}, {1.1f, -0.1f, 0.2f, 0.8f}, {-1.1f, -0.15f, -0.1f, 0.75f},
        {0.5f, 0.45f, 0.0f, 0.75f}, {-0.45f, 0.35f, 0.1f, 0.7f}
    };
    setCol(C_CLOUD);
    for (int i = 0; i < 5; ++i) {
        glPushMatrix();
        glTranslatef(x + off[i][0] * s, y + off[i][1] * s, z + off[i][2] * s);
        glScalef(1.0f, 0.6f, 0.9f);
        gluSphere(gQuad, off[i][3] * s, 14, 10);
        glPopMatrix();
    }
}

static void drawEnvironment()
{
    // ground
    setCol(C_GROUND);
    glNormal3f(0, 1, 0);
    glBegin(GL_QUADS);
    glVertex3f(-500, -0.02f, -500); glVertex3f(-500, -0.02f, 500);
    glVertex3f(500, -0.02f, 500);   glVertex3f(500, -0.02f, -500);
    glEnd();

    // paved forecourt in front of the palace
    setCol(C_PLAZA);
    glBegin(GL_QUADS);
    glVertex3f(-34, 0.01f, -46); glVertex3f(-34, 0.01f, 14);
    glVertex3f(34, 0.01f, 14);   glVertex3f(34, 0.01f, -46);
    glEnd();

    // sky clouds
    drawCloud(-70, 42,  90, 9);
    drawCloud( 45, 50,  95, 11);
    drawCloud(110, 40,  10, 10);
    drawCloud(-115, 46, -20, 12);
    drawCloud(  0, 55, -120, 11);
    drawCloud(-55, 38, -90, 8);
}

// ----------------------------------------------------------------------------
// 11. Camera
// ----------------------------------------------------------------------------
static void eyePosition(float& ex, float& ey, float& ez)
{
    float az = cameraAzimuth * PI / 180.0f;
    float el = cameraElevation * PI / 180.0f;
    ex = cameraTargetX + cameraDistance * cosf(el) * sinf(az);
    ey = cameraTargetY + cameraDistance * sinf(el);
    ez = cameraTargetZ - cameraDistance * cosf(el) * cosf(az);
}

static void clampCamera()
{
    if (cameraElevation < 3.0f)  cameraElevation = 3.0f;
    if (cameraElevation > 85.0f) cameraElevation = 85.0f;
    if (cameraDistance < 14.0f)  cameraDistance = 14.0f;
    if (cameraDistance > 220.0f) cameraDistance = 220.0f;
    while (cameraAzimuth > 180.0f)  cameraAzimuth -= 360.0f;
    while (cameraAzimuth < -180.0f) cameraAzimuth += 360.0f;

    // keep the camera outside the palace volume
    for (int i = 0; i < 400; ++i) {
        float ex, ey, ez;
        eyePosition(ex, ey, ez);
        bool inside = fabsf(ex) < 25.0f && fabsf(ez) < 8.0f && ey < 21.0f && ey > -1.0f;
        if (!inside) break;
        cameraDistance += 0.5f;
    }
}

static void resetCamera()
{
    cameraDistance = 72.0f; cameraAzimuth = 28.0f; cameraElevation = 16.0f;
    cameraTargetX = 0.0f; cameraTargetY = 8.0f; cameraTargetZ = 0.0f;
}

// ----------------------------------------------------------------------------
// 12. Lighting, overlay, display
// ----------------------------------------------------------------------------
static void setupLighting()
{
    // Soft warm light from the front
    GLfloat pos0[] = {-0.25f, 0.60f, -0.85f, 0.0f};
    GLfloat dif0[] = { 0.95f, 0.91f, 0.88f, 1.0f};
    GLfloat amb0[] = { 0.12f, 0.10f, 0.10f, 1.0f};

    // Gentle fill light from behind-right
    GLfloat pos1[] = { 0.60f, 0.35f, 0.70f, 0.0f};
    GLfloat dif1[] = { 0.35f, 0.34f, 0.38f, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, pos0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb0);

    glLightfv(GL_LIGHT1, GL_POSITION, pos1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, dif1);
}

static void drawText(float x, float y, const char* s)
{
    glRasterPos2f(x, y);
    for (; *s; ++s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s);
}

static void printControls()
{
    printf("\n");
    printf("============================================\n");
    printf("       AHSAN MANZIL - 3D MODEL\n");
    printf("============================================\n");
    printf(" Mouse drag     : Orbit camera\n");
    printf(" Mouse wheel    : Zoom in / out\n");
    printf(" W / S          : Move closer / farther\n");
    printf(" A / D          : Rotate left / right\n");
    printf(" Q / E          : Raise / lower camera\n");
    printf(" + / -          : Zoom in / out\n");
    printf(" Arrow keys     : Rotate / change elevation\n");
    printf(" R              : Reset camera\n");
    printf(" ESC            : Exit\n");
    printf("============================================\n\n");
}

static void display()
{
    // glClearColor(0.72f, 0.82f, 0.93f, 1.0f);
    glClearColor(0.76f, 0.84f, 0.93f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float ex, ey, ez;
    eyePosition(ex, ey, ez);
    gluLookAt(ex, ey, ez, cameraTargetX, cameraTargetY, cameraTargetZ, 0, 1, 0);

    setupLighting();
    drawEnvironment();
    drawMainBuilding();
    glutSwapBuffers();
}

// ----------------------------------------------------------------------------
// 13. Callbacks
// ----------------------------------------------------------------------------
static void reshape(int w, int h)
{
    if (h == 0) h = 1;
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(40.0, (double)w / (double)h, 3.0, 600.0);
    glMatrixMode(GL_MODELVIEW);
}

static void mouse(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON) {
        mouseDragging = (state == GLUT_DOWN);
        lastMouseX = x; lastMouseY = y;
    }
}

static void motion(int x, int y)
{
    if (!mouseDragging) return;
    int dx = x - lastMouseX, dy = y - lastMouseY;
    lastMouseX = x; lastMouseY = y;
    cameraAzimuth   += dx * 0.4f;
    cameraElevation += dy * 0.3f;
    clampCamera();
    glutPostRedisplay();
}

static void mouseWheel(int, int dir, int, int)
{
    cameraDistance -= dir * 3.0f;     // wheel up (dir > 0) zooms in
    clampCamera();
    glutPostRedisplay();
}

static void keyboard(unsigned char key, int, int)
{
    switch (key) {
    case 27: exit(0); break;
    case 'w': case 'W': cameraDistance -= 2.0f; break;
    case 's': case 'S': cameraDistance += 2.0f; break;
    case 'a': case 'A': cameraAzimuth -= 3.0f; break;
    case 'd': case 'D': cameraAzimuth += 3.0f; break;
    case 'q': case 'Q': cameraElevation += 2.0f; break;
    case 'e': case 'E': cameraElevation -= 2.0f; break;
    case '+': case '=': cameraDistance -= 2.0f; break;
    case '-': case '_': cameraDistance += 2.0f; break;
    case 'r': case 'R': resetCamera(); break;
    case 'h': case 'H': showHelp = !showHelp; break;
    default: return;
    }
    clampCamera();
    glutPostRedisplay();
}

static void special(int key, int, int)
{
    switch (key) {
    case GLUT_KEY_LEFT:  cameraAzimuth   -= 3.0f; break;
    case GLUT_KEY_RIGHT: cameraAzimuth   += 3.0f; break;
    case GLUT_KEY_UP:    cameraElevation += 2.0f; break;
    case GLUT_KEY_DOWN:  cameraElevation -= 2.0f; break;
    default: return;
    }
    clampCamera();
    glutPostRedisplay();
}

// ----------------------------------------------------------------------------
// 14. Initialisation and main
// ----------------------------------------------------------------------------
static void initGL()
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    GLfloat globalAmb[] = {0.32f, 0.30f, 0.30f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmb);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat spec[] = {0.04f, 0.04f, 0.04f, 1.0f};     // matt stone, barely shiny
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 8.0f);

    gQuad = gluNewQuadric();
    gluQuadricNormals(gQuad, GLU_SMOOTH);
    resetCamera();
    clampCamera();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(1200, 800);
    glutInitWindowPosition(100, 50);
    glutCreateWindow("Ahsan Manzil - Fa3");
    printControls();
    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutMouseWheelFunc(mouseWheel);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);

    glutMainLoop();
    return 0;
}