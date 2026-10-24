// _test_gl.cpp -- Smoke test: create a real OpenGL window, build the shader
// program, create the geometry, draw a few frames and pick a square.
//
// This checks the parts that the maths tests cannot: that the shaders
// compile on this machine, that the buffers are created, and that the
// drawing code runs without crashing.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "render.hpp"
#include "shader.hpp"
#include <iostream>

int main()
{
    if (!glfwInit()) { std::cerr << "glfwInit failed\n"; return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);   // no window on screen

    GLFWwindow* window = glfwCreateWindow(1000, 800, "test", nullptr, nullptr);
    if (!window) { std::cerr << "window creation failed\n"; glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "glewInit failed\n"; return 1; }
    glGetError();

    std::cout << "[GL] Version : " << glGetString(GL_VERSION) << "\n";
    std::cout << "[GL] Renderer: " << glGetString(GL_RENDERER) << "\n";

    // --- Load the shader program ---
    Shader shader;
    const std::string vert = find_asset_path("assets/shaders/basic.vert");
    const std::string frag = find_asset_path("assets/shaders/basic.frag");
    if (!shader.load_from_files(vert, frag))
    {
        std::cerr << "[FAIL] shader did not compile\n";
        return 1;
    }
    std::cout << "[PASS] shader program compiled and linked\n";

    // --- Create the scene geometry ---
    Renderer renderer;
    if (!renderer.initialise())
    {
        std::cerr << "[FAIL] renderer.initialise() failed\n";
        return 1;
    }
    std::cout << "[PASS] scene geometry created\n";

    // --- Draw a few frames ---
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.10f, 0.12f, 0.16f, 1.0f);

    ChessBoard board;
    board.reset();

    Camera camera;
    camera.yaw = 40.0f;
    camera.pitch = 55.0f;
    camera.distance = 11.0f;

    // Select the e2 pawn so the highlight and the move markers are drawn too.
    std::vector<Move> legal = board.legal_moves_from(1, 4);

    Move last;
    last.fromRow = 1; last.fromCol = 4;
    last.toRow = 3;   last.toCol = 4;

    for (int frame = 0; frame < 3; ++frame)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer.draw_scene(shader, board, camera, 1000, 800,
                            1, 4, legal, &last, false);
        glfwSwapBuffers(window);
    }

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cerr << "[FAIL] OpenGL error 0x" << std::hex << error << "\n";
        return 1;
    }
    std::cout << "[PASS] 3 frames drawn with no OpenGL errors\n";

    // --- Picking against the real matrices ---
    int row = -1, col = -1;
    const bool hit = renderer.pick_square(500, 400, 1000, 800, camera, false, row, col);
    std::cout << (hit ? "[PASS] pick_square at the window centre returned a square ("
                      : "[FAIL] pick_square at the window centre returned nothing (")
          << row << "," << col << ")\n";

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "\nAll GL smoke tests passed.\n";
    return 0;
}
