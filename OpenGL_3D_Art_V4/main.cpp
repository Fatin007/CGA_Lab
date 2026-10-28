// main.cpp -- 3D Solar System.
//
// An OpenGL 3.3 project that shows our solar system in 3D:
//
//   * the sun in the middle, with a glowing corona
//   * eight planets moving along their orbits
//   * the planets spinning on their own axes, and leaning over at their
//     axial tilt
//   * Saturn's and Uranus's rings
//   * a field of background stars
//   * the asteroid belt between Mars and Jupiter
//
// The whole thing is drawn with GLSL shaders. There are five shader
// programs:
//
//   planet.* -- the planets, lit by the sun, with procedural surface bands
//   sun.*    -- the sun itself and its corona
//   ring.*   -- the planetary rings, with the planet's shadow on them
//   line.*   -- the faint orbit lines
//   point.*  -- every dot in the scene: the stars and the asteroids
//
// The camera can only be ORBITED and ZOOMED. There is no planet-following
// and no speed control -- the animation always runs at the same speed.

#include "math3d.hpp"
#include "render.hpp"
#include "shader.hpp"
#include "solar_system.hpp"

// IMPORTANT: glew.h must be included BEFORE glfw3.h anywhere in the project.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <iostream>
#include <string>

namespace
{
    const int   WINDOW_WIDTH  = 1100;
    const int   WINDOW_HEIGHT = 750;

    // How much the camera moves per pixel of mouse drag.
    constexpr float DRAG_SPEED = 0.3f;

    // How much the camera zooms per notch of the mouse wheel.
    constexpr float ZOOM_SPEED = 0.10f;

    // The longest step the simulation is allowed to take in one frame.
    //
    // If the program is paused in the background, or the computer stalls,
    // the gap between two frames can be huge. Taking that gap as one step
    // would teleport the planets across the screen, so we cap it.
    constexpr float MAX_STEP = 0.1f;
}

// ---------------------------------------------------------------------------
// The state of the program.
// ---------------------------------------------------------------------------
struct App
{
    SolarSystem system;
    Renderer    renderer;
    Camera      camera;

    // --- The animation ---
    float time = 0.0f;       // how long the animation has been running
    bool  paused = false;

    // --- The input ---
    bool   dragging = false;      // is the mouse button held down?
    double last_mouse_x = 0.0;
    double last_mouse_y = 0.0;
};

// ---------------------------------------------------------------------------
// Clamping a value into a range.
// ---------------------------------------------------------------------------
static float clamp(float value, float low, float high)
{
    if (value < low)  return low;
    if (value > high) return high;
    return value;
}

// ---------------------------------------------------------------------------
// The keyboard.
//
// There is very little to do here: the camera is moved with the mouse, and
// the only keys we need are "pause", "restart" and "quit".
// ---------------------------------------------------------------------------
static void on_key(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    switch (key)
    {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        break;

    case GLFW_KEY_SPACE:
        // Pause and un-pause.
        app->paused = !app->paused;
        std::cout << "  " << (app->paused ? "Paused." : "Running.") << std::endl;
        break;

    case GLFW_KEY_R:
        // Restart the animation.
        app->time = 0.0f;
        app->system = SolarSystem();
        std::cout << "  Restarted." << std::endl;
        break;

    case GLFW_KEY_H:
        // Print the controls again.
        std::cout << "\n  Controls:\n"
                  << "    Left drag         rotate the camera\n"
                  << "    Right drag / wheel zoom in and out\n"
                  << "    SPACE             pause and un-pause\n"
                  << "    R                 restart the animation\n"
                  << "    H                 print this help\n"
                  << "    ESC               quit\n" << std::endl;
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// The mouse.
//
// Dragging moves the camera around the sun, and the wheel moves it closer
// or further away. That is all the camera control there is.
// ---------------------------------------------------------------------------
static void on_mouse_button(GLFWwindow* window, int button, int action, int /*mods*/)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            app->dragging = true;
            glfwGetCursorPos(window, &app->last_mouse_x, &app->last_mouse_y);
        }
        else if (action == GLFW_RELEASE)
        {
            app->dragging = false;
        }
    }
}

static void on_cursor_motion(GLFWwindow* window, double x, double y)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    if (!app->dragging) return;

    // How far the mouse has moved since the last event.
    const double delta_x = x - app->last_mouse_x;
    const double delta_y = y - app->last_mouse_y;

    app->last_mouse_x = x;
    app->last_mouse_y = y;

    // Dragging sideways turns the camera around the sun.
    app->camera.yaw += static_cast<float>(delta_x) * DRAG_SPEED;

    // Dragging up and down raises and lowers it.
    app->camera.pitch -= static_cast<float>(delta_y) * DRAG_SPEED;

    // Keep the pitch inside sensible limits, so the camera never flips over
    // the top or the bottom.
    app->camera.pitch = clamp(app->camera.pitch, Camera::MIN_PITCH, Camera::MAX_PITCH);
}

static void on_scroll(GLFWwindow* window, double x_offset, double y_offset)
{
    App* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    (void)x_offset;

    // Scrolling up (a positive offset) moves us closer.
    app->camera.distance -= static_cast<float>(y_offset) * app->camera.distance * ZOOM_SPEED;

    app->camera.distance = clamp(app->camera.distance,
                                  Camera::MIN_DISTANCE, Camera::MAX_DISTANCE);
}

// ---------------------------------------------------------------------------
// The window size changed.
// ---------------------------------------------------------------------------
static void on_framebuffer_size(GLFWwindow* /*window*/, int width, int height)
{
    if (width > 0 && height > 0)
        glViewport(0, 0, width, height);
}

// ---------------------------------------------------------------------------
// The main program.
// ---------------------------------------------------------------------------
int main()
{
    std::cout << "\n"
              << "==========================================\n"
              << "        3D  S O L A R   S Y S T E M\n"
              << "      An OpenGL 3.3 graphics project\n"
              << "==========================================\n\n"
              << "  Controls:\n"
              << "    Left drag         rotate the camera\n"
              << "    Right drag / wheel zoom in and out\n"
              << "    SPACE             pause and un-pause\n"
              << "    R                 restart the animation\n"
              << "    H                 print this help\n"
              << "    ESC               quit\n\n";

    // --- Step 1: start GLFW ---
    if (!glfwInit())
    {
        std::cerr << "[Main] ERROR: could not start GLFW." << std::endl;
        return 1;
    }

    // We ask for OpenGL 3.3 in the core profile.
    //
    // "Core profile" means we get the modern OpenGL, without the old
    // immediate mode commands like glBegin and glEnd. Those were removed in
    // OpenGL 3.1 and do not work here.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                          "3D Solar System", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "[Main] ERROR: could not create the window." << std::endl;
        glfwTerminate();
        return 1;
    }

    // The context has to be current before we can call any OpenGL function.
    glfwMakeContextCurrent(window);

    // --- Step 2: load the OpenGL functions ---
    //
    // GLEW finds the addresses of the OpenGL functions at run time. It has
    // to be told to use its experimental features first, or it refuses to
    // load anything on some drivers.
    glewExperimental = GL_TRUE;
    const GLenum glew_status = glewInit();

    if (glew_status != GLEW_OK)
    {
        std::cerr << "[Main] ERROR: GLEW failed: "
                  << glewGetErrorString(glew_status) << std::endl;
        glfwTerminate();
        return 1;
    }

    std::cout << "[OpenGL] Version : " << glGetString(GL_VERSION) << "\n";
    std::cout << "[OpenGL] GLSL    : " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
    std::cout << "[OpenGL] Renderer: " << glGetString(GL_RENDERER) << "\n\n";

    // --- Step 3: set up the app and the renderer ---
    App app;

    glfwSetWindowUserPointer(window, &app);

    if (!app.renderer.initialise())
    {
        std::cerr << "[Main] ERROR: the renderer could not start." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // --- Step 4: the callbacks ---
    glfwSetKeyCallback(window, on_key);
    glfwSetMouseButtonCallback(window, on_mouse_button);
    glfwSetCursorPosCallback(window, on_cursor_motion);
    glfwSetScrollCallback(window, on_scroll);
    glfwSetFramebufferSizeCallback(window, on_framebuffer_size);

    int width = WINDOW_WIDTH;
    int height = WINDOW_HEIGHT;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // --- Step 5: the main loop ---
    //
    // We remember when the last frame started, so that we know how much
    // time has passed since then.
    double previous_time = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        // --- Handle events ---
        //
        // We do this at the TOP of the loop, before drawing.
        glfwPollEvents();

        // --- How much time has passed? ---
        //
        // glfwGetTime() gives us the number of seconds since the program
        // started. The difference between now and the last frame is the
        // length of this frame, in seconds.
        //
        // This is what makes the animation run at the SAME speed on every
        // computer. If we simply advanced by a fixed amount every frame,
        // then a fast computer (which draws 1000 frames a second) would
        // make the planets fly, while a slow one would make them crawl.
        // Measuring the real time means the animation always takes the
        // same number of seconds to play, whatever the frame rate.
        const double now = glfwGetTime();
        float delta_seconds = static_cast<float>(now - previous_time);
        previous_time = now;

        // A very long pause (for example when the program is in the
        // background) would make the planets jump. We limit the step so
        // that never happens.
        if (delta_seconds > MAX_STEP) delta_seconds = MAX_STEP;

        // --- Advance the simulation ---
        if (!app.paused)
        {
            app.time += delta_seconds;
            app.system.update(delta_seconds);
        }

        // --- Draw ---
        glfwGetFramebufferSize(window, &width, &height);
        app.renderer.draw(app.system, app.camera, app.time, width, height);

        // --- Show it ---
        //
        // We draw into a back buffer and then swap it to the screen. That
        // is what stops the animation from flickering.
        glfwSwapBuffers(window);
    }

    // --- Step 6: tidy up ---
    //
    // The renderer's destructor deletes the OpenGL objects, so we only have
    // to destroy the window and stop GLFW.
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "\n  Goodbye!\n\n";
    return 0;
}
