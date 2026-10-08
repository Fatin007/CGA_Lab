#include <GL/glut.h>

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1.0f, 0.0f, 0.0f); // red
    // glColor3f(0, 1, 0); // Green
    // glColor3f(0, 0, 1); // Blue
    // glColor3f(1, 1, 0); // Yellow
    // glColor3f(1, 1, 1); // White
    // glColor3f(0, 0, 0); // Black

    glBegin(GL_TRIANGLES);

        glColor3f(1, 0, 0); // Red
        glVertex2f(0.0f, 0.5f);

        glColor3f(0, 1, 0); // Green
        glVertex2f(-0.5f, -0.5f);

        glColor3f(0, 0, 1); // Blue
        glVertex2f(0.5f, -0.5f);

    glEnd();

    glFlush();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

    glutInitWindowSize(700, 500);
    glutCreateWindow("Triangle");

    glutDisplayFunc(display);

    glutMainLoop();

    return 0;
}