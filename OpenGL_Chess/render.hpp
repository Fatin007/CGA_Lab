// render.hpp -- Everything to do with drawing the scene.
//
// The renderer owns:
//   * the camera (orbit around the board, zoom in and out)
//   * the OpenGL buffers for the board squares and the six piece types
//   * the drawing of a frame
//   * turning a mouse position into a board square (picking)

#pragma once

#include "math3d.hpp"
#include "shader.hpp"
#include "geometry.hpp"
#include "board.hpp"

#include <array>

// ---------------------------------------------------------------------------
// Board geometry constants.
//
// The board is 8x8 squares, each 1 unit across, centred on the origin.
// Square (row, col) maps to world position:
//
//     x = col - 3.5        (file a is at x = -3.5, file h at x = +3.5)
//     z = row - 3.5        (rank 1 is at z = -3.5, rank 8 at z = +3.5)
//
// Flipping the board simply negates z.
// ---------------------------------------------------------------------------
namespace board_layout
{
    const float SQUARE_SIZE = 1.0f;
    const float BOARD_SIZE  = 8.0f;
    const float PIECE_HEIGHT = 0.02f;   // lifts pieces just above the board

    // Converts a board square into a position on the XZ plane.
    math3d::Vec3 square_to_world(int row, int col, bool flipped);

    // The opposite direction: which square is at this world position?
    // Returns false if the position is outside the board.
    bool world_to_square(const math3d::Vec3& world, bool flipped, int& row, int& col);
}

// ---------------------------------------------------------------------------
// Camera.
//
// The camera orbits around the centre of the board. We keep the angles and
// the distance, and rebuild the view matrix every frame with look_at().
// ---------------------------------------------------------------------------
struct Camera
{
    float yaw          = 0.0f;      // degrees around the Y axis
    float pitch        = 55.0f;     // degrees above the board
    float distance     = 11.0f;
    math3d::Vec3 target = math3d::Vec3(0.0f, 0.0f, 0.0f);

    // Builds the view matrix from the current angles.
    math3d::Mat4 view_matrix() const;

    // Where the camera eye currently is (needed for specular lighting and for
    // building the picking ray).
    math3d::Vec3 eye_position() const;

    // Moves the camera. delta_x and delta_y are in degrees, delta_distance
    // is a change in the distance from the board.
    void orbit(float delta_yaw, float delta_pitch, float delta_distance);
};

// ---------------------------------------------------------------------------
// Renderer.
//
// It holds the GPU buffers for the static geometry (the 64 board squares and
// the six piece types) and knows how to draw a frame.
// ---------------------------------------------------------------------------
class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    // Non-copyable: it owns OpenGL object names.
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Creates the OpenGL buffers. Returns false on failure.
    bool initialise();

    // Draws the whole scene.
    //
    //   shader        the shader program to use
    //   board         the chess position to draw
    //   camera        where to look from
    //   window_width  width of the window, needed for the aspect ratio
    //   window_height height of the window, needed for the aspect ratio
    //   selected      the square the player has picked (-1 for none)
    //   legal_moves   the legal destinations of the selected piece
    //   last_move     the move that was just played
    //   flipped       true if the board is shown from Black's side
    void draw_scene(const Shader& shader,
                    const ChessBoard& board,
                    const Camera& camera,
                    int window_width, int window_height,
                    int selected_row, int selected_col,
                    const std::vector<Move>& legal_moves,
                    const Move* last_move,
                    bool flipped);

    // Turns a mouse position (in window pixels) into a board square.
    //
    //   mouse_x, mouse_y   position relative to the top-left of the window
    //   window_width       width of the window in pixels
    //   window_height      height of the window in pixels
    //   camera             used to build the picking ray
    //   flipped            true if the board is shown from Black's side
    //
    // Returns false if the ray does not hit the board.
    bool pick_square(int mouse_x, int mouse_y,
                     int window_width, int window_height,
                     const Camera& camera, bool flipped,
                     int& row, int& col) const;

    // Releases the OpenGL buffers.
    void shutdown();

private:
    // Uploads a mesh into a new vertex array object + buffer.
    unsigned int create_mesh_buffer(const Mesh& mesh,
                                    unsigned int& vao_out,
                                    unsigned int& vbo_out,
                                    unsigned int& ebo_out) const;

    // Uploads raw vertex and index arrays. Used for the board, which is built
    // directly rather than from a Mesh.
    unsigned int create_mesh_buffer_from_arrays(const std::vector<float>& vertices,
                                                const std::vector<unsigned int>& indices,
                                                unsigned int& vao_out,
                                                unsigned int& vbo_out,
                                                unsigned int& ebo_out) const;

    // The 64 board squares, baked into a single buffer (one draw call).
    unsigned int board_vao_ = 0;
    unsigned int board_vbo_ = 0;
    unsigned int board_ebo_ = 0;
    unsigned int board_index_count_ = 0;

    // One buffer per piece type and colour: [type][colour].
    struct PieceBuffer
    {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        unsigned int index_count = 0;
    };
    std::array<PieceBuffer, 12> piece_buffers_;

    // A flat disc used to mark the legal destination squares.
    unsigned int marker_vao_ = 0;
    unsigned int marker_vbo_ = 0;
    unsigned int marker_ebo_ = 0;
    unsigned int marker_index_count_ = 0;

    // A flat square used for the selection and last-move highlights.
    unsigned int highlight_vao_ = 0;
    unsigned int highlight_vbo_ = 0;
    unsigned int highlight_ebo_ = 0;
    unsigned int highlight_index_count_ = 0;
};
