#include <GL/glut.h>

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_QUADS);

        glColor3f(1, 0, 0); // Red
        glVertex2f(-0.5f, 0.5f);
        glColor3f(0, 1, 0); // Green
        glVertex2f(0.5f, 0.5f);
        glColor3f(0, 1, 0); // Green
        glVertex2f(0.5f, -0.5f);
        glColor3f(1, 0, 0); // Red
        glVertex2f(-0.5f, -0.5f);

    glEnd();

    glFlush();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

    glutInitWindowSize(700, 500);
    glutCreateWindow("Quads");

    glutDisplayFunc(display);

    glutMainLoop();

    return 0;
}