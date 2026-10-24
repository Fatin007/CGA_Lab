// geometry.hpp -- The 3D shapes for the solar system.
//
// There are no model files (.obj) in this project, so every shape is built
// in code.
//
// The vertices are stored interleaved as:
//     position (3 floats), normal (3 floats), uv (2 floats)
// which is exactly the layout the vertex shader expects.
//
// The UV coordinates are needed because the planet shader uses them to paint
// the surface bands.

#pragma once

#include <vector>

// A piece of geometry: the vertices, and the indices that say how to join
// them into triangles.
struct Mesh
{
    std::vector<float>        vertices;   // pos(3), normal(3), uv(2)
    std::vector<unsigned int> indices;
};

// ---------------------------------------------------------------------------
// A UV sphere of the given radius, centred on the origin.
//
// "segments" is the number of steps around the equator (like the segments of
// an orange), and "rings" is the number of steps from the north pole to the
// south pole.
//
// The sphere is built with a duplicated seam column, because the UV
// coordinate wraps from 1.0 back to 0.0. Without the duplicate there would
// be a visible seam where the texture wraps.
// ---------------------------------------------------------------------------
Mesh make_sphere(float radius, int segments = 64, int rings = 32);

// ---------------------------------------------------------------------------
// A flat annulus (a ring with a hole in the middle) in the XZ plane.
//
// This is the shape of Saturn's rings. The UV coordinate runs from 0 at the
// inner edge to 1 at the outer edge, so the fragment shader can draw the
// bands and the gaps.
//
// The vertices use the same layout as the sphere, so the same vertex
// attribute setup works for both.
// ---------------------------------------------------------------------------
Mesh make_ring(float inner_radius, float outer_radius, int segments = 96);

// ---------------------------------------------------------------------------
// A ring of points in the XZ plane -- one full orbit.
//
// The points are drawn as a GL_LINE_STRIP to show the path a planet takes.
// ---------------------------------------------------------------------------
std::vector<float> make_orbit_points(float radius, int segments = 128);

// ---------------------------------------------------------------------------
// A flat square in the XY plane, centred on the origin.
//
// This is the sun's corona -- a billboard that we always turn to face the
// camera. The UV coordinate runs from 0 at the bottom-left corner to 1 at
// the top-right one.
//
// The vertices use the same layout as the sphere (position, normal, UV) so
// the same vertex attribute setup works for it too.
// ---------------------------------------------------------------------------
Mesh make_quad(float size = 1.0f);
