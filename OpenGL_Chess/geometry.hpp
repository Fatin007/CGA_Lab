// geometry.hpp -- Procedural 3D shapes for the board and the chess pieces.
//
// There are no model files (.obj) in this project, so every shape is built in
// code from simple primitives. Each piece is a stack of cones, cylinders,
// spheres and cubes.
//
// The vertices are stored interleaved as:
//     position (3 floats), normal (3 floats), colour (3 floats)
// which matches the attribute layout the vertex shader expects.

#pragma once

#include "math3d.hpp"
#include "board.hpp"     // for PieceType

#include <vector>

struct Mesh
{
    std::vector<float>        vertices;   // interleaved: pos(3) normal(3) colour(3)
    std::vector<unsigned int> indices;

    void clear() { vertices.clear(); indices.clear(); }

    bool empty() const { return indices.empty(); }
};

// ---------------------------------------------------------------------------
// Basic primitives. All of them are centred on the X/Z axes and sit on the
// plane y = 0, so they can be stacked on top of each other.
// ---------------------------------------------------------------------------

// A cone standing on the XZ plane, with its tip at y = height.
Mesh make_cone(float radius, float height, int segments = 24);

// A cylinder standing on the XZ plane, from y = 0 to y = height.
Mesh make_cylinder(float radius, float height, int segments = 24);

// A cube of the given side length, centred on the origin.
Mesh make_cube(float size);

// A sphere of the given radius, centred on the origin.
Mesh make_sphere(float radius, int segments = 24, int rings = 16);

// A flat disc lying on the XZ plane, used for the legal-move markers.
Mesh make_disc(float radius, int segments = 24);

// A flat square lying on the XZ plane, used for the board squares.
Mesh make_flat_square(float size);

// ---------------------------------------------------------------------------
// Helpers for building the pieces.
// ---------------------------------------------------------------------------

// Copies a mesh, moves it by the given offset and scales it uniformly.
// The colour of every vertex can be replaced at the same time.
Mesh transform_mesh(const Mesh& source, const math3d::Vec3& offset,
                    float uniform_scale,
                    const math3d::Vec3& new_colour = math3d::Vec3(1.0f, 1.0f, 1.0f));

// Joins several meshes into one, so a piece can be drawn with a single call.
Mesh merge_meshes(const std::vector<Mesh>& parts);

// Sets the colour of every vertex in a mesh.
void set_mesh_colour(Mesh& mesh, const math3d::Vec3& colour);

// ---------------------------------------------------------------------------
// The six chess pieces.
//
// Every piece is built in "local space": it stands on the plane y = 0 and is
// centred on the origin. The renderer then moves it to the right board square
// with a model matrix, which is why one mesh per piece type is enough for all
// 32 pieces on the board.
//
// The colour argument lets us build the same shape in white or black.
// ---------------------------------------------------------------------------
Mesh make_pawn  (const math3d::Vec3& colour);
Mesh make_rook  (const math3d::Vec3& colour);
Mesh make_knight(const math3d::Vec3& colour);
Mesh make_bishop(const math3d::Vec3& colour);
Mesh make_queen (const math3d::Vec3& colour);
Mesh make_king  (const math3d::Vec3& colour);

// Builds the mesh for any piece type, used by the renderer.
Mesh make_piece(PieceType type, const math3d::Vec3& colour);
