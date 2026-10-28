
#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

const float PI = 3.14159265358979323846f;

float cameraYaw = 35.0f;
float cameraPitch = 22.0f;
float cameraDistance = 13.0f;

int lastMouseX = 0;
int lastMouseY = 0;
bool dragging = false;

int windowWidth = 1200;
int windowHeight = 800;

// --------------------------------------------------
// BASIC HELPERS
// --------------------------------------------------

void setColor(float r, float g, float b)
{
    glColor3f(r, g, b);
}

void drawCube(
    float x, float y, float z,
    float sx, float sy, float sz,
    float r, float g, float b)
{
    glPushMatrix();

    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);

    setColor(r, g, b);
    glutSolidCube(1.0);

    glPopMatrix();
}

void drawBox(
    float x, float y, float z,
    float sx, float sy, float sz,
    float r, float g, float b)
{
    drawCube(x, y, z, sx, sy, sz, r, g, b);
}

// Draw a slanted 3D body section using six faces.
void drawSlantedBody()
{
    // Cybertruck cabin, with a sloping front windshield
    // and a sloping rear roof.

    const float frontX = -1.05f;
    const float rearX  =  1.10f;

    const float bottomY = 0.95f;
    const float roofY   = 2.05f;

    const float frontTopX = -0.38f;
    const float rearTopX  =  0.45f;

    const float halfWidth = 0.88f;

    // Left and right side profiles
    for (int side = -1; side <= 1; side += 2)
    {
        glBegin(GL_POLYGON);
        glNormal3f(0, 0, (float)side);

        glVertex3f(frontX, bottomY, side * halfWidth);
        glVertex3f(rearX, bottomY, side * halfWidth);
        glVertex3f(rearTopX, roofY, side * halfWidth);
        glVertex3f(frontTopX, roofY, side * halfWidth);

        glEnd();
    }

    // Front sloped panel
    glBegin(GL_QUADS);
    glColor3f(0.58f, 0.61f, 0.64f);

    glVertex3f(frontX, bottomY, -halfWidth);
    glVertex3f(frontX, bottomY,  halfWidth);
    glVertex3f(frontTopX, roofY,  halfWidth);
    glVertex3f(frontTopX, roofY, -halfWidth);

    glEnd();

    // Rear sloped panel
    glBegin(GL_QUADS);
    glColor3f(0.43f, 0.46f, 0.49f);

    glVertex3f(rearX, bottomY, -halfWidth);
    glVertex3f(rearX, bottomY,  halfWidth);
    glVertex3f(rearTopX, roofY,  halfWidth);
    glVertex3f(rearTopX, roofY, -halfWidth);

    glEnd();

    // Roof
    glBegin(GL_QUADS);
    glColor3f(0.72f, 0.75f, 0.78f);

    glVertex3f(frontTopX, roofY, -halfWidth);
    glVertex3f(frontTopX, roofY,  halfWidth);
    glVertex3f(rearTopX, roofY,  halfWidth);
    glVertex3f(rearTopX, roofY, -halfWidth);

    glEnd();
}

// --------------------------------------------------
// CYBERTRUCK
// --------------------------------------------------

void drawWheel(float x, float z)
{
    glPushMatrix();

    glTranslatef(x, 0.53f, z);

    // Wheels are cylinders along the Z axis.
    // Rotate a standard GLUT cylinder to face sideways.
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);

    // Tire
    setColor(0.07f, 0.07f, 0.075f);
    glutSolidTorus(0.10, 0.34, 16, 32);

    // Hub
    setColor(0.58f, 0.60f, 0.62f);
    glutSolidTorus(0.045, 0.19, 12, 24);

    // Center cap
    setColor(0.23f, 0.24f, 0.25f);
    glutSolidSphere(0.13, 16, 16);

    glPopMatrix();
}

void drawCybertruck()
{
    // Main lower chassis
    drawBox(
        0.0f, 0.92f, 0.0f,
        4.25f, 0.48f, 1.65f,
        0.36f, 0.39f, 0.42f
    );

    // Main angular body
    drawBox(
        0.0f, 1.12f, 0.0f,
        3.90f, 0.50f, 1.75f,
        0.58f, 0.61f, 0.64f
    );

    // Front hood
    drawBox(
        -1.55f, 1.40f, 0.0f,
        0.95f, 0.22f, 1.70f,
        0.67f, 0.70f, 0.73f
    );

    // Angular cabin
    drawSlantedBody();

    // Dark front windshield
    glBegin(GL_QUADS);
    setColor(0.07f, 0.12f, 0.16f);

    glVertex3f(-1.00f, 1.48f, -0.77f);
    glVertex3f(-1.00f, 1.48f,  0.77f);
    glVertex3f(-0.42f, 2.00f,  0.77f);
    glVertex3f(-0.42f, 2.00f, -0.77f);

    glEnd();

    // Side windows
    for (int side = -1; side <= 1; side += 2)
    {
        glBegin(GL_QUADS);
        setColor(0.08f, 0.14f, 0.18f);

        glVertex3f(-0.77f, 1.55f, side * 0.895f);
        glVertex3f( 0.30f, 1.55f, side * 0.895f);
        glVertex3f( 0.27f, 1.91f, side * 0.895f);
        glVertex3f(-0.40f, 1.91f, side * 0.895f);

        glEnd();
    }

    // Front LED light bar
    drawBox(
        -2.00f, 1.32f, 0.0f,
        0.035f, 0.055f, 1.35f,
        0.92f, 0.97f, 1.00f
    );

    // Rear red light bar
    drawBox(
        1.98f, 1.30f, 0.0f,
        0.035f, 0.065f, 1.30f,
        0.85f, 0.06f, 0.04f
    );

    // Side trim strips
    for (int side = -1; side <= 1; side += 2)
    {
        drawBox(
            0.0f, 1.02f, side * 0.89f,
            3.80f, 0.055f, 0.035f,
            0.27f, 0.29f, 0.31f
        );
    }

    // Four wheels
    drawWheel(-1.35f, -0.90f);
    drawWheel( 1.35f, -0.90f);
    drawWheel(-1.35f,  0.90f);
    drawWheel( 1.35f,  0.90f);

    // Side mirrors
    drawBox(
        -0.75f, 1.52f, -1.02f,
        0.22f, 0.10f, 0.16f,
        0.16f, 0.17f, 0.18f
    );

    drawBox(
        -0.75f, 1.52f, 1.02f,
        0.22f, 0.10f, 0.16f,
        0.16f, 0.17f, 0.18f
    );
}

// --------------------------------------------------
// ENVIRONMENT
// --------------------------------------------------

void drawGround()
{
    // Green meadow
    glBegin(GL_QUADS);
    setColor(0.20f, 0.49f, 0.20f);

    glVertex3f(-100.0f, 0.0f, -100.0f);
    glVertex3f(-100.0f, 0.0f,  100.0f);
    glVertex3f( 100.0f, 0.0f,  100.0f);
    glVertex3f( 100.0f, 0.0f, -100.0f);

    glEnd();

    // Road
    drawBox(
        0.0f, 0.025f, -4.8f,
        100.0f, 0.05f, 4.0f,
        0.20f, 0.21f, 0.22f
    );

    // Road lane markings
    for (float x = -40.0f; x <= 40.0f; x += 5.0f)
    {
        drawBox(
            x, 0.06f, -4.8f,
            2.5f, 0.015f, 0.09f,
            0.93f, 0.91f, 0.72f
        );
    }

    // Small grass patches
    for (int i = 0; i < 100; i++)
    {
        float x = (float)((i * 37) % 80) - 40.0f;
        float z = (float)((i * 19) % 25) - 15.0f;

        if (std::fabs(z + 4.8f) < 2.5f)
            continue;

        drawBox(
            x, 0.035f, z,
            0.07f, 0.06f, 0.07f,
            0.12f, 0.35f, 0.12f
        );
    }
}

void drawMountain(float x, float z, float height, float radius)
{
    glPushMatrix();

    glTranslatef(x, 0.0f, z);

    setColor(0.29f, 0.39f, 0.31f);
    glutSolidCone(radius, height, 8, 4);

    glPopMatrix();
}

void drawEnvironment()
{
    // Mountains in the distance
    drawMountain(-14.0f, -22.0f, 8.0f, 7.0f);
    drawMountain(-4.0f, -25.0f, 10.0f, 8.0f);
    drawMountain(8.0f, -23.0f, 7.0f, 6.0f);
    drawMountain(19.0f, -28.0f, 11.0f, 9.0f);

    // Simple trees on the meadow
    for (int i = 0; i < 12; i++)
    {
        float x = -15.0f + i * 2.7f;
        float z = -10.0f - (i % 3) * 1.8f;

        drawBox(
            x, 0.55f, z,
            0.20f, 1.10f, 0.20f,
            0.32f, 0.19f, 0.09f
        );

        glPushMatrix();
        glTranslatef(x, 1.55f, z);
        setColor(0.10f, 0.32f, 0.12f);
        glutSolidSphere(0.65, 12, 12);
        glPopMatrix();
    }
}

// --------------------------------------------------
// SKY
// --------------------------------------------------

void drawSky()
{
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);

    glColor3f(0.35f, 0.67f, 0.91f);
    glVertex2i(0, windowHeight);
    glVertex2i(windowWidth, windowHeight);

    glColor3f(0.78f, 0.89f, 0.96f);
    glVertex2i(windowWidth, windowHeight / 3);
    glVertex2i(0, windowHeight / 3);

    glEnd();

    // Sun
    glColor3f(1.0f, 0.91f, 0.53f);

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(windowWidth * 0.82f, windowHeight * 0.80f);

    for (int i = 0; i <= 60; i++)
    {
        float angle = 2.0f * PI * i / 60.0f;

        glVertex2f(
            windowWidth * 0.82f + std::cos(angle) * 42.0f,
            windowHeight * 0.80f + std::sin(angle) * 42.0f
        );
    }

    glEnd();

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

// --------------------------------------------------
// CAMERA AND RENDERING
// --------------------------------------------------

void setupCamera()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(
        45.0,
        (double)windowWidth / (double)std::max(1, windowHeight),
        0.1,
        200.0
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float yawRad = cameraYaw * PI / 180.0f;
    float pitchRad = cameraPitch * PI / 180.0f;

    float camX = cameraDistance *
                 std::cos(pitchRad) * std::sin(yawRad);

    float camY = cameraDistance * std::sin(pitchRad) + 1.0f;

    float camZ = cameraDistance *
                 std::cos(pitchRad) * std::cos(yawRad);

    gluLookAt(
        camX, camY, camZ,
        0.0, 1.0, 0.0,
        0.0, 1.0, 0.0
    );
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw the sky first
    drawSky();

    setupCamera();

    // Lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat lightPosition[] = { -5.0f, 10.0f, 8.0f, 1.0f };
    GLfloat ambientLight[] = { 0.45f, 0.45f, 0.45f, 1.0f };
    GLfloat diffuseLight[] = { 0.85f, 0.85f, 0.85f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);

    glEnable(GL_NORMALIZE);

    drawGround();
    drawEnvironment();
    drawCybertruck();

    glDisable(GL_LIGHTING);

    glutSwapBuffers();
}

// --------------------------------------------------
// INPUT
// --------------------------------------------------

void reshape(int width, int height)
{
    windowWidth = std::max(1, width);
    windowHeight = std::max(1, height);

    glViewport(0, 0, windowWidth, windowHeight);

    glutPostRedisplay();
}

void mouseButton(int button, int state, int x, int y)
{
    if (button == GLUT_LEFT_BUTTON)
    {
        dragging = (state == GLUT_DOWN);

        lastMouseX = x;
        lastMouseY = y;
    }

    // Mouse wheel zoom
    if (button == 3 && state == GLUT_DOWN)
    {
        cameraDistance -= 0.6f;
        cameraDistance = std::max(5.0f, cameraDistance);
    }

    if (button == 4 && state == GLUT_DOWN)
    {
        cameraDistance += 0.6f;
        cameraDistance = std::min(35.0f, cameraDistance);
    }

    glutPostRedisplay();
}

void mouseMotion(int x, int y)
{
    if (dragging)
    {
        cameraYaw += (x - lastMouseX) * 0.5f;
        cameraPitch += (lastMouseY - y) * 0.5f;

        cameraPitch = std::max(-5.0f, cameraPitch);
        cameraPitch = std::min(80.0f, cameraPitch);

        lastMouseX = x;
        lastMouseY = y;

        glutPostRedisplay();
    }
}

void keyboard(unsigned char key, int, int)
{
    switch (key)
    {
        case 27: // ESC
            std::exit(0);
            break;

        case '+':
        case '=':
            cameraDistance -= 0.7f;
            break;

        case '-':
        case '_':
            cameraDistance += 0.7f;
            break;

        case 'a':
        case 'A':
            cameraYaw -= 5.0f;
            break;

        case 'd':
        case 'D':
            cameraYaw += 5.0f;
            break;

        case 'w':
        case 'W':
            cameraPitch += 3.0f;
            break;

        case 's':
        case 'S':
            cameraPitch -= 3.0f;
            break;

        case 'r':
        case 'R':
            cameraYaw = 35.0f;
            cameraPitch = 22.0f;
            cameraDistance = 13.0f;
            break;
    }

    cameraDistance = std::max(5.0f, cameraDistance);
    cameraDistance = std::min(35.0f, cameraDistance);

    cameraPitch = std::max(-5.0f, cameraPitch);
    cameraPitch = std::min(80.0f, cameraPitch);

    glutPostRedisplay();
}

// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);

    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 50);

    glutCreateWindow("Cybertruck 3D - OpenGL");

    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    glClearColor(0.55f, 0.75f, 0.95f, 1.0f);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouseButton);
    glutMotionFunc(mouseMotion);
    glutKeyboardFunc(keyboard);

    glutMainLoop();

    return 0;
}
