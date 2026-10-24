// geometry.hpp -- Procedural 3D shapes for the solar system.
//
// There are no model files (.obj) in this project, so every shape is built in
// code. The planets are UV spheres: a grid of quads wrapped around the globe.
//
// The vertices are stored interleaved as:
//     position (3 floats), normal (3 floats), uv (2 floats)
// which matches the attribute layout the vertex shader expects.
//
// NOTE: the UV coordinates are needed here, unlike in the chess project.
// The planet shader uses them to paint the surface bands, and to work out
// which way is "up" on the planet when it is tilted.

#pragma once

#include "math3d.hpp"

#include <vector>

struct Mesh
{
    std::vector<float>        vertices;   // interleaved: pos(3) normal(3) uv(2)
    std::vector<unsigned int> indices;

    void clear() { vertices.clear(); indices.clear(); }

    bool empty() const { return indices.empty(); }
};

// ---------------------------------------------------------------------------
// A UV sphere of the given radius, centred on the origin.
//
// "segments" is the number of steps around the equator (like the segments of
// an orange), and "rings" is the number of steps from the north pole to the
// south pole. More of each means a smoother sphere, at the cost of more
// triangles.
//
// The sphere is built with a duplicated seam column, because the UV
// coordinate wraps from 1.0 back to 0.0. Without the duplicate there would
// be a visible seam where the texture wraps.
// ---------------------------------------------------------------------------
Mesh make_sphere(float radius, int segments = 48, int rings = 32);

// ---------------------------------------------------------------------------
// A flat quad in the XY plane, centred on the origin.
//
// We use this for the sun's corona (the glow around it). It is drawn as a
// billboard: every frame we turn it to face the camera, so it always looks
// like a flat disc of light no matter where the camera is.
// ---------------------------------------------------------------------------
Mesh make_quad(float size);

// ---------------------------------------------------------------------------
// A ring of points in the XZ plane -- one full orbit.
//
// The points are drawn as a GL_LINE_STRIP to show the path a planet takes.
// ---------------------------------------------------------------------------
std::vector<float> make_orbit_points(float radius, int segments = 128);

// ---------------------------------------------------------------------------
// A flat annulus (a ring with a hole in the middle) in the XZ plane.
//
// This is the shape of Saturn's rings. The UV coordinate runs from 0 at the
// inner edge to 1 at the outer edge, so the fragment shader can draw the
// bands and the gaps.
//
// The vertices are interleaved as: position (3), normal (3), uv (2) -- the
// same layout as the sphere, so the same vertex attribute setup works.
// ---------------------------------------------------------------------------
Mesh make_ring(float inner_radius, float outer_radius, int segments = 96);
