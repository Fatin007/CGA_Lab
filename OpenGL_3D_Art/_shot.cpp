// _shot.cpp -- Take a screenshot of the solar system.
//
// This is a test program, not part of the main application. It builds the
// real scene, draws a few frames and saves the result to a PNG-less PPM
// file, so we can check that the scene actually looks right.
//
// Build it with:
//   g++ _shot.cpp shader.cpp geometry.cpp solar_system.cpp render.cpp
//       -o _shot.exe -lglfw3 -lglew32 -lopengl32 -std=c++17
//
// Run it with:
//   ./_shot.exe
//
// It writes "shot.ppm" into the current folder.

#include "render.hpp"
#include "solar_system.hpp"

// IMPORTANT: glew.h before glfw3.h.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cmath>
#include <vector>

// Writes the colour buffer to a PPM file (the simplest image format there
// is -- it is just a header followed by raw RGB bytes).
static bool save_screenshot(GLFWwindow* window, const char* filename)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) return false;

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    FILE* file = fopen(filename, "wb");
    if (!file)
    {
        printf("  Could not open %s for writing.\n", filename);
        return false;
    }

    fprintf(file, "P6\n%d %d\n255\n", width, height);

    // OpenGL's origin is at the bottom-left, but image files put it at the
    // top-left, so we write the rows in reverse order.
    for (int row = height - 1; row >= 0; --row)
    {
        const unsigned char* line = pixels.data() + static_cast<size_t>(row) * width * 3;
        fwrite(line, 1, static_cast<size_t>(width) * 3, file);
    }

    fclose(file);

    printf("  Saved %s (%d x %d)\n", filename, width, height);
    return true;
}

// Counts how many pixels are not the background colour. If the scene is
// drawing, there should be plenty.
static int count_bright_pixels(GLFWwindow* window)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) return 0;

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    int bright = 0;
    for (size_t i = 0; i < pixels.size(); i += 3)
    {
        if (pixels[i] + pixels[i + 1] + pixels[i + 2] > 40) ++bright;
    }
    return bright;
}

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);   // hidden, but still renders

    GLFWwindow* window = glfwCreateWindow(1100, 750, "Screenshot", nullptr, nullptr);
    if (!window) { printf("  Could not create the window.\n"); glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    glewInit();

    SolarSystem system;
    Renderer renderer;

    if (!renderer.initialise())
    {
        printf("  [FAIL] the renderer could not start.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    printf("  [PASS] the renderer started and all five shaders compiled.\n");

    int width = 1100, height = 750;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // --- Draw a wide shot of the whole system ---
    {
        Camera camera;
        camera.distance = 120.0f;
        camera.yaw      = 35.0f;
        camera.pitch    = 28.0f;

        // Let the planets move a little so they are not all bunched up.
        for (int i = 0; i < 120; ++i) system.update(1.0f / 60.0f);

        renderer.draw(system, camera, 5.0f, width, height);

        const int bright = count_bright_pixels(window);
        printf("  [%s] %d lit pixels in the wide shot\n",
               bright > 5000 ? "PASS" : "FAIL", bright);
    }

    save_screenshot(window, "shot_wide.ppm");

    // --- Draw a shot of the inner solar system ---
    {
        Camera camera;
        camera.distance = 26.0f;
        camera.yaw      = 35.0f;
        camera.pitch    = 30.0f;

        renderer.draw(system, camera, 5.0f, width, height);
    }

    save_screenshot(window, "shot_inner.ppm");

    // --- Draw a close-up of Saturn, so we can check the rings ---
    {
        Camera camera;

        // Find Saturn and look at it.
        int saturn = 0;
        for (int i = 0; i < static_cast<int>(system.planets().size()); ++i)
            if (system.planets()[i].name == "Saturn") saturn = i;

        const Planet& planet = system.planets()[saturn];
        camera.target = system.planet_position(planet);
        camera.distance = planet.radius * 7.0f;
        camera.yaw = 20.0f;
        camera.pitch = 22.0f;

        renderer.draw(system, camera, 5.0f, width, height);
    }

    save_screenshot(window, "shot_saturn.ppm");

    // --- Draw a close-up of Earth ---
    {
        Camera camera;

        int earth = 0;
        for (int i = 0; i < static_cast<int>(system.planets().size()); ++i)
            if (system.planets()[i].name == "Earth") earth = i;

        const Planet& planet = system.planets()[earth];
        camera.target = system.planet_position(planet);
        camera.distance = planet.radius * 5.0f;
        camera.yaw = 60.0f;
        camera.pitch = 15.0f;

        renderer.draw(system, camera, 5.0f, width, height);
    }

    save_screenshot(window, "shot_earth.ppm");

    glfwDestroyWindow(window);
    glfwTerminate();

    printf("  All screenshot tests finished.\n");
    return 0;
}
