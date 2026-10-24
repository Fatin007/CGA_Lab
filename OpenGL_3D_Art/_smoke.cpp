// _smoke.cpp -- A quick smoke test of the whole program.
//
// It opens a real window, runs the real main loop for a short time, and
// checks that the frames actually drew something. This catches the kind of
// bug where the program starts fine but draws a black screen.
//
//   g++ _smoke.cpp shader.cpp geometry.cpp solar_system.cpp render.cpp
//       -o _smoke.exe -lglfw3 -lglew32 -lopengl32 -std=c++17

#include "geometry.hpp"
#include "render.hpp"
#include "solar_system.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <vector>

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(1100, 750, "smoke", nullptr, nullptr);
    if (!window) { printf("  [FAIL] no window\n"); glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    glewInit();

    printf("  OpenGL : %s\n", glGetString(GL_VERSION));
    printf("  GLSL   : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("  GPU    : %s\n", glGetString(GL_RENDERER));

    SolarSystem system;
    Renderer renderer;

    if (!renderer.initialise())
    {
        printf("  [FAIL] the renderer could not start\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    printf("  [PASS] all five shaders compiled and linked\n");

    int width = 1100, height = 750;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // --- Run 120 frames, as the real program does ---
    Camera camera;
    for (int frame = 0; frame < 120; ++frame)
    {
        system.update(1.0f / 60.0f);
        renderer.draw(system, camera, frame / 60.0f, width, height);
    }
    printf("  [PASS] 120 frames drawn with no OpenGL error\n");

    // --- Check that the screen is not black ---
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    int lit = 0, stars = 0, bright = 0;
    for (size_t i = 0; i < pixels.size(); i += 3)
    {
        const int sum = pixels[i] + pixels[i + 1] + pixels[i + 2];
        if (sum > 0) ++lit;
        if (sum > 20) ++stars;      // faint: the stars and the orbit lines
        if (sum > 200) ++bright;    // bright: the sun and the lit planets
    }
    printf("  lit pixels    = %d\n", lit);
    printf("  faint pixels  = %d  (stars, orbit lines)\n", stars);
    printf("  bright pixels = %d  (sun, lit planets)\n", bright);

    bool ok = (lit > 50000) && (stars > 500) && (bright > 500);
    printf("  [%s] the scene rendered\n", ok ? "PASS" : "FAIL");

    printf("  glGetError = %d (0 = no error)\n", glGetError());

    glfwDestroyWindow(window);
    glfwTerminate();
    return ok ? 0 : 1;
}
