// main.cpp -- Entry point for the 3D Chess project.
//
// The program is split into modules so that each part stays easy to explain:
//
//   math3d.hpp      -- vector and matrix maths we wrote ourselves
//   shader.*        -- GLSL loading / compiling / linking
//   geometry.*      -- procedural 3D shapes (board squares and chess pieces)
//   board.*         -- chess rules engine (no OpenGL code at all)
//   ai.*            -- computer opponent (minimax + alpha-beta, on a thread)
//   render.*        -- camera, drawing and mouse picking
//   main.cpp        -- this file: menus, game state and the main loop

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "math3d.hpp"
#include "shader.hpp"
#include "geometry.hpp"
#include "board.hpp"
#include "ai.hpp"
#include "render.hpp"

#include <cstdio>
#include <cmath>
#include <string>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
// Window configuration.
// ---------------------------------------------------------------------------
static const int   WINDOW_WIDTH  = 1000;
static const int   WINDOW_HEIGHT = 800;
static const char* WINDOW_TITLE  = "3D Chess - OpenGL";

// ---------------------------------------------------------------------------
// How the game is being played.
// ---------------------------------------------------------------------------
enum GameMode
{
    MODE_VS_COMPUTER,
    MODE_VS_FRIEND
};

// What the game is currently doing.
enum GameState
{
    STATE_PLAYING,
    STATE_GAME_OVER
};

// ---------------------------------------------------------------------------
// The whole game state lives in one struct. There is no class hierarchy and
// no entity system, which keeps the program easy to follow.
// ---------------------------------------------------------------------------
struct Game
{
    ChessBoard board;
    Camera     camera;
    Renderer   renderer;
    ChessAI    ai;

    GameMode    mode        = MODE_VS_FRIEND;
    Difficulty  difficulty  = MEDIUM;
    Color       human_colour = WHITE;    // which side the player controls
    GameState   state       = STATE_PLAYING;

    // The square the player has clicked on, or (-1, -1) if nothing is selected.
    int selected_row = -1;
    int selected_col = -1;

    // The legal moves of the selected piece, so we can draw the markers.
    std::vector<Move> legal_moves;

    // The move that was just played, used to tint those two squares.
    Move last_move;

    // True when the board is shown from Black's point of view.
    bool flipped = false;

    // True while the computer is thinking.
    bool computer_thinking = false;

    // The result of the finished game.
    std::string result_text;
};

// ---------------------------------------------------------------------------
// Console menu.
//
// We ask for the settings in the console before the window opens. This keeps
// the graphics code free of menus and buttons, and it means the menu can be
// explained in one sentence.
// ---------------------------------------------------------------------------
static void print_main_menu()
{
    std::cout << "\n";
    std::cout << "==========================================\n";
    std::cout << "             3D  C H E S S\n";
    std::cout << "        An OpenGL 3.3 graphics project\n";
    std::cout << "==========================================\n";
    std::cout << "\n";
    std::cout << "  1. Play against the Computer\n";
    std::cout << "  2. Play against a Friend (same computer)\n";
    std::cout << "  3. Run the chess rules self-test\n";
    std::cout << "  4. Quit\n";
    std::cout << "\n";
    std::cout << "  Choose an option (1-4): ";
}

static int read_int(int fallback)
{
    int value = fallback;
    if (!(std::cin >> value))
    {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return fallback;
    }
    return value;
}

// Asks for the mode and the difficulty, and fills in the game.
static void configure_game(Game& game)
{
    bool finished = false;

    while (!finished)
    {
        print_main_menu();
        int choice = read_int(4);

        if (choice == 1)
        {
            game.mode = MODE_VS_COMPUTER;

            std::cout << "\n  How hard should the computer play?\n";
            std::cout << "    1. Easy   (looks 1 move ahead)\n";
            std::cout << "    2. Medium (looks 3 moves ahead)\n";
            std::cout << "    3. Hard   (looks 4 moves ahead)\n";
            std::cout << "\n  Choose (1-3): ";
            int level = read_int(2);
            if      (level == 1) game.difficulty = EASY;
            else if (level == 3) game.difficulty = HARD;
            else                 game.difficulty = MEDIUM;

            std::cout << "\n  Which side do you want to play?\n";
            std::cout << "    1. White\n";
            std::cout << "    2. Black\n";
            std::cout << "\n  Choose (1-2): ";
            int side = read_int(1);
            game.human_colour = (side == 2) ? BLACK : WHITE;

            std::cout << "\n  Starting a game against the computer ("
                      << difficulty_name(game.difficulty) << ").\n";
            finished = true;
        }
        else if (choice == 2)
        {
            game.mode = MODE_VS_FRIEND;
            std::cout << "\n  Starting a game against a friend.\n";
            finished = true;
        }
        else if (choice == 3)
        {
            std::cout << "\n";
            run_rules_self_test();
        }
        else
        {
            std::cout << "\n  Goodbye!\n";
            std::exit(0);
        }
    }
}

// ---------------------------------------------------------------------------
// Starts a fresh game.
// ---------------------------------------------------------------------------
static void restart_game(Game& game)
{
    game.board.reset();
    game.selected_row = -1;
    game.selected_col = -1;
    game.legal_moves.clear();
    game.last_move = Move();
    game.state = STATE_PLAYING;
    game.result_text.clear();
    game.computer_thinking = false;

    // When the player is Black we show the board from Black's side.
    game.flipped = (game.mode == MODE_VS_COMPUTER && game.human_colour == BLACK);

    game.ai.cancel();
    game.ai.wait_for_completion();
}

// ---------------------------------------------------------------------------
// Reports the end of the game and sets the state.
// ---------------------------------------------------------------------------
static void finish_game(Game& game)
{
    const Color winner_colour = (game.board.side_to_move() == WHITE) ? BLACK : WHITE;

    if (game.board.is_checkmate())
    {
        const char* winner = (winner_colour == WHITE) ? "White" : "Black";
        game.result_text = std::string("Checkmate! ") + winner + " wins.";

        if (game.mode == MODE_VS_COMPUTER)
        {
            if (winner_colour == game.human_colour)
                game.result_text += "  You won!";
            else
                game.result_text += "  The computer won.";
        }
    }
    else if (game.board.is_stalemate())
    {
        game.result_text = "Stalemate! The game is a draw.";
    }
    else if (game.board.is_fifty_move_draw())
    {
        game.result_text = "Draw by the 50-move rule.";
    }
    else if (game.board.is_threefold_repetition())
    {
        game.result_text = "Draw by threefold repetition.";
    }
    else
    {
        game.result_text = "Game over.";
    }

    game.state = STATE_GAME_OVER;

    std::cout << "\n  " << game.result_text << "\n";
    std::cout << "  Press R in the window to play again.\n";
}

// ---------------------------------------------------------------------------
// Checks whether the game has ended and reports it if so.
// ---------------------------------------------------------------------------
static void check_game_over(Game& game)
{
    if (game.state != STATE_PLAYING) return;

    if (game.board.is_checkmate() ||
        game.board.is_stalemate() ||
        game.board.is_fifty_move_draw() ||
        game.board.is_threefold_repetition())
    {
        finish_game(game);
    }
}

// ---------------------------------------------------------------------------
// Plays a move on the board and updates the selection.
// ---------------------------------------------------------------------------
static void play_move(Game& game, const Move& move)
{
    if (!game.board.make_move(move)) return;

    game.last_move = move;
    game.selected_row = -1;
    game.selected_col = -1;
    game.legal_moves.clear();

    std::cout << "  " << ChessBoard::square_name(move.fromRow, move.fromCol)
              << " -> " << ChessBoard::square_name(move.toRow, move.toCol);

    if (move.promotion != EMPTY)
        std::cout << "  (promotion)";

    if (move.enPassant)
        std::cout << "  (en passant)";

    if (move.castleKingSide || move.castleQueenSide)
        std::cout << "  (castling)";

    std::cout << "\n";

    check_game_over(game);
}

// ---------------------------------------------------------------------------
// Starts the computer's turn.
//
// The search runs on its own thread, so the window keeps responding (and the
// camera can still be moved) while the computer thinks.
// ---------------------------------------------------------------------------
static void start_computer_turn(Game& game)
{
    if (game.mode != MODE_VS_COMPUTER) return;
    if (game.board.side_to_move() == game.human_colour) return;
    if (game.state != STATE_PLAYING) return;
    if (game.computer_thinking) return;

    game.computer_thinking = true;
    game.ai.start_search(game.board, game.difficulty);

    std::cout << "  Computer (" << difficulty_name(game.difficulty)
              << ") is thinking...\n";
}

// ---------------------------------------------------------------------------
// Checks whether the computer has finished thinking, and plays its move.
// ---------------------------------------------------------------------------
static void update_computer_turn(Game& game)
{
    if (!game.computer_thinking) return;
    if (!game.ai.is_search_complete()) return;

    game.computer_thinking = false;

    const Move move = game.ai.result();

    if (move.is_null())
    {
        // The computer had no move (should not happen, the game would be over).
        check_game_over(game);
        return;
    }

    std::cout << "  Computer searched " << game.ai.positions_searched()
              << " positions.\n";

    play_move(game, move);
}

// ---------------------------------------------------------------------------
// Handles a click on a board square.
//
//   * clicking one of our own pieces selects it and shows its legal moves
//   * clicking a legal destination plays the move
//   * clicking somewhere else clears the selection
// ---------------------------------------------------------------------------
static void handle_square_click(Game& game, int row, int col)
{
    if (game.state != STATE_PLAYING) return;

    // In a game against the computer the player can only move their own side.
    if (game.mode == MODE_VS_COMPUTER &&
        game.board.side_to_move() != game.human_colour)
        return;

    // Already have a piece selected: is this click a legal destination?
    if (game.selected_row >= 0)
    {
        for (const Move& move : game.legal_moves)
        {
            if (move.toRow == row && move.toCol == col)
            {
                // A pawn that reaches the last rank may promote. We ask on the
                // console, which is simpler than building a dialog in 3D.
                if (move.promotion != EMPTY)
                {
                    std::cout << "\n  Pawn promotion - choose a piece:\n";
                    std::cout << "    1. Queen\n    2. Rook\n    3. Bishop\n    4. Knight\n";
                    std::cout << "  Choose (1-4): ";
                    int choice = read_int(1);

                    Move chosen = move;
                    if      (choice == 2) chosen.promotion = ROOK;
                    else if (choice == 3) chosen.promotion = BISHOP;
                    else if (choice == 4) chosen.promotion = KNIGHT;
                    else                  chosen.promotion = QUEEN;

                    play_move(game, chosen);
                    return;
                }

                play_move(game, move);
                return;
            }
        }
    }

    // Otherwise, try to select the piece on this square.
    const Piece piece = game.board.at(row, col);

    if (piece.is_empty() || piece.color != game.board.side_to_move())
    {
        game.selected_row = -1;
        game.selected_col = -1;
        game.legal_moves.clear();
        return;
    }

    game.selected_row = row;
    game.selected_col = col;
    game.legal_moves = game.board.legal_moves_from(row, col);

    std::cout << "  Selected " << ChessBoard::square_name(row, col)
              << " (" << game.legal_moves.size() << " legal move"
              << (game.legal_moves.size() == 1 ? "" : "s") << ")\n";
}

// ---------------------------------------------------------------------------
// The main loop.
// ---------------------------------------------------------------------------
int main()
{
    // -----------------------------------------------------------------------
    // Step 1: ask the player what they want to do, before we open a window.
    // -----------------------------------------------------------------------
    Game game;
    configure_game(game);

    // -----------------------------------------------------------------------
    // Step 2: initialise GLFW and create an OpenGL 3.3 core profile context.
    //
    // We ask for 3.3 because that is the version where modern shader features
    // (and the "#version 330 core" syntax) are guaranteed to be available.
    // -----------------------------------------------------------------------
    if (!glfwInit())
    {
        std::cerr << "[OpenGL] Failed to initialise GLFW" << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!window)
    {
        std::cerr << "[OpenGL] Failed to create an OpenGL 3.3 core profile window" << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);   // synchronise with the monitor refresh rate

    // -----------------------------------------------------------------------
    // Step 3: initialise GLEW, which loads the modern OpenGL function pointers.
    //
    // glewExperimental must be set BEFORE glewInit().
    // -----------------------------------------------------------------------
    glewExperimental = GL_TRUE;
    GLenum glew_status = glewInit();
    if (glew_status != GLEW_OK)
    {
        std::cerr << "[OpenGL] Failed to initialise GLEW: "
                  << glewGetErrorString(glew_status) << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // On some drivers GLEW leaves a harmless GL_INVALID_ENUM behind; clear it
    // so it does not confuse us later.
    glGetError();

    std::cout << "\n[OpenGL] Version : " << glGetString(GL_VERSION) << "\n";
    std::cout << "[OpenGL] GLSL    : " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
    std::cout << "[OpenGL] Renderer: " << glGetString(GL_RENDERER) << "\n";

    // -----------------------------------------------------------------------
    // Step 4: load the shader program.
    // -----------------------------------------------------------------------
    Shader shader;
    const std::string vertex_path   = find_asset_path("assets/shaders/basic.vert");
    const std::string fragment_path = find_asset_path("assets/shaders/basic.frag");

    if (!shader.load_from_files(vertex_path, fragment_path))
    {
        std::cerr << "[OpenGL] Failed to build the shader program" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // -----------------------------------------------------------------------
    // Step 5: set up the OpenGL state we need for a 3D scene.
    //
    // The older examples in the OpenGL/ folder never enabled depth testing,
    // which is why they could only draw flat 2D shapes. For 3D we must turn it
    // on so that nearer objects hide farther ones.
    // -----------------------------------------------------------------------
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glClearColor(0.10f, 0.12f, 0.16f, 1.0f);

    // -----------------------------------------------------------------------
    // Step 6: create the geometry and start the game.
    // -----------------------------------------------------------------------
    if (!game.renderer.initialise())
    {
        std::cerr << "[OpenGL] Failed to create the scene geometry" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    restart_game(game);

    std::cout << "\n  Controls:\n";
    std::cout << "    Left click        select a piece / make a move\n";
    std::cout << "    Left drag         rotate the camera\n";
    std::cout << "    Right drag        zoom in and out\n";
    std::cout << "    R                 restart the game\n";
    std::cout << "    F                 flip the board\n";
    std::cout << "    M                 print the board in the console\n";
    std::cout << "    ESC               quit\n";
    std::cout << "\n";

    // -----------------------------------------------------------------------
    // Step 7: the main loop.
    // -----------------------------------------------------------------------
    bool mouse_dragging = false;
    double last_mouse_x = 0.0;
    double last_mouse_y = 0.0;

    while (!glfwWindowShouldClose(window))
    {
        // Process window events FIRST, so input is always up to date.
        glfwPollEvents();

        int window_width = 0;
        int window_height = 0;
        glfwGetWindowSize(window, &window_width, &window_height);

        // --- Keyboard ---
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        {
            restart_game(game);
            std::cout << "\n  Game restarted.\n";
        }

        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        {
            game.flipped = !game.flipped;
            std::cout << "  Board flipped.\n";
        }

        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
        {
            std::cout << game.board.to_ascii();
        }

        // --- Mouse: camera control and picking ---
        double mouse_x = 0.0;
        double mouse_y = 0.0;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        const bool left_down  = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool right_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        // Dragging with the left button rotates the camera, but only if the
        // player did not just click on a square.
        if (left_down)
        {
            if (!mouse_dragging)
            {
                mouse_dragging = true;
                last_mouse_x = mouse_x;
                last_mouse_y = mouse_y;
            }
            else
            {
                float delta_x = static_cast<float>(mouse_x - last_mouse_x);
                float delta_y = static_cast<float>(mouse_y - last_mouse_y);

                // Only rotate when the mouse has really moved, so a plain
                // click does not also swing the camera.
                if (std::fabs(delta_x) > 1.0f || std::fabs(delta_y) > 1.0f)
                {
                    game.camera.orbit(delta_x * 0.4f, -delta_y * 0.3f, 0.0f);
                    last_mouse_x = mouse_x;
                    last_mouse_y = mouse_y;
                }
            }
        }
        else
        {
            if (mouse_dragging)
            {
                // The button was released: if the mouse barely moved, treat
                // it as a click on a square.
                double moved_x = mouse_x - last_mouse_x;
                double moved_y = mouse_y - last_mouse_y;

                if (std::fabs(moved_x) < 4.0 && std::fabs(moved_y) < 4.0)
                {
                    int row = -1;
                    int col = -1;
                    if (game.renderer.pick_square(static_cast<int>(mouse_x),
                                                 static_cast<int>(mouse_y),
                                                 window_width, window_height,
                                                 game.camera, game.flipped,
                                                 row, col))
                    {
                        handle_square_click(game, row, col);
                    }
                }
            }
            mouse_dragging = false;
        }

        // Right button drag zooms in and out.
        if (right_down)
        {
            game.camera.orbit(0.0f, 0.0f, -0.08f);
        }

        // Scroll wheel also zooms.
        if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
            game.camera.orbit(0.0f, 0.0f, -0.3f);
        if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
            game.camera.orbit(0.0f, 0.0f, 0.3f);

        // --- The computer's turn ---
        update_computer_turn(game);

        // If it is the computer's turn and it is not thinking yet, start it.
        start_computer_turn(game);

        // --- Draw the frame ---
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const Move* last_move_pointer = game.last_move.is_null() ? nullptr : &game.last_move;

        game.renderer.draw_scene(shader, game.board, game.camera,
                                  window_width, window_height,
                                  game.selected_row, game.selected_col,
                                  game.legal_moves, last_move_pointer,
                                  game.flipped);

        glfwSwapBuffers(window);
    }

    // -----------------------------------------------------------------------
    // Clean up. The AI thread must be stopped before anything it touches is
    // destroyed.
    // -----------------------------------------------------------------------
    game.ai.cancel();
    game.ai.wait_for_completion();

    game.renderer.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "\n  Thanks for playing!\n\n";
    return 0;
}
