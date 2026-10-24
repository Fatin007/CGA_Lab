// geometry.cpp -- Building the 3D shapes for the board and the chess pieces.
//
// Every function here generates vertices on the CPU. Nothing is loaded from
// disk, which keeps the project self-contained.
//
// The vertex format is interleaved:
//     position (3) | normal (3) | colour (3)

#include "geometry.hpp"

#include <cmath>
#include <algorithm>

namespace
{

// Appends one vertex to a mesh.
void add_vertex(Mesh& mesh, const math3d::Vec3& position,
                const math3d::Vec3& normal, const math3d::Vec3& colour)
{
    mesh.vertices.push_back(position.x);
    mesh.vertices.push_back(position.y);
    mesh.vertices.push_back(position.z);

    mesh.vertices.push_back(normal.x);
    mesh.vertices.push_back(normal.y);
    mesh.vertices.push_back(normal.z);

    mesh.vertices.push_back(colour.x);
    mesh.vertices.push_back(colour.y);
    mesh.vertices.push_back(colour.z);
}

// Adds a triangle by its three vertex indices.
void add_triangle(Mesh& mesh, unsigned int a, unsigned int b, unsigned int c)
{
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);
}

// The default colour used while building shapes.
const math3d::Vec3 WHITE_COLOUR(1.0f, 1.0f, 1.0f);

} // namespace

// ---------------------------------------------------------------------------
// Cone: a circular base at y = 0 with a single point (the tip) at y = height.
//
// The side surface is made of triangles, and the base is a flat fan.
// ---------------------------------------------------------------------------
Mesh make_cone(float radius, float height, int segments)
{
    Mesh mesh;
    segments = std::max(3, segments);

    const float two_pi = 6.28318530718f;

    // --- Tip vertex ---
    const unsigned int tip_index = static_cast<unsigned int>(mesh.vertices.size() / 9);
    add_vertex(mesh, math3d::Vec3(0.0f, height, 0.0f),
                     math3d::Vec3(0.0f, 1.0f, 0.0f), WHITE_COLOUR);

    // --- Base rim vertices ---
    // Each rim vertex has an outward normal that tilts with the cone's slope.
    const float slope = radius / height;
    std::vector<unsigned int> rim;

    for (int i = 0; i <= segments; ++i)
    {
        float angle = two_pi * static_cast<float>(i) / static_cast<float>(segments);
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;

        math3d::Vec3 normal = math3d::Vec3(x, slope * radius, z).normalized();

        rim.push_back(static_cast<unsigned int>(mesh.vertices.size() / 9));
        add_vertex(mesh, math3d::Vec3(x, 0.0f, z), normal, WHITE_COLOUR);
    }

    // --- Side surface ---
    for (int i = 0; i < segments; ++i)
        add_triangle(mesh, tip_index, rim[i + 1], rim[i]);

    // --- Base (facing down) ---
    const unsigned int centre_index = static_cast<unsigned int>(mesh.vertices.size() / 9);
    add_vertex(mesh, math3d::Vec3(0.0f, 0.0f, 0.0f),
                     math3d::Vec3(0.0f, -1.0f, 0.0f), WHITE_COLOUR);

    std::vector<unsigned int> base_rim;
    for (int i = 0; i <= segments; ++i)
    {
        float angle = two_pi * static_cast<float>(i) / static_cast<float>(segments);
        base_rim.push_back(static_cast<unsigned int>(mesh.vertices.size() / 9));
        add_vertex(mesh,
                   math3d::Vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius),
                   math3d::Vec3(0.0f, -1.0f, 0.0f), WHITE_COLOUR);
    }

    for (int i = 0; i < segments; ++i)
        add_triangle(mesh, centre_index, base_rim[i], base_rim[i + 1]);

    return mesh;
}

// ---------------------------------------------------------------------------
// Cylinder: a circular tube from y = 0 to y = height.
// ---------------------------------------------------------------------------
Mesh make_cylinder(float radius, float height, int segments)
{
    Mesh mesh;
    segments = std::max(3, segments);

    const float two_pi = 6.28318530718f;

    // --- Side wall ---
    // We need two rings of vertices (bottom and top) because the side
    // normals point outwards and are therefore different from the cap normals.
    std::vector<unsigned int> bottom;
    std::vector<unsigned int> top;

    for (int i = 0; i <= segments; ++i)
    {
        float angle = two_pi * static_cast<float>(i) / static_cast<float>(segments);

        // The normal points straight outwards from the axis, so it is the unit
        // vector at this angle (not the scaled position).
        float nx = std::cos(angle);
        float nz = std::sin(angle);

        float x = nx * radius;
        float z = nz * radius;
        math3d::Vec3 normal(nx, 0.0f, nz);

        bottom.push_back(static_cast<unsigned int>(mesh.vertices.size() / 9));
        add_vertex(mesh, math3d::Vec3(x, 0.0f, z), normal, WHITE_COLOUR);

        top.push_back(static_cast<unsigned int>(mesh.vertices.size() / 9));
        add_vertex(mesh, math3d::Vec3(x, height, z), normal, WHITE_COLOUR);
    }

    for (int i = 0; i < segments; ++i)
    {
        add_triangle(mesh, bottom[i], bottom[i + 1], top[i]);
        add_triangle(mesh, bottom[i + 1], top[i + 1], top[i]);
    }

    // --- Top cap ---
    const unsigned int top_centre = static_cast<unsigned int>(mesh.vertices.size() / 9);
    add_vertex(mesh, math3d::Vec3(0.0f, height, 0.0f),
                     math3d::Vec3(0.0f, 1.0f, 0.0f), WHITE_COLOUR);

    std::vector<unsigned int> top_rim;
    for (int i = 0; i <= segments; ++i)
    {
        float angle = two_pi * static_cast<float>(i) / static_cast<float>(segments);
        top_rim.push_back(static_cast<unsigned int>(mesh.vertices.size() / 9));
        add_vertex(mesh,
                   math3d::Vec3(std::cos(angle) * radius, height, std::sin(angle) * radius),
                   math3d::Vec3(0.0f, 1.0f, 0.0f), WHITE_COLOUR);
    }

    for (int i = 0; i < segments; ++i)
        add_triangle(mesh, top_centre, top_rim[i], top_rim[i + 1]);

    // --- Bottom cap ---
    const unsigned int bottom_centre = static_cast<unsigned int>(mesh.vertices.size() / 9);
    add_vertex(mesh, math3d::Vec3(0.0f, 0.0f, 0.0f),
                     math3d::Vec3(0.0f, -1.0f, 0.0f), WHITE_COLOUR);

    std::vector<unsigned int> bottom_rim;
    for (int i = 0; i <= segments; ++i)
    {
        float angle = two_pi * static_cast<float>(i) / static_cast<float>(segments);
        bottom_rim.push_back(static_cast<unsigned int>(mesh.vertices.size() / 9));
        add_vertex(mesh,
                   math3d::Vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius),
                   math3d::Vec3(0.0f, -1.0f, 0.0f), WHITE_COLOUR);
    }

    for (int i = 0; i < segments; ++i)
        add_triangle(mesh, bottom_centre, bottom_rim[i + 1], bottom_rim[i]);

    return mesh;
}

// ---------------------------------------------------------------------------
// Cube: six separate faces, each with its own normal.
// ---------------------------------------------------------------------------
Mesh make_cube(float size)
{
    Mesh mesh;

    const float h = size * 0.5f;

    // Each face is described by its normal and four corners.
    struct Face
    {
        math3d::Vec3 normal;
        math3d::Vec3 corners[4];
    };

    const Face faces[6] = {
        // +Y (top)
        { math3d::Vec3( 0,  1,  0), { { -h,  h,  h }, { -h,  h, -h }, {  h,  h, -h }, {  h,  h,  h } } },
        // -Y (bottom)
        { math3d::Vec3( 0, -1,  0), { { -h, -h, -h }, { -h, -h,  h }, {  h, -h,  h }, {  h, -h, -h } } },
        // +X (right)
        { math3d::Vec3( 1,  0,  0), { {  h, -h,  h }, {  h, -h, -h }, {  h,  h, -h }, {  h,  h,  h } } },
        // -X (left)
        { math3d::Vec3(-1,  0,  0), { { -h, -h, -h }, { -h, -h,  h }, { -h,  h,  h }, { -h,  h, -h } } },
        // +Z (front)
        { math3d::Vec3( 0,  0,  1), { { -h, -h,  h }, {  h, -h,  h }, {  h,  h,  h }, { -h,  h,  h } } },
        // -Z (back)
        { math3d::Vec3( 0,  0, -1), { {  h, -h, -h }, { -h, -h, -h }, { -h,  h, -h }, {  h,  h, -h } } }
    };

    for (const Face& face : faces)
    {
        unsigned int base = static_cast<unsigned int>(mesh.vertices.size() / 9);
        for (int i = 0; i < 4; ++i)
            add_vertex(mesh, face.corners[i], face.normal, WHITE_COLOUR);

        add_triangle(mesh, base, base + 1, base + 2);
        add_triangle(mesh, base, base + 2, base + 3);
    }

    return mesh;
}

// ---------------------------------------------------------------------------
// Sphere: built as a stack of rings, each ring made of triangles.
//
// We use a UV sphere: `rings` horizontal bands from the top pole to the
// bottom pole, each with `segments` vertices around.
// ---------------------------------------------------------------------------
Mesh make_sphere(float radius, int segments, int rings)
{
    Mesh mesh;
    segments = std::max(3, segments);
    rings    = std::max(2, rings);

    const float pi = 3.14159265359f;

    std::vector<std::vector<unsigned int>> ring_indices(rings + 1);

    // --- Top pole ---
    const unsigned int top = static_cast<unsigned int>(mesh.vertices.size() / 9);
    add_vertex(mesh, math3d::Vec3(0.0f, radius, 0.0f),
                     math3d::Vec3(0.0f, 1.0f, 0.0f), WHITE_COLOUR);
    ring_indices[0].push_back(top);

    // --- Rings in between ---
    for (int r = 1; r < rings; ++r)
    {
        float phi   = pi * static_cast<float>(r) / static_cast<float>(rings);
        float y     = std::cos(phi) * radius;
        float ring_radius = std::sin(phi) * radius;

        for (int s = 0; s <= segments; ++s)
        {
            float theta = 2.0f * pi * static_cast<float>(s) / static_cast<float>(segments);
            float x = std::cos(theta) * ring_radius;
            float z = std::sin(theta) * ring_radius;

            math3d::Vec3 normal(x, y, z);
            normal = normal.normalized();

            ring_indices[r].push_back(static_cast<unsigned int>(mesh.vertices.size() / 9));
            add_vertex(mesh, math3d::Vec3(x, y, z), normal, WHITE_COLOUR);
        }
    }

    // --- Bottom pole ---
    const unsigned int bottom = static_cast<unsigned int>(mesh.vertices.size() / 9);
    add_vertex(mesh, math3d::Vec3(0.0f, -radius, 0.0f),
                     math3d::Vec3(0.0f, -1.0f, 0.0f), WHITE_COLOUR);
    ring_indices[rings].push_back(bottom);

    // --- Triangles between the rings ---
    // Top pole to first ring.
    for (int s = 0; s < segments; ++s)
        add_triangle(mesh, ring_indices[0][0], ring_indices[1][s + 1], ring_indices[1][s]);

    // Middle rings.
    for (int r = 1; r < rings - 1; ++r)
    {
        for (int s = 0; s < segments; ++s)
        {
            unsigned int a = ring_indices[r][s];
            unsigned int b = ring_indices[r][s + 1];
            unsigned int c = ring_indices[r + 1][s];
            unsigned int d = ring_indices[r + 1][s + 1];

            add_triangle(mesh, a, b, c);
            add_triangle(mesh, b, d, c);
        }
    }

    // Last ring to bottom pole.
    for (int s = 0; s < segments; ++s)
        add_triangle(mesh, ring_indices[rings - 1][s], ring_indices[rings - 1][s + 1],
                     ring_indices[rings][0]);

    return mesh;
}

// ---------------------------------------------------------------------------
// Disc: a flat circle on the XZ plane, facing up.
// ---------------------------------------------------------------------------
Mesh make_disc(float radius, int segments)
{
    Mesh mesh;
    segments = std::max(3, segments);

    const float two_pi = 6.28318530718f;
    const math3d::Vec3 up(0.0f, 1.0f, 0.0f);

    const unsigned int centre = static_cast<unsigned int>(mesh.vertices.size() / 9);
    add_vertex(mesh, math3d::Vec3(0.0f, 0.0f, 0.0f), up, WHITE_COLOUR);

    std::vector<unsigned int> rim;
    for (int i = 0; i <= segments; ++i)
    {
        float angle = two_pi * static_cast<float>(i) / static_cast<float>(segments);
        rim.push_back(static_cast<unsigned int>(mesh.vertices.size() / 9));
        add_vertex(mesh,
                   math3d::Vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius),
                   up, WHITE_COLOUR);
    }

    for (int i = 0; i < segments; ++i)
        add_triangle(mesh, centre, rim[i], rim[i + 1]);

    return mesh;
}

// ---------------------------------------------------------------------------
// Flat square on the XZ plane, facing up. Used for the 64 board squares.
// ---------------------------------------------------------------------------
Mesh make_flat_square(float size)
{
    Mesh mesh;

    const float h = size * 0.5f;
    const math3d::Vec3 up(0.0f, 1.0f, 0.0f);

    const unsigned int base = static_cast<unsigned int>(mesh.vertices.size() / 9);

    add_vertex(mesh, math3d::Vec3(-h, 0.0f,  h), up, WHITE_COLOUR);
    add_vertex(mesh, math3d::Vec3(-h, 0.0f, -h), up, WHITE_COLOUR);
    add_vertex(mesh, math3d::Vec3( h, 0.0f, -h), up, WHITE_COLOUR);
    add_vertex(mesh, math3d::Vec3( h, 0.0f,  h), up, WHITE_COLOUR);

    add_triangle(mesh, base, base + 1, base + 2);
    add_triangle(mesh, base, base + 2, base + 3);

    return mesh;
}

// ---------------------------------------------------------------------------
// Mesh helpers.
// ---------------------------------------------------------------------------
void set_mesh_colour(Mesh& mesh, const math3d::Vec3& colour)
{
    // The colour of each vertex is the last 3 floats of its 9-float block.
    for (size_t i = 0; i + 2 < mesh.vertices.size(); i += 9)
    {
        mesh.vertices[i + 6] = colour.x;
        mesh.vertices[i + 7] = colour.y;
        mesh.vertices[i + 8] = colour.z;
    }
}

Mesh transform_mesh(const Mesh& source, const math3d::Vec3& offset,
                    float uniform_scale, const math3d::Vec3& new_colour)
{
    Mesh result = source;

    for (size_t i = 0; i + 8 < result.vertices.size(); i += 9)
    {
        // Position: scale then move.
        result.vertices[i + 0] = result.vertices[i + 0] * uniform_scale + offset.x;
        result.vertices[i + 1] = result.vertices[i + 1] * uniform_scale + offset.y;
        result.vertices[i + 2] = result.vertices[i + 2] * uniform_scale + offset.z;

        // Normal: uniform scaling does not change the direction of a normal,
        // so only the offset matters (and it does not affect normals at all).
        // We still normalise in case the source normal was not unit length.
        float nx = result.vertices[i + 3];
        float ny = result.vertices[i + 4];
        float nz = result.vertices[i + 5];
        float length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (length > 1e-6f)
        {
            result.vertices[i + 3] = nx / length;
            result.vertices[i + 4] = ny / length;
            result.vertices[i + 5] = nz / length;
        }

        // Colour.
        result.vertices[i + 6] = new_colour.x;
        result.vertices[i + 7] = new_colour.y;
        result.vertices[i + 8] = new_colour.z;
    }

    return result;
}

Mesh merge_meshes(const std::vector<Mesh>& parts)
{
    Mesh result;

    for (const Mesh& part : parts)
    {
        unsigned int offset = static_cast<unsigned int>(result.vertices.size() / 9);

        result.vertices.insert(result.vertices.end(), part.vertices.begin(), part.vertices.end());

        for (unsigned int index : part.indices)
            result.indices.push_back(index + offset);
    }

    return result;
}

// ---------------------------------------------------------------------------
// The chess pieces.
//
// Each piece is a short stack of primitives. The heights are chosen so that
// the pieces are visually distinguishable at a glance:
//     pawn   ~0.45   (shortest)
//     knight ~0.55
//     bishop ~0.65
//     rook   ~0.55
//     queen  ~0.75
//     king   ~0.85   (tallest)
//
// We use a slightly darker "base" colour for the flat parts so the pieces
// read well against the board.
// ---------------------------------------------------------------------------
namespace
{

// The colour used for the flat parts (bases) of the pieces.
math3d::Vec3 shade(const math3d::Vec3& colour, float factor)
{
    return math3d::Vec3(colour.x * factor, colour.y * factor, colour.z * factor);
}

} // namespace

// --- Pawn: a small cone on a disc, topped with a sphere. ---
Mesh make_pawn(const math3d::Vec3& colour)
{
    std::vector<Mesh> parts;

    parts.push_back(transform_mesh(make_cylinder(0.16f, 0.04f, 20),
                                   math3d::Vec3(0.0f, 0.0f, 0.0f), 1.0f,
                                   shade(colour, 0.75f)));
    parts.push_back(transform_mesh(make_cone(0.13f, 0.26f, 20),
                                   math3d::Vec3(0.0f, 0.04f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_sphere(0.085f, 20, 14),
                                   math3d::Vec3(0.0f, 0.32f, 0.0f), 1.0f, colour));

    return merge_meshes(parts);
}

// --- Rook: a cylinder with a wider "crenellated" top. ---
Mesh make_rook(const math3d::Vec3& colour)
{
    std::vector<Mesh> parts;

    parts.push_back(transform_mesh(make_cylinder(0.18f, 0.05f, 20),
                                   math3d::Vec3(0.0f, 0.0f, 0.0f), 1.0f,
                                   shade(colour, 0.75f)));
    parts.push_back(transform_mesh(make_cylinder(0.11f, 0.34f, 20),
                                   math3d::Vec3(0.0f, 0.05f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_cylinder(0.16f, 0.10f, 20),
                                   math3d::Vec3(0.0f, 0.39f, 0.0f), 1.0f, colour));

    // Four small blocks around the top to suggest battlements.
    const float offset = 0.115f;
    const float top_y  = 0.49f;
    for (int i = 0; i < 4; ++i)
    {
        float angle = 3.14159265f * 0.5f * static_cast<float>(i);
        float x = std::cos(angle) * offset;
        float z = std::sin(angle) * offset;

        parts.push_back(transform_mesh(make_cube(0.075f),
                                       math3d::Vec3(x, top_y, z), 1.0f, colour));
    }

    return merge_meshes(parts);
}

// --- Knight: a stylised horse head. ---
//
// A real sculpted horse is far too complex for this project, so the knight is
// built from a few simple shapes: a base, a slanted body, a box for the snout
// and a cone for the ear.
Mesh make_knight(const math3d::Vec3& colour)
{
    std::vector<Mesh> parts;

    // Base.
    parts.push_back(transform_mesh(make_cylinder(0.17f, 0.05f, 20),
                                   math3d::Vec3(0.0f, 0.0f, 0.0f), 1.0f,
                                   shade(colour, 0.75f)));

    // The head leans forward slightly. We build it upright and then shift it.
    Mesh head = make_cone(0.13f, 0.30f, 20);
    // Squeeze it a little so it is not a perfect cone.
    for (size_t i = 0; i + 8 < head.vertices.size(); i += 9)
    {
        head.vertices[i + 0] *= 0.85f;   // narrow in X
        head.vertices[i + 2] *= 1.25f;   // longer in Z (the horse's length)
    }
    parts.push_back(transform_mesh(head, math3d::Vec3(0.0f, 0.05f, 0.0f), 1.0f, colour));

    // Snout: a small box at the front.
    parts.push_back(transform_mesh(make_cube(0.10f),
                                   math3d::Vec3(0.0f, 0.30f, 0.11f), 1.0f, colour));

    // Ear: a small cone on top.
    parts.push_back(transform_mesh(make_cone(0.035f, 0.09f, 12),
                                   math3d::Vec3(0.0f, 0.36f, -0.03f), 1.0f, colour));

    // Mane: a thin flattened box along the back.
    parts.push_back(transform_mesh(make_cube(0.05f),
                                   math3d::Vec3(0.0f, 0.22f, -0.10f), 1.0f,
                                   shade(colour, 0.70f)));

    return merge_meshes(parts);
}

// --- Bishop: a cone body with a sphere on top and a small point. ---
Mesh make_bishop(const math3d::Vec3& colour)
{
    std::vector<Mesh> parts;

    parts.push_back(transform_mesh(make_cylinder(0.16f, 0.05f, 20),
                                   math3d::Vec3(0.0f, 0.0f, 0.0f), 1.0f,
                                   shade(colour, 0.75f)));
    parts.push_back(transform_mesh(make_cylinder(0.10f, 0.06f, 20),
                                   math3d::Vec3(0.0f, 0.05f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_cone(0.115f, 0.38f, 20),
                                   math3d::Vec3(0.0f, 0.11f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_sphere(0.075f, 20, 14),
                                   math3d::Vec3(0.0f, 0.49f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_cone(0.032f, 0.10f, 12),
                                   math3d::Vec3(0.0f, 0.54f, 0.0f), 1.0f, colour));

    return merge_meshes(parts);
}

// --- Queen: the tallest piece with a crown. ---
Mesh make_queen(const math3d::Vec3& colour)
{
    std::vector<Mesh> parts;

    parts.push_back(transform_mesh(make_cylinder(0.18f, 0.05f, 22),
                                   math3d::Vec3(0.0f, 0.0f, 0.0f), 1.0f,
                                   shade(colour, 0.75f)));
    parts.push_back(transform_mesh(make_cylinder(0.11f, 0.06f, 22),
                                   math3d::Vec3(0.0f, 0.05f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_cone(0.14f, 0.46f, 22),
                                   math3d::Vec3(0.0f, 0.11f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_sphere(0.085f, 20, 14),
                                   math3d::Vec3(0.0f, 0.57f, 0.0f), 1.0f, colour));

    // A ring of small spheres forms the crown.
    const int count = 6;
    const float ring_radius = 0.10f;
    for (int i = 0; i < count; ++i)
    {
        float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(count);
        parts.push_back(transform_mesh(make_sphere(0.028f, 12, 8),
                                       math3d::Vec3(std::cos(angle) * ring_radius, 0.66f,
                                                    std::sin(angle) * ring_radius),
                                       1.0f, colour));
    }

    return merge_meshes(parts);
}

// --- King: the tallest piece with a cross on top. ---
Mesh make_king(const math3d::Vec3& colour)
{
    std::vector<Mesh> parts;

    parts.push_back(transform_mesh(make_cylinder(0.19f, 0.05f, 22),
                                   math3d::Vec3(0.0f, 0.0f, 0.0f), 1.0f,
                                   shade(colour, 0.75f)));
    parts.push_back(transform_mesh(make_cylinder(0.12f, 0.06f, 22),
                                   math3d::Vec3(0.0f, 0.05f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_cone(0.145f, 0.52f, 22),
                                   math3d::Vec3(0.0f, 0.11f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_sphere(0.09f, 20, 14),
                                   math3d::Vec3(0.0f, 0.63f, 0.0f), 1.0f, colour));

    // The cross on top: one vertical bar and one horizontal bar.
    parts.push_back(transform_mesh(make_cube(0.035f),
                                   math3d::Vec3(0.0f, 0.78f, 0.0f), 1.0f, colour));
    parts.push_back(transform_mesh(make_cube(0.11f),
                                   math3d::Vec3(0.0f, 0.80f, 0.0f), 1.0f, colour));

    return merge_meshes(parts);
}

// ---------------------------------------------------------------------------
// Builds the mesh for any piece type.
// ---------------------------------------------------------------------------
Mesh make_piece(PieceType type, const math3d::Vec3& colour)
{
    switch (type)
    {
    case PAWN:   return make_pawn(colour);
    case KNIGHT: return make_knight(colour);
    case BISHOP: return make_bishop(colour);
    case ROOK:   return make_rook(colour);
    case QUEEN:  return make_queen(colour);
    case KING:   return make_king(colour);
    default:     return make_pawn(colour);
    }
}
