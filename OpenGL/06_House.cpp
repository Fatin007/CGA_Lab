#include <GL/glut.h>

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(1, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(-0.6f,  0.1f);
        glVertex2f( 0.6f,  0.1f);
        glVertex2f( 0.6f, -0.6f);
        glVertex2f(-0.6f, -0.6f);
    glEnd();

    glColor3f(0, 0, 1);
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

    glColor3f(1, 1, 0); // Yellow
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

    glColor3f(1, 1, 0); // Yellow
    glBegin(GL_QUADS);
        glVertex2f(-0.5f, -0.1f);
        glVertex2f(-0.3f, -0.1f);
        glVertex2f(-0.3f, -0.3f);
        glVertex2f(-0.5f, -0.3f);
    glEnd();
    
    // window 2
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(0.51f, -0.09f);
        glVertex2f(0.29f, -0.09f);
        glVertex2f(0.29f, -0.31f);
        glVertex2f(0.51f, -0.31f);
    glEnd();

    glColor3f(1, 1, 0); // Yellow
    glBegin(GL_QUADS);
        glVertex2f(0.5f, -0.1f);
        glVertex2f(0.3f, -0.1f);
        glVertex2f(0.3f, -0.3f);
        glVertex2f(0.5f, -0.3f);
    glEnd();

    glFlush();
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

    glutInitWindowSize(700, 500);
    glutCreateWindow("House");

    glutDisplayFunc(display);

    glutMainLoop();

    return 0;
}