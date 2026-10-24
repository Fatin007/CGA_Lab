// math3d.hpp -- The small amount of 3D maths this project needs.
//
// We write our own instead of using GLM because GLM is not installed on this
// machine. It is also something we can explain to faculty in one sentence:
// "we implemented the vector and matrix operations the project needs".
//
// Two conventions to remember:
//
//   1. Mat4 holds 16 floats in COLUMN-MAJOR order, exactly as OpenGL wants.
//      The element at row r, column c lives at m[c * 4 + r].
//
//   2. A matrix is applied to a vector as  M * v  -- the matrix is on the
//      LEFT. When we chain matrices we multiply them in the opposite order
//      to how we read them.
//
// Only the operations that the project actually uses are here. There is no
// matrix inverse and no picking code, because this project does not need
// them.

#pragma once

#include <cmath>

namespace math3d
{

// ---------------------------------------------------------------------------
// Vec3 -- a point or a direction in 3D.
// ---------------------------------------------------------------------------
struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }

    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

    Vec3 cross(const Vec3& o) const
    {
        return Vec3(y * o.z - z * o.y,
                    z * o.x - x * o.z,
                    x * o.y - y * o.x);
    }

    float length() const { return std::sqrt(dot(*this)); }

    Vec3 normalized() const
    {
        const float len = length();
        if (len < 1e-8f) return Vec3(0.0f, 0.0f, 0.0f);   // no direction
        return *this / len;
    }
};

// ---------------------------------------------------------------------------
// Mat4 -- a 4x4 transformation matrix, stored column-major.
//
//   | m[0]  m[4]  m[8]   m[12] |
//   | m[1]  m[5]  m[9]   m[13] |
//   | m[2]  m[6]  m[10]  m[14] |
//   | m[3]  m[7]  m[11]  m[15] |
// ---------------------------------------------------------------------------
struct Mat4
{
    float m[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // Read or write one element. We say (row, column) because that is how
    // the matrix is drawn on paper.
    float& at(int row, int col) { return m[col * 4 + row]; }
    const float& at(int row, int col) const { return m[col * 4 + row]; }
};

// Multiply two matrices: the result applies b FIRST, then a.
inline Mat4 multiply(const Mat4& a, const Mat4& b)
{
    Mat4 out;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += a.at(r, k) * b.at(k, c);
            out.at(r, c) = sum;
        }
    return out;
}

// Move a point by a matrix (rotation, scale and translation all at once).
inline Vec3 transform(const Mat4& m, const Vec3& p)
{
    return Vec3(
        m.at(0, 0) * p.x + m.at(0, 1) * p.y + m.at(0, 2) * p.z + m.at(0, 3),
        m.at(1, 0) * p.x + m.at(1, 1) * p.y + m.at(1, 2) * p.z + m.at(1, 3),
        m.at(2, 0) * p.x + m.at(2, 1) * p.y + m.at(2, 2) * p.z + m.at(2, 3));
}

// --- The three transformation matrices we need ---

inline Mat4 translate(float x, float y, float z)
{
    Mat4 out;
    out.at(0, 3) = x;
    out.at(1, 3) = y;
    out.at(2, 3) = z;
    return out;
}

// Uniform scale. We ONLY ever scale uniformly, because the fragment shader
// transforms normals with mat3(uModel), and that is only correct when the
// same factor is applied to every axis.
inline Mat4 scale(float s)
{
    Mat4 out;
    out.at(0, 0) = s;
    out.at(1, 1) = s;
    out.at(2, 2) = s;
    return out;
}

// Rotation about the X axis, in radians.
//
// X is the sideways axis (east to west), so this is what we use for a
// planet's axial tilt -- leaning its north pole towards the sun.
inline Mat4 rotate_x(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    Mat4 out;
    out.at(1, 1) =  c;  out.at(1, 2) = -s;
    out.at(2, 1) =  s;  out.at(2, 2) =  c;
    return out;
}

// Rotation about the Y axis, in radians.
//
// Y is straight up, so this is the axis every planet spins around.
inline Mat4 rotate_y(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    Mat4 out;
    out.at(0, 0) =  c;  out.at(0, 2) =  s;
    out.at(2, 0) = -s;  out.at(2, 2) =  c;
    return out;
}

// --- The two camera matrices ---

// The view matrix: where the camera is, and what it looks at.
//
// We build it directly from three perpendicular directions rather than
// inverting a matrix, which is much easier to follow.
inline Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up)
{
    const Vec3 forward = (target - eye).normalized();
    const Vec3 right   = forward.cross(up).normalized();
    const Vec3 true_up = right.cross(forward);

    Mat4 out;
    out.at(0, 0) = right.x;   out.at(0, 1) = right.y;   out.at(0, 2) = right.z;   out.at(0, 3) = -right.dot(eye);
    out.at(1, 0) = true_up.x; out.at(1, 1) = true_up.y; out.at(1, 2) = true_up.z; out.at(1, 3) = -true_up.dot(eye);
    out.at(2, 0) = -forward.x; out.at(2, 1) = -forward.y; out.at(2, 2) = -forward.z; out.at(2, 3) = forward.dot(eye);
    return out;
}

// The projection matrix: turns 3D into 2D.
//
// It is what gives us perspective -- things further away look smaller.
// The field of view is in DEGREES.
inline Mat4 perspective(float fov_y_degrees, float aspect,
                        float near_plane, float far_plane)
{
    const float f = 1.0f / std::tan(fov_y_degrees * 3.14159265f / 180.0f * 0.5f);

    Mat4 out;                       // starts as the identity
    out.at(0, 0) = f / aspect;
    out.at(1, 1) = f;
    out.at(2, 2) = (far_plane + near_plane) / (near_plane - far_plane);
    out.at(2, 3) = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    out.at(3, 2) = -1.0f;
    out.at(3, 3) = 0.0f;
    return out;
}

// Degrees to radians.
inline float to_radians(float degrees) { return degrees * 3.14159265f / 180.0f; }

} // namespace math3d
