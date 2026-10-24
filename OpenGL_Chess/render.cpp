// render.cpp -- Drawing the scene: camera, board squares, pieces and picking.

#include "render.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Colours used throughout the scene.
// ---------------------------------------------------------------------------
namespace
{
    const math3d::Vec3 LIGHT_SQUARE_COLOUR(0.86f, 0.80f, 0.68f);
    const math3d::Vec3 DARK_SQUARE_COLOUR (0.45f, 0.33f, 0.22f);

    const math3d::Vec3 WHITE_PIECE_COLOUR(0.93f, 0.91f, 0.86f);
    const math3d::Vec3 BLACK_PIECE_COLOUR(0.13f, 0.13f, 0.15f);

    const math3d::Vec3 SELECTED_COLOUR   (0.20f, 0.80f, 0.30f);
    const math3d::Vec3 LEGAL_MOVE_COLOUR (0.20f, 0.60f, 0.95f);
    const math3d::Vec3 LAST_MOVE_COLOUR  (0.95f, 0.75f, 0.20f);
    const math3d::Vec3 CHECK_COLOUR      (0.90f, 0.15f, 0.15f);

    // The piece types, in the order we store them in the buffer table.
    const PieceType PIECE_ORDER[6] = { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

    // Maps a PieceType to its row in the buffer table.
    int piece_slot(PieceType type)
    {
        for (int i = 0; i < 6; ++i)
            if (PIECE_ORDER[i] == type) return i;
        return 0;   // pawn
    }
}

// ---------------------------------------------------------------------------
// Board layout.
// ---------------------------------------------------------------------------
math3d::Vec3 board_layout::square_to_world(int row, int col, bool flipped)
{
    // Row 0 is rank 1 (White's home rank) and column 0 is file a.
    float x = static_cast<float>(col) - 3.5f;
    float z = static_cast<float>(row) - 3.5f;

    // Flipping the board turns it around, so the z axis is mirrored.
    if (flipped)
        z = -z;

    return math3d::Vec3(x, 0.0f, z);
}

bool board_layout::world_to_square(const math3d::Vec3& world, bool flipped,
                                   int& row, int& col)
{
    float z = world.z;
    if (flipped)
        z = -z;

    int c = static_cast<int>(std::lround(world.x + 3.5f));
    int r = static_cast<int>(std::lround(z + 3.5f));

    if (r < 0 || r > 7 || c < 0 || c > 7)
        return false;

    row = r;
    col = c;
    return true;
}

// ---------------------------------------------------------------------------
// Camera.
// ---------------------------------------------------------------------------
math3d::Vec3 Camera::eye_position() const
{
    // Convert the spherical angles into a position on a sphere around the
    // target point.
    float yaw_radians   = math3d::to_radians(yaw);
    float pitch_radians = math3d::to_radians(pitch);

    float x = distance * std::cos(pitch_radians) * std::sin(yaw_radians);
    float y = distance * std::sin(pitch_radians);
    float z = distance * std::cos(pitch_radians) * std::cos(yaw_radians);

    return target + math3d::Vec3(x, y, z);
}

math3d::Mat4 Camera::view_matrix() const
{
    return math3d::look_at(eye_position(), target, math3d::Vec3(0.0f, 1.0f, 0.0f));
}

void Camera::orbit(float delta_yaw, float delta_pitch, float delta_distance)
{
    yaw += delta_yaw;

    // We keep the camera between 5 and 85 degrees above the board. The lower
    // limit stops the view from becoming flat, and the upper limit stops it
    // from going directly overhead (which would make picking awkward).
    pitch += delta_pitch;
    pitch = std::max(5.0f, std::min(85.0f, pitch));

    distance += delta_distance;
    distance = std::max(5.0f, std::min(22.0f, distance));
}

// ---------------------------------------------------------------------------
// Renderer.
// ---------------------------------------------------------------------------
Renderer::~Renderer()
{
    shutdown();
}

void Renderer::shutdown()
{
    if (board_vao_)   { glDeleteVertexArrays(1, &board_vao_);   board_vao_ = 0; }
    if (board_vbo_)   { glDeleteBuffers(1, &board_vbo_);         board_vbo_ = 0; }
    if (board_ebo_)   { glDeleteBuffers(1, &board_ebo_);         board_ebo_ = 0; }

    for (PieceBuffer& buffer : piece_buffers_)
    {
        if (buffer.vao) glDeleteVertexArrays(1, &buffer.vao);
        if (buffer.vbo) glDeleteBuffers(1, &buffer.vbo);
        if (buffer.ebo) glDeleteBuffers(1, &buffer.ebo);
        buffer = PieceBuffer();
    }

    if (marker_vao_)    { glDeleteVertexArrays(1, &marker_vao_);    marker_vao_ = 0; }
    if (marker_vbo_)    { glDeleteBuffers(1, &marker_vbo_);        marker_vbo_ = 0; }
    if (marker_ebo_)    { glDeleteBuffers(1, &marker_ebo_);        marker_ebo_ = 0; }

    if (highlight_vao_) { glDeleteVertexArrays(1, &highlight_vao_); highlight_vao_ = 0; }
    if (highlight_vbo_) { glDeleteBuffers(1, &highlight_vbo_);     highlight_vbo_ = 0; }
    if (highlight_ebo_) { glDeleteBuffers(1, &highlight_ebo_);     highlight_ebo_ = 0; }
}

// Uploads a mesh into a fresh vertex array object and returns its index count.
unsigned int Renderer::create_mesh_buffer(const Mesh& mesh,
                                          unsigned int& vao_out,
                                          unsigned int& vbo_out,
                                          unsigned int& ebo_out) const
{
    glGenVertexArrays(1, &vao_out);
    glGenBuffers(1, &vbo_out);
    glGenBuffers(1, &ebo_out);

    glBindVertexArray(vao_out);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_out);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_out);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(unsigned int)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    // The vertex format is interleaved: position(3), normal(3), colour(3),
    // so the stride is 9 floats.
    const GLsizei stride = 9 * sizeof(float);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return static_cast<unsigned int>(mesh.indices.size());
}

bool Renderer::initialise()
{
    // -----------------------------------------------------------------------
    // The board: 64 flat squares, each with its own colour, all baked into a
    // single buffer so the whole board is one draw call.
    //
    // We build this by hand rather than calling make_flat_square() 64 times,
    // because each square needs its own colour and its own position.
    // -----------------------------------------------------------------------
    {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        for (int row = 0; row < 8; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                // The bottom-left corner of this square in world space.
                float x = static_cast<float>(col) - 4.0f;
                float z = static_cast<float>(row) - 4.0f;

                // Light and dark squares alternate, and the a8 square (the
                // top-left corner from White's point of view) is light.
                bool is_light = ((row + col) % 2) == 1;
                const math3d::Vec3& colour = is_light ? LIGHT_SQUARE_COLOUR : DARK_SQUARE_COLOUR;

                unsigned int base = static_cast<unsigned int>(vertices.size() / 9);

                const float corners[4][2] = {
                    { x,                                      z },
                    { x,                                      z + board_layout::SQUARE_SIZE },
                    { x + board_layout::SQUARE_SIZE,           z + board_layout::SQUARE_SIZE },
                    { x + board_layout::SQUARE_SIZE,           z }
                };

                for (const auto& corner : corners)
                {
                    vertices.push_back(corner[0]);   // position x
                    vertices.push_back(0.0f);        // position y
                    vertices.push_back(corner[1]);   // position z
                    vertices.push_back(0.0f);        // normal x
                    vertices.push_back(1.0f);        // normal y
                    vertices.push_back(0.0f);        // normal z
                    vertices.push_back(colour.x);    // colour r
                    vertices.push_back(colour.y);    // colour g
                    vertices.push_back(colour.z);    // colour b
                }

                indices.push_back(base);
                indices.push_back(base + 1);
                indices.push_back(base + 2);

                indices.push_back(base);
                indices.push_back(base + 2);
                indices.push_back(base + 3);
            }
        }

        board_index_count_ = create_mesh_buffer_from_arrays(vertices, indices,
                                                             board_vao_, board_vbo_, board_ebo_);
    }

    // -----------------------------------------------------------------------
    // The six piece types, in two colours each: 12 buffers in total.
    //
    // A piece mesh is built in "local space": standing on the plane y = 0 and
    // centred on the origin. The model matrix then moves it to the square it
    // belongs on, so one buffer can serve every piece of that type.
    // -----------------------------------------------------------------------
    for (int slot = 0; slot < 6; ++slot)
    {
        for (int colour = 0; colour < 2; ++colour)
        {
            const math3d::Vec3 piece_colour = (colour == 0) ? WHITE_PIECE_COLOUR
                                                             : BLACK_PIECE_COLOUR;

            Mesh mesh = make_piece(PIECE_ORDER[slot], piece_colour);

            PieceBuffer& buffer = piece_buffers_[static_cast<size_t>(slot) * 2 + static_cast<size_t>(colour)];
            buffer.index_count = create_mesh_buffer(mesh, buffer.vao, buffer.vbo, buffer.ebo);
        }
    }

    // -----------------------------------------------------------------------
    // The marker disc, used to show where a selected piece may move.
    //
    // The mesh is white: the actual colour comes from the uTint uniform, so
    // the same buffer can be reused for any marker colour.
    // -----------------------------------------------------------------------
    {
        Mesh disc = make_disc(0.17f, 24);
        set_mesh_colour(disc, math3d::Vec3(1.0f, 1.0f, 1.0f));
        marker_index_count_ = create_mesh_buffer(disc, marker_vao_, marker_vbo_, marker_ebo_);
    }

    // -----------------------------------------------------------------------
    // The highlight square, used for the selected square, the last move and
    // the king in check. Like the marker, it is white in the mesh and takes
    // its colour from the uTint uniform.
    // -----------------------------------------------------------------------
    {
        Mesh square = make_flat_square(board_layout::SQUARE_SIZE * 0.96f);
        set_mesh_colour(square, math3d::Vec3(1.0f, 1.0f, 1.0f));
        highlight_index_count_ = create_mesh_buffer(square, highlight_vao_, highlight_vbo_, highlight_ebo_);
    }

    return true;
}

// Uploads raw vertex and index arrays. Used for the board, which is built
// directly rather than from a Mesh.
unsigned int Renderer::create_mesh_buffer_from_arrays(const std::vector<float>& vertices,
                                                       const std::vector<unsigned int>& indices,
                                                       unsigned int& vao_out,
                                                       unsigned int& vbo_out,
                                                       unsigned int& ebo_out) const
{
    glGenVertexArrays(1, &vao_out);
    glGenBuffers(1, &vbo_out);
    glGenBuffers(1, &ebo_out);

    glBindVertexArray(vao_out);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_out);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_out);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = 9 * sizeof(float);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return static_cast<unsigned int>(indices.size());
}

// ---------------------------------------------------------------------------
// Drawing a frame.
//
// The shader is passed in because the renderer needs to set the uniforms for
// each object it draws, and the program lives in main.cpp.
// ---------------------------------------------------------------------------
void Renderer::draw_scene(const Shader& shader,
                           const ChessBoard& board,
                           const Camera& camera,
                           int window_width, int window_height,
                           int selected_row, int selected_col,
                           const std::vector<Move>& legal_moves,
                           const Move* last_move,
                           bool flipped)
{
    // --- Camera matrices ---
    const float aspect = static_cast<float>(window_width) /
                         static_cast<float>(std::max(1, window_height));

    const math3d::Mat4 projection = math3d::perspective(45.0f, aspect, 0.1f, 100.0f);
    const math3d::Mat4 view       = camera.view_matrix();
    const math3d::Vec3 eye        = camera.eye_position();

    shader.use();
    shader.set_mat4("uProjection", projection);
    shader.set_mat4("uView", view);
    shader.set_vec3("uEyePosition", eye);

    // A fixed directional light from above and to the side.
    shader.set_vec3("uLightDirection", math3d::Vec3(0.4f, 1.0f, 0.6f));
    shader.set_vec3("uLightColour", math3d::Vec3(0.95f, 0.94f, 0.90f));
    shader.set_vec3("uAmbientColour", math3d::Vec3(0.30f, 0.30f, 0.34f));
    shader.set_float("uShininess", 28.0f);

    // Everything is drawn with a white tint unless we say otherwise. The tint
    // multiplies the colour stored in the mesh.
    const math3d::Vec3 NO_TINT(1.0f, 1.0f, 1.0f);
    shader.set_vec3("uTint", NO_TINT);

    // --- Helper that draws a flat shape lying on the board ---
    //
    // The flat shapes (the highlight square and the move markers) are white
    // in the mesh, and the colour we want comes from the uTint uniform. That
    // way one buffer can be drawn in several different colours.
    auto draw_flat_shape = [&](unsigned int vao, unsigned int index_count,
                               const math3d::Vec3& position, const math3d::Vec3& colour)
    {
        const math3d::Mat4 model = math3d::translate(position.x, position.y, position.z);

        shader.set_mat4("uModel", model);
        shader.set_vec3("uTint", colour);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count),
                       GL_UNSIGNED_INT, (void*)0);
        glBindVertexArray(0);
    };

    // --- Helper that draws a flat shape on a particular square ---
    auto draw_flat_on_square = [&](unsigned int vao, unsigned int index_count,
                                   int row, int col, float height,
                                   const math3d::Vec3& colour)
    {
        math3d::Vec3 position = board_layout::square_to_world(row, col, flipped);
        position.y = height;
        draw_flat_shape(vao, index_count, position, colour);
    };

    // -----------------------------------------------------------------------
    // 1. The board squares.
    // -----------------------------------------------------------------------
    shader.set_mat4("uModel", math3d::Mat4::identity());
    shader.set_vec3("uTint", NO_TINT);
    glBindVertexArray(board_vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(board_index_count_),
                   GL_UNSIGNED_INT, (void*)0);
    glBindVertexArray(0);

    // -----------------------------------------------------------------------
    // 2. The two squares of the move that was played last.
    // -----------------------------------------------------------------------
    if (last_move != nullptr)
    {
        draw_flat_on_square(highlight_vao_, highlight_index_count_,
                            last_move->fromRow, last_move->fromCol, 0.010f,
                            LAST_MOVE_COLOUR);
        draw_flat_on_square(highlight_vao_, highlight_index_count_,
                            last_move->toRow, last_move->toCol, 0.010f,
                            LAST_MOVE_COLOUR);
    }

    // -----------------------------------------------------------------------
    // 3. The selected square.
    // -----------------------------------------------------------------------
    if (selected_row >= 0 && selected_col >= 0)
    {
        draw_flat_on_square(highlight_vao_, highlight_index_count_,
                            selected_row, selected_col, 0.015f,
                            SELECTED_COLOUR);
    }

    // -----------------------------------------------------------------------
    // 4. The markers showing where the selected piece may move.
    // -----------------------------------------------------------------------
    for (const Move& move : legal_moves)
    {
        draw_flat_on_square(marker_vao_, marker_index_count_,
                            move.toRow, move.toCol, 0.020f,
                            LEGAL_MOVE_COLOUR);
    }

    // -----------------------------------------------------------------------
    // 5. The pieces.
    // -----------------------------------------------------------------------
    for (int row = 0; row < 8; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            const Piece piece = board.at(row, col);
            if (piece.is_empty()) continue;

            math3d::Vec3 position = board_layout::square_to_world(row, col, flipped);
            position.y = board_layout::PIECE_HEIGHT;

            // The model matrix moves the piece from its local space to the
            // board. Only translation is needed, because the pieces were built
            // at the right size already.
            math3d::Mat4 model = math3d::translate(position.x, position.y, position.z);
            shader.set_mat4("uModel", model);
            shader.set_vec3("uTint", NO_TINT);

            const int slot = piece_slot(piece.type);
            const int colour = (piece.color == WHITE) ? 0 : 1;

            const PieceBuffer& buffer =
                piece_buffers_[static_cast<size_t>(slot) * 2 + static_cast<size_t>(colour)];

            glBindVertexArray(buffer.vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(buffer.index_count),
                           GL_UNSIGNED_INT, (void*)0);
            glBindVertexArray(0);
        }
    }

    // -----------------------------------------------------------------------
    // 6. The king in check is marked with a red square.
    // -----------------------------------------------------------------------
    if (board.in_check(board.side_to_move()))
    {
        int king_row = -1;
        int king_col = -1;
        for (int row = 0; row < 8 && king_row == -1; ++row)
        {
            for (int col = 0; col < 8; ++col)
            {
                const Piece piece = board.at(row, col);
                if (piece.type == KING && piece.color == board.side_to_move())
                {
                    king_row = row;
                    king_col = col;
                    break;
                }
            }
        }

        if (king_row != -1)
        {
            draw_flat_on_square(highlight_vao_, highlight_index_count_,
                                king_row, king_col, 0.025f,
                                CHECK_COLOUR);
        }
    }
}

// ---------------------------------------------------------------------------
// Picking: turning a mouse position into a board square.
//
// The idea:
//   1. "Unproject" the cursor position back into the world using the inverse
//      of (projection * view). We do this twice, once for the near plane and
//      once for the far plane, which gives us a ray that starts at the camera
//      and goes through the cursor.
//   2. Find where that ray crosses the board plane (y = 0).
//   3. Convert that world position back into a square.
// ---------------------------------------------------------------------------
bool Renderer::pick_square(int mouse_x, int mouse_y,
                            int window_width, int window_height,
                            const Camera& camera, bool flipped,
                            int& row, int& col) const
{
    if (window_width <= 0 || window_height <= 0)
        return false;

    const float aspect = static_cast<float>(window_width) /
                         static_cast<float>(window_height);

    const math3d::Mat4 projection = math3d::perspective(45.0f, aspect, 0.1f, 100.0f);
    const math3d::Mat4 view       = camera.view_matrix();
    const math3d::Vec3 eye        = camera.eye_position();

    // --- Step 1: unproject the cursor into the world ---
    //
    // math3d::unproject_from_screen does the perspective divide for us, so
    // the two points we get back are genuine world positions.
    math3d::Vec3 near_point;
    math3d::Vec3 far_point;

    if (!math3d::unproject_from_screen(projection, view,
                                       window_width, window_height,
                                       static_cast<float>(mouse_x),
                                       static_cast<float>(mouse_y),
                                       -1.0f, near_point))
        return false;   // the cursor is behind the camera

    if (!math3d::unproject_from_screen(projection, view,
                                       window_width, window_height,
                                       static_cast<float>(mouse_x),
                                       static_cast<float>(mouse_y),
                                       1.0f, far_point))
        return false;

    // The ray goes from the camera through the cursor.
    const math3d::Vec3 direction = (far_point - near_point).normalized();

    // --- Step 2: intersect the ray with the board plane (y = 0) ---
    // The plane equation is y = 0, so:
    //     eye.y + t * direction.y = 0   ->   t = -eye.y / direction.y
    if (std::fabs(direction.y) < 1e-6f)
        return false;   // the ray is parallel to the board, so it never hits

    const float t = -eye.y / direction.y;
    if (t < 0.0f)
        return false;   // the board is behind the camera

    const math3d::Vec3 hit = eye + direction * t;

    // --- Step 3: convert the world position into a square ---
    return board_layout::world_to_square(hit, flipped, row, col);
}
