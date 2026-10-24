// main.cpp -- 3D Solar System.
//
// A small OpenGL 3.3 project that shows our solar system in 3D:
//
//   * the sun in the middle, with a glowing corona
//   * eight planets moving along their orbits
//   * the planets spinning on their own axes, and leaning over at their
//     axial tilt
//   * Saturn's rings
//   * a field of background stars
//
// The whole thing is drawn with GLSL shaders. There are five shader
// programs:
//
//   planet.* -- the planets, lit by the sun, with procedural surface bands
//   sun.*    -- the sun itself and its corona
//   star.*   -- the background stars
//   ring.*   -- Saturn's rings, with the planet's shadow on them
//   line.*   -- the faint orbit lines
//
// Controls are listed in the console when the program starts.

#include "math3d.hpp"
#include "render.hpp"
#include "shader.hpp"
#include "solar_system.hpp"

// IMPORTANT: glew.h must be included BEFORE glfw3.h anywhere in the project.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// The state of the program.
// ---------------------------------------------------------------------------
struct App
{
    SolarSystem system;
    Renderer    renderer;
    Camera      camera;

    // --- The animation ---
    float time = 0.0f;            // how long the animation has been running
    float time_scale = 1.0f;      // how fast time passes (1 = normal)
    bool  paused = false;

    // --- The input ---
    bool  dragging = false;       // is the mouse button held down?
    double last_mouse_x = 0.0;
    double last_mouse_y = 0.0;

    // Which planet the camera is following, or -1 for the sun.
    int   focused_planet = -1;
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

    case GLFW_KEY_RIGHT:
        // Speed the planets up.
        app->time_scale *= 1.5f;
        std::cout << "  Speed: x" << app->time_scale << std::endl;
        break;

    case GLFW_KEY_LEFT:
        // Slow them down.
        app->time_scale /= 1.5f;
        if (app->time_scale < 0.01f) app->time_scale = 0.01f;
        std::cout << "  Speed: x" << app->time_scale << std::endl;
        break;

    case GLFW_KEY_F:
        // Focus on the next planet in turn.
    {
        const int count = static_cast<int>(app->system.planets().size());
        app->focused_planet = (app->focused_planet + 1) % (count + 1);

        if (app->focused_planet == count)
        {
            app->camera.target = math3d::Vec3(0.0f, 0.0f, 0.0f);
            std::cout << "  Camera: the sun" << std::endl;
        }
        else
        {
            const Planet& planet = app->system.planets()[app->focused_planet];
            app->camera.target = app->system.planet_position(planet);

            // Move the camera to a distance that frames this planet well.
            //
            // The planets are different sizes, so a fixed distance would
            // make Jupiter fill the screen and Mercury a speck. We use a
            // multiple of the planet's own radius instead.
            app->camera.distance = planet.radius * 6.0f;

            std::cout << "  Camera: " << planet.name << std::endl;
        }
        break;
    }

    case GLFW_KEY_1:
        app->camera.target = math3d::Vec3(0.0f, 0.0f, 0.0f);
        app->focused_planet = -1;
        std::cout << "  Camera: the sun" << std::endl;
        break;

    case GLFW_KEY_H:
        // Print the controls again.
        std::cout << "\n  Controls:\n"
                  << "    Left drag         rotate the camera\n"
                  << "    Right drag / wheel zoom in and out\n"
                  << "    SPACE             pause and un-pause\n"
                  << "    Left / Right      slow down / speed up time\n"
                  << "    F                 follow the next planet\n"
                  << "    1                 look at the sun\n"
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
    app->camera.yaw += static_cast<float>(delta_x) * 0.3f;

    // Dragging up and down raises and lowers it.
    app->camera.pitch -= static_cast<float>(delta_y) * 0.3f;

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
    app->camera.distance -= static_cast<float>(y_offset) * app->camera.distance * 0.10f;

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
              << "    Left / Right      slow down / speed up time\n"
              << "    F                 follow the next planet\n"
              << "    1                 look at the sun\n"
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

    GLFWwindow* window = glfwCreateWindow(1100, 750, "3D Solar System", nullptr, nullptr);
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

    // glewInit sometimes reports an error even though everything works, so
    // we check for a real problem by asking for the OpenGL version.
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

    int width = 1100;
    int height = 750;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    // --- Step 5: the main loop ---
    //
    // We keep track of the time between frames so the animation runs at the
    // same speed whatever the frame rate. If a frame takes longer (because
    // the computer is busy), the planets jump further, so they do not
    // appear to move in slow motion.
    auto previous_time = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window))
    {
        // --- Handle events ---
        //
        // We do this at the TOP of the loop, before drawing.
        glfwPollEvents();

        // --- How much time has passed? ---
        const auto now = std::chrono::steady_clock::now();
        const float delta_seconds = std::chrono::duration<float>(now - previous_time).count();
        previous_time = now;

        // A very long pause (for example when the program is in the
        // background) would make the planets jump. We limit the step so
        // that never happens.
        const float safe_delta = (delta_seconds > 0.1f) ? 0.1f : delta_seconds;

        // --- Advance the simulation ---
        if (!app.paused)
        {
            app.time += safe_delta * app.time_scale;
            app.system.update(safe_delta * app.time_scale);
        }

        // --- Follow the planet we are watching ---
        //
        // The camera's target follows the planet around its orbit, so the
        // planet stays in the middle of the screen.
        if (app.focused_planet >= 0)
        {
            const Planet& planet = app.system.planets()[app.focused_planet];
            app.camera.target = app.system.planet_position(planet);
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
