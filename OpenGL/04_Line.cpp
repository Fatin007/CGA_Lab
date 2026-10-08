#include <GL/glut.h>

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_LINES);

        glColor3f(1, 0, 0); // Red
        glVertex2f(-0.5f, 0.0f);
        glVertex2f(0.5f, 0.0f);

        glColor3f(0, 1, 0); // Green
        glVertex2f(0.0f, -0.5f);
        glVertex2f(0.0f, 0.5f);

    glEnd();

    glFlush();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

    glutInitWindowSize(700, 500);
    glutCreateWindow("Lines");

    glutDisplayFunc(display);

    glutMainLoop();

    return 0;
}