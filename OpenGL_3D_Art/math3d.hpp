// math3d.hpp -- Minimal 3D math library for the solar system project.
//
// We write our own instead of using GLM because GLM is not installed on this
// machine, and writing it by hand is also something we can explain to faculty:
// "we implemented the vector and matrix operations the project needs".
//
// Conventions used throughout the project:
//   * Mat4 stores 16 floats in COLUMN-MAJOR order, exactly like OpenGL expects.
//     Element m[c * 4 + r] means "column c, row r".
//   * Matrices are applied to vectors as  M * v  (matrix on the left).
//   * The right-handed coordinate system is used: +X right, +Y up, +Z toward viewer.

#pragma once

#include <cmath>
#include <algorithm>

namespace math3d
{

// ---------------------------------------------------------------------------
// Vec3 -- a 3-component vector.
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
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }

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
        float len = length();
        // Guard against dividing by zero when the vector has no direction.
        if (len < 1e-8f) return Vec3(0.0f, 0.0f, 0.0f);
        return *this / len;
    }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

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

    static Mat4 identity() { return Mat4(); }

    // Element access using (row, column) for readability at call sites.
    float& at(int row, int col) { return m[col * 4 + row]; }
    const float& at(int row, int col) const { return m[col * 4 + row]; }
};

// Matrix multiplication: result = a * b  (apply b first, then a).
inline Mat4 multiply(const Mat4& a, const Mat4& b)
{
    Mat4 out;
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
                sum += a.at(r, k) * b.at(k, c);
            out.at(r, c) = sum;
        }
    }
    return out;
}

// Transform a point (w = 1) by a matrix.
inline Vec3 transform_point(const Mat4& m, const Vec3& p)
{
    return Vec3(
        m.at(0, 0) * p.x + m.at(0, 1) * p.y + m.at(0, 2) * p.z + m.at(0, 3),
        m.at(1, 0) * p.x + m.at(1, 1) * p.y + m.at(1, 2) * p.z + m.at(1, 3),
        m.at(2, 0) * p.x + m.at(2, 1) * p.y + m.at(2, 2) * p.z + m.at(2, 3));
}

// A 4-component vector, needed when projecting points to screen coordinates
// because the perspective divide uses the w component.
struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// Transform a point by a matrix, keeping the w component.
//
// This is what we use to project a world position into clip space.
inline Vec4 transform_vec4(const Mat4& m, const Vec3& p)
{
    return Vec4(
        m.at(0, 0) * p.x + m.at(0, 1) * p.y + m.at(0, 2) * p.z + m.at(0, 3),
        m.at(1, 0) * p.x + m.at(1, 1) * p.y + m.at(1, 2) * p.z + m.at(1, 3),
        m.at(2, 0) * p.x + m.at(2, 1) * p.y + m.at(2, 2) * p.z + m.at(2, 3),
        m.at(3, 0) * p.x + m.at(3, 1) * p.y + m.at(3, 2) * p.z + m.at(3, 3));
}

// Project a world position into window pixel coordinates.
//
// This is the "forward" direction of picking: given a place on the board,
// where does it appear on the screen?
inline bool project_to_screen(const Mat4& projection, const Mat4& view,
                              const Vec3& world,
                              int window_width, int window_height,
                              float& screen_x, float& screen_y)
{
    // World -> camera space.
    const Vec4 camera_space = transform_vec4(view, world);

    // Camera space -> clip space.
    const Vec4 clip = transform_vec4(projection,
                                      Vec3(camera_space.x, camera_space.y, camera_space.z));

    // Points behind the camera have w <= 0 and cannot be projected.
    if (clip.w <= 1e-6f)
        return false;

    // Perspective divide, then from normalised device coordinates (-1..1)
    // to window pixels.
    const float ndc_x = clip.x / clip.w;
    const float ndc_y = clip.y / clip.w;

    screen_x = (ndc_x * 0.5f + 0.5f) * static_cast<float>(window_width);
    screen_y = (1.0f - (ndc_y * 0.5f + 0.5f)) * static_cast<float>(window_height);

    return true;
}

// Translation matrix.
inline Mat4 translate(float x, float y, float z)
{
    Mat4 out;
    out.at(0, 3) = x;
    out.at(1, 3) = y;
    out.at(2, 3) = z;
    return out;
}

// Uniform scale matrix.
//
// IMPORTANT: we only ever scale uniformly. The fragment shader transforms
// normals with mat3(uModel), which is only correct when the same factor is
// applied to every axis.
inline Mat4 scale(float s)
{
    Mat4 out;
    out.at(0, 0) = s;
    out.at(1, 1) = s;
    out.at(2, 2) = s;
    return out;
}

// Rotation about the X axis, by an angle in RADIANS.
//
// This is the axis that points "sideways" (east to west), so it is what we
// use for a planet's axial tilt -- leaning the north pole towards or away
// from the sun.
inline Mat4 rotate_x(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    Mat4 out;
    out.at(1, 1) =  c;  out.at(1, 2) = -s;
    out.at(2, 1) =  s;  out.at(2, 2) =  c;
    return out;
}

// Rotation about the Y axis, by an angle in RADIANS.
//
// The Y axis is the "up" direction, so this is the axis every planet spins
// around: one full turn of this matrix is one rotation period.
inline Mat4 rotate_y(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    Mat4 out;
    out.at(0, 0) =  c;  out.at(0, 2) =  s;
    out.at(2, 0) = -s;  out.at(2, 2) =  c;
    return out;
}

// Rotation about the Z axis, by an angle in RADIANS.
inline Mat4 rotate_z(float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    Mat4 out;
    out.at(0, 0) =  c;  out.at(0, 1) = -s;
    out.at(1, 0) =  s;  out.at(1, 1) =  c;
    return out;
}

// Build a view matrix from an eye position, a look-at target and an up vector.
//
// The camera always looks AT the target, so the view matrix is simply the
// inverse of the camera's world transform. We compute it directly with the
// classic "right / up / forward" basis, which is easier to follow than a
// general matrix inverse.
inline Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up)
{
    Vec3 forward = (target - eye).normalized();
    Vec3 right   = forward.cross(up).normalized();
    Vec3 true_up = right.cross(forward);

    Mat4 out;
    out.at(0, 0) = right.x;   out.at(0, 1) = right.y;   out.at(0, 2) = right.z;   out.at(0, 3) = -right.dot(eye);
    out.at(1, 0) = true_up.x; out.at(1, 1) = true_up.y; out.at(1, 2) = true_up.z; out.at(1, 3) = -true_up.dot(eye);
    out.at(2, 0) = -forward.x; out.at(2, 1) = -forward.y; out.at(2, 2) = -forward.z; out.at(2, 3) = forward.dot(eye);
    out.at(3, 0) = 0.0f;       out.at(3, 1) = 0.0f;       out.at(3, 2) = 0.0f;       out.at(3, 3) = 1.0f;
    return out;
}

// Build a perspective projection matrix (field of view in degrees).
inline Mat4 perspective(float fov_y_degrees, float aspect, float near_plane, float far_plane)
{
    float f = 1.0f / std::tan(fov_y_degrees * 3.14159265f / 180.0f * 0.5f);

    Mat4 out;                       // start as identity
    out.at(0, 0) = f / aspect;
    out.at(1, 1) = f;
    out.at(2, 2) = (far_plane + near_plane) / (near_plane - far_plane);
    out.at(2, 3) = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    out.at(3, 2) = -1.0f;
    out.at(3, 3) = 0.0f;
    return out;
}

// General 4x4 matrix inverse.
//
// We need this for mouse picking: to turn a cursor position into a 3D ray we
// "unproject" a point by multiplying by the inverse of (projection * view).
//
// The implementation uses Gauss-Jordan elimination. We build a 4x8 matrix
// with our matrix on the left and the identity on the right, then use row
// operations to turn the left half into the identity. What is left on the
// right half is the inverse.
//
// This is slower than the closed-form cofactor formula, but it is much easier
// to get right and much easier to explain.
inline Mat4 inverse(const Mat4& src)
{
    // The augmented matrix: 4 rows, 8 columns.
    // Left half (columns 0-3) is the matrix we want to invert.
    // Right half (columns 4-7) starts out as the identity.
    float a[4][8];

    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
            a[row][col] = src.at(row, col);

        for (int col = 0; col < 4; ++col)
            a[row][col + 4] = (row == col) ? 1.0f : 0.0f;
    }

    // For each column we pick the row with the largest value in that column
    // (partial pivoting keeps the numbers well behaved), then use that row
    // to clear the column.
    for (int col = 0; col < 4; ++col)
    {
        // --- Pivot ---
        int pivot_row = col;
        for (int row = col + 1; row < 4; ++row)
        {
            if (std::fabs(a[row][col]) > std::fabs(a[pivot_row][col]))
                pivot_row = row;
        }

        if (std::fabs(a[pivot_row][col]) < 1e-12f)
            return Mat4::identity();   // singular matrix: nothing sensible to do

        // Swap the pivot row into place.
        if (pivot_row != col)
        {
            for (int k = 0; k < 8; ++k)
                std::swap(a[col][k], a[pivot_row][k]);
        }

        // --- Normalise the pivot row so the pivot becomes 1 ---
        float pivot = a[col][col];
        for (int k = 0; k < 8; ++k)
            a[col][k] /= pivot;

        // --- Clear the column above and below the pivot ---
        for (int row = 0; row < 4; ++row)
        {
            if (row == col) continue;

            float factor = a[row][col];
            if (factor == 0.0f) continue;

            for (int k = 0; k < 8; ++k)
                a[row][k] -= factor * a[col][k];
        }
    }

    // The left half is now the identity, so the right half is the inverse.
    Mat4 out;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            out.at(row, col) = a[row][col + 4];

    return out;
}

// Unproject a screen pixel back into the world.
//
// This is the "backward" direction of picking: given a place on the screen,
// where in the world did the cursor point?
//
// We take the pixel, turn it into normalised device coordinates, put it into
// clip space, and multiply by the inverse of (projection * view). The result
// is a point in clip space with a w component, so we must divide by w to get
// back to ordinary 3D coordinates. That division is the perspective divide,
// and skipping it is the classic picking bug: the ray then always points at
// the centre of the screen.
//
// ndc_depth chooses how far along the view direction the point is:
//   -1 = the near plane, +1 = the far plane.
//
// Returns false if the point is behind the camera and cannot be unprojected.
inline bool unproject_from_screen(const Mat4& projection, const Mat4& view,
                                  int window_width, int window_height,
                                  float screen_x, float screen_y,
                                  float ndc_depth,
                                  Vec3& world)
{
    // Pixel -> normalised device coordinates (-1 .. +1).
    const float ndc_x = (screen_x / static_cast<float>(window_width)) * 2.0f - 1.0f;
    const float ndc_y = 1.0f - (screen_y / static_cast<float>(window_height)) * 2.0f;

    // Clip space -> world space, undoing the perspective divide.
    const Mat4 inverse_view_projection = inverse(multiply(projection, view));

    const Vec4 clip = transform_vec4(inverse_view_projection,
                                     Vec3(ndc_x, ndc_y, ndc_depth));

    // w <= 0 means the point is behind the camera (or on the eye plane),
    // where the projection is not invertible.
    if (clip.w <= 1e-6f)
        return false;

    world = Vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
    return true;
}

// Convert radians to degrees and back (handy for camera angles).
inline float to_radians(float degrees) { return degrees * 3.14159265f / 180.0f; }
inline float to_degrees(float radians) { return radians * 180.0f / 3.14159265f; }

} // namespace math3d
