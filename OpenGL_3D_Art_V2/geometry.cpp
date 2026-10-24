// geometry.cpp -- Building the shapes.

#include "geometry.hpp"

#include <cmath>

namespace
{
    constexpr float PI = 3.14159265f;
}

// ---------------------------------------------------------------------------
// A UV sphere.
//
// We walk down the sphere one ring at a time. At the top (the north pole)
// all the points in a ring are at the same place, and the same is true at
// the bottom. In between, each ring sits at a different latitude.
//
// For each ring we work out:
//   * the height   (y) -- from +radius at the pole down to -radius
//   * the radius of the circle at that height
//
// A point on that circle is then:
//   x = circle_radius * cos(angle)
//   y = height
//   z = circle_radius * sin(angle)
//
// The normal of a sphere at a point is simply the direction from the centre
// to that point, which is why the normal is just the position divided by the
// radius.
// ---------------------------------------------------------------------------
Mesh make_sphere(float radius, int segments, int rings)
{
    Mesh mesh;

    if (segments < 3) segments = 3;
    if (rings < 2) rings = 2;

    for (int ring = 0; ring <= rings; ++ring)
    {
        // v goes from 0 at the north pole to 1 at the south pole.
        const float v = static_cast<float>(ring) / static_cast<float>(rings);

        // The polar angle: 0 at the north pole, PI at the south pole.
        const float phi = v * PI;

        const float y = radius * std::cos(phi);
        const float circle_radius = radius * std::sin(phi);

        for (int segment = 0; segment <= segments; ++segment)
        {
            // u goes from 0 to 1 as we go around the equator. We run the
            // loop one step past the end so the last column repeats the
            // first one with u = 1.0. That closes the seam smoothly.
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = u * 2.0f * PI;

            const float x = circle_radius * std::cos(theta);
            const float z = circle_radius * std::sin(theta);

            // Position.
            mesh.vertices.push_back(x);
            mesh.vertices.push_back(y);
            mesh.vertices.push_back(z);

            // Normal. On a sphere the normal points straight out from the
            // centre, so it is the position scaled back to length 1.
            mesh.vertices.push_back(x / radius);
            mesh.vertices.push_back(y / radius);
            mesh.vertices.push_back(z / radius);

            // UV coordinate.
            mesh.vertices.push_back(u);
            mesh.vertices.push_back(v);
        }
    }

    // --- Indices ---
    //
    // Each quad is made of two triangles. We walk over the grid of points
    // and connect each block of four.
    const int row_size = segments + 1;

    for (int ring = 0; ring < rings; ++ring)
    {
        for (int segment = 0; segment < segments; ++segment)
        {
            const unsigned int top_left     = static_cast<unsigned int>(ring * row_size + segment);
            const unsigned int top_right    = static_cast<unsigned int>(ring * row_size + segment + 1);
            const unsigned int bottom_left  = static_cast<unsigned int>((ring + 1) * row_size + segment);
            const unsigned int bottom_right = static_cast<unsigned int>((ring + 1) * row_size + segment + 1);

            // First triangle: top-left, bottom-left, top-right.
            mesh.indices.push_back(top_left);
            mesh.indices.push_back(bottom_left);
            mesh.indices.push_back(top_right);

            // Second triangle: top-right, bottom-left, bottom-right.
            mesh.indices.push_back(top_right);
            mesh.indices.push_back(bottom_left);
            mesh.indices.push_back(bottom_right);
        }
    }

    return mesh;
}

// ---------------------------------------------------------------------------
// A flat annulus in the XZ plane.
//
// We build two rings of points -- one at the inner radius and one at the
// outer radius -- and then join them up with a strip of quads.
//
// The normal points straight up (along +Y), because the ring is flat.
//
// The UV coordinate is 0 at the inner edge and 1 at the outer edge, which is
// how the shader knows where the Cassini division goes.
// ---------------------------------------------------------------------------
Mesh make_ring(float inner_radius, float outer_radius, int segments)
{
    Mesh mesh;

    if (segments < 3) segments = 3;

    for (int i = 0; i <= segments; ++i)
    {
        const float angle = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * PI;
        const float cosine = std::cos(angle);
        const float sine   = std::sin(angle);

        // --- The inner edge ---
        mesh.vertices.push_back(inner_radius * cosine);
        mesh.vertices.push_back(0.0f);
        mesh.vertices.push_back(inner_radius * sine);

        mesh.vertices.push_back(0.0f);   // normal, straight up
        mesh.vertices.push_back(1.0f);
        mesh.vertices.push_back(0.0f);

        mesh.vertices.push_back(0.0f);   // u = 0 at the inner edge
        mesh.vertices.push_back(0.0f);   // v is unused, but must be present

        // --- The outer edge ---
        mesh.vertices.push_back(outer_radius * cosine);
        mesh.vertices.push_back(0.0f);
        mesh.vertices.push_back(outer_radius * sine);

        mesh.vertices.push_back(0.0f);
        mesh.vertices.push_back(1.0f);
        mesh.vertices.push_back(0.0f);

        mesh.vertices.push_back(1.0f);   // u = 1 at the outer edge
        mesh.vertices.push_back(0.0f);
    }

    // --- Indices ---
    //
    // Each segment is a quad made of two triangles. The points come in
    // pairs (inner, outer), so the inner point of segment i is at
    // index i * 2 and the outer one at i * 2 + 1.
    for (int i = 0; i < segments; ++i)
    {
        const unsigned int inner_left  = static_cast<unsigned int>(i * 2);
        const unsigned int outer_left  = static_cast<unsigned int>(i * 2 + 1);
        const unsigned int inner_right = static_cast<unsigned int>((i + 1) * 2);
        const unsigned int outer_right = static_cast<unsigned int>((i + 1) * 2 + 1);

        mesh.indices.push_back(inner_left);
        mesh.indices.push_back(outer_left);
        mesh.indices.push_back(inner_right);

        mesh.indices.push_back(inner_right);
        mesh.indices.push_back(outer_left);
        mesh.indices.push_back(outer_right);
    }

    return mesh;
}

// ---------------------------------------------------------------------------
// The points of one circular orbit, in the XZ plane.
//
// The Y coordinate is 0, because the orbits all lie in the plane of the
// ecliptic (the flat plane the planets travel around).
// ---------------------------------------------------------------------------
std::vector<float> make_orbit_points(float radius, int segments)
{
    std::vector<float> points;

    for (int i = 0; i <= segments; ++i)
    {
        const float angle = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * PI;

        points.push_back(radius * std::cos(angle));
        points.push_back(0.0f);
        points.push_back(radius * std::sin(angle));
    }

    return points;
}

// ---------------------------------------------------------------------------
// A flat square in the XY plane.
//
// The four corners are in order: bottom-left, bottom-right, top-right,
// top-left. That order makes a counter-clockwise loop when seen from the
// +Z side, which is the side we look at it from.
//
// The normal points along +Z, and the UV coordinate runs 0 to 1 across the
// square. The sun shader uses the UV to work out how far each fragment is
// from the middle of the corona.
// ---------------------------------------------------------------------------
Mesh make_quad(float size)
{
    Mesh mesh;

    const float half = size * 0.5f;

    // The four corners.
    const float corners[4][3] = {
        { -half, -half, 0.0f },
        {  half, -half, 0.0f },
        {  half,  half, 0.0f },
        { -half,  half, 0.0f }
    };

    // The UV coordinate of each corner.
    const float uvs[4][2] = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f }
    };

    for (int i = 0; i < 4; ++i)
    {
        mesh.vertices.push_back(corners[i][0]);
        mesh.vertices.push_back(corners[i][1]);
        mesh.vertices.push_back(corners[i][2]);

        mesh.vertices.push_back(0.0f);   // normal, straight out of the quad
        mesh.vertices.push_back(0.0f);
        mesh.vertices.push_back(1.0f);

        mesh.vertices.push_back(uvs[i][0]);
        mesh.vertices.push_back(uvs[i][1]);
    }

    // Two triangles make the square.
    mesh.indices.push_back(0);
    mesh.indices.push_back(1);
    mesh.indices.push_back(2);

    mesh.indices.push_back(0);
    mesh.indices.push_back(2);
    mesh.indices.push_back(3);

    return mesh;
}
