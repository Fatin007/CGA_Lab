// g++ 06_House.cpp -lfreeglut -lopengl32 -lglu32

#include <GL/glut.h>

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(-0.61f,  0.1f);
        glVertex2f( 0.61f,  0.1f);
        glVertex2f( 0.61f, -0.61f);
        glVertex2f(-0.61f, -0.61f);
    glEnd();

    glColor3f(1, 1, 0); // Yellow
    glBegin(GL_QUADS);
        glVertex2f(-0.6f,  0.1f);
        glVertex2f( 0.6f,  0.1f);
        glVertex2f( 0.6f, -0.6f);
        glVertex2f(-0.6f, -0.6f);
    glEnd();

    glColor3f(0, 0, 0);
    glBegin(GL_TRIANGLES);
        glVertex2f(-0.72f,  0.09f);
        glVertex2f( 0.72f,  0.09f);
        glVertex2f( 0.00f,  0.61f);
    glEnd();

    glColor3f(1, 0, 0);
    glBegin(GL_TRIANGLES);
        glVertex2f(-0.7f,  0.1f);
        glVertex2f( 0.7f,  0.1f);
        glVertex2f( 0.0f,  0.6f);
    glEnd();
   
    glColor3f(0, 0, 0); // Yellow
    glBegin(GL_QUADS);
        glVertex2f(-0.11f, -0.09f);
        glVertex2f( 0.11f, -0.09f);
        glVertex2f( 0.11f, -0.6f);
        glVertex2f(-0.11f, -0.6f);
    glEnd();

    // Brown door
    glColor3f(0.5f, 0.2f, 0.0f); // Brown
    glBegin(GL_QUADS);
        glVertex2f(-0.1f,  -0.1f);
        glVertex2f( 0.1f,  -0.1f);
        glVertex2f( 0.1f, -0.6f);
        glVertex2f(-0.1f, -0.6f);
    glEnd();

    // window 1
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(-0.51f, -0.09f);
        glVertex2f(-0.29f, -0.09f);
        glVertex2f(-0.29f, -0.31f);
        glVertex2f(-0.51f, -0.31f);
    glEnd();

    glColor3f(0.5f, 0.8f, 1.0f); // skyblue
    glBegin(GL_QUADS);
        glVertex2f(-0.5f, -0.1f);
        glVertex2f(-0.3f, -0.1f);
        glVertex2f(-0.3f, -0.3f);
        glVertex2f(-0.5f, -0.3f);
    glEnd();

    glColor3f(0, 0, 0);
    glBegin(GL_LINES);
        glVertex2f(-0.4f, -0.1f);
        glVertex2f(-0.4f, -0.3f);

        glVertex2f(-0.5f, -0.2f);
        glVertex2f(-0.3f, -0.2f);
    glEnd();

    // window 2
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(0.51f, -0.09f);
        glVertex2f(0.29f, -0.09f);
        glVertex2f(0.29f, -0.31f);
        glVertex2f(0.51f, -0.31f);
    glEnd();

    glColor3f(0.5f, 0.8f, 1.0f); // skyblue
    glBegin(GL_QUADS);
        glVertex2f(0.5f, -0.1f);
        glVertex2f(0.3f, -0.1f);
        glVertex2f(0.3f, -0.3f);
        glVertex2f(0.5f, -0.3f);
    glEnd();

    glColor3f(0, 0, 0);
    glBegin(GL_LINES);
        glVertex2f(0.4f, -0.1f);
        glVertex2f(0.4f, -0.3f);

        glVertex2f(0.5f, -0.2f);
        glVertex2f(0.3f, -0.2f);
    glEnd();

    glFlush();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

    glutInitWindowSize(700, 500);
    glutCreateWindow("House");

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glutDisplayFunc(display);

    glutMainLoop();

    return 0;
}