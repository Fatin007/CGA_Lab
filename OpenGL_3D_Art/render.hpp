// render.hpp -- The camera, the OpenGL objects and the drawing code.
//
// The renderer owns all the GPU-side state: the vertex arrays, the shader
// programs and the buffers. The main loop only has to tell it "draw the
// scene from this camera at this time".

#pragma once

#include "math3d.hpp"
#include "solar_system.hpp"
#include "shader.hpp"

#include <vector>

// ---------------------------------------------------------------------------
// The camera.
//
// The camera orbits around the sun. Instead of moving the camera itself and
// then working out where it is, we keep three simple numbers and rebuild the
// view matrix from them every frame:
//
//   distance -- how far away from the sun
//   yaw      -- how far round, in degrees
//   pitch    -- how far up or down, in degrees
//
// This is the same idea as the chess project's camera, and it is the easiest
// way to get a camera that feels good to control.
// ---------------------------------------------------------------------------
struct Camera
{
    // The starting view. We start far enough out to see most of the solar
    // system -- Neptune's orbit is about 49 units away, so 95 units puts the
    // whole thing comfortably in frame.
    float distance = 95.0f;
    float yaw      = 35.0f;
    float pitch    = 28.0f;

    // The limits. The pitch is clamped so the camera never goes directly
    // above or below the sun, where the view would look strange.
    //
    // The lower limit is -18 degrees rather than -85 for a good reason. At
    // a shallow angle the orbits flatten into overlapping bands, and the
    // planets that are further out pile up on top of the ones nearer the
    // sun. Venus ends up looking as if it is touching Earth, and Jupiter
    // as if it is sitting in the asteroid belt. Both are illusions caused
    // by the viewing angle, but they are very confusing ones, so we simply
    // do not let the camera get that low.
    static constexpr float MIN_DISTANCE = 3.0f;
    static constexpr float MAX_DISTANCE = 400.0f;
    static constexpr float MIN_PITCH     = -18.0f;
    static constexpr float MAX_PITCH     =  85.0f;

    // Where the camera actually is, in world space.
    math3d::Vec3 eye_position() const;

    // The matrix that turns world space into camera space.
    math3d::Mat4 view_matrix() const;

    // The point the camera looks at. Usually the sun at the origin, but we
    // allow it to follow a planet.
    math3d::Vec3 target = math3d::Vec3(0.0f, 0.0f, 0.0f);
};

// ---------------------------------------------------------------------------
// One planet's GPU data.
// ---------------------------------------------------------------------------
struct PlanetGpu
{
    unsigned int vao = 0;              // the vertex array
    unsigned int vbo = 0;              // the vertex buffer
    unsigned int ebo = 0;              // the index buffer
    unsigned int index_count = 0;      // how many indices to draw
};

// ---------------------------------------------------------------------------
// The renderer.
// ---------------------------------------------------------------------------
class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    // Non-copyable: it owns GL objects.
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Creates the shaders and all the geometry.
    // Returns false if something went wrong (the error is printed).
    bool initialise();

    // Draws one frame.
    void draw(const SolarSystem& system, const Camera& camera,
              float time_seconds, int window_width, int window_height);

    // The shader programs, so the main loop can check they loaded.
    bool shaders_ok() const { return shaders_ok_; }

private:
    // --- Helper functions ---

    // Creates a VAO, VBO and EBO for a mesh and stores them.
    void create_mesh(const struct Mesh& mesh,
                     unsigned int& vao, unsigned int& vbo,
                     unsigned int& ebo, unsigned int& index_count);

    // Draws one planet.
    void draw_planet(const Planet& planet, const SolarSystem& system,
                     const math3d::Mat4& view, const math3d::Mat4& projection,
                     const math3d::Vec3& eye, float time);

    // Draws the sun and its corona.
    void draw_sun(const SolarSystem& system,
                  const math3d::Mat4& view, const math3d::Mat4& projection,
                  const math3d::Vec3& eye, float time);

    // Draws the background stars.
    void draw_stars(const math3d::Mat4& view, const math3d::Mat4& projection, float time);

    // Draws the asteroid belt.
    void draw_asteroids(const math3d::Mat4& view, const math3d::Mat4& projection,
                        float time);

    // Draws the orbit lines.
    void draw_orbits(const SolarSystem& system,
                     const math3d::Mat4& view, const math3d::Mat4& projection,
                     const math3d::Vec3& eye);

    // Draws the rings of a planet that has them.
    void draw_rings(const Planet& planet, const SolarSystem& system,
                    const math3d::Mat4& view, const math3d::Mat4& projection,
                    const math3d::Vec3& eye);

    // --- The shaders ---

    Shader planet_shader_;
    Shader sun_shader_;
    Shader star_shader_;
    Shader ring_shader_;
    Shader line_shader_;
    Shader asteroid_shader_;

    bool shaders_ok_ = false;

    // --- The geometry ---

    // The sun's sphere.
    PlanetGpu sun_;

    // The corona: a flat quad we always turn towards the camera.
    unsigned int corona_vao_ = 0;
    unsigned int corona_vbo_ = 0;
    unsigned int corona_ebo_ = 0;
    unsigned int corona_index_count_ = 0;

    // One sphere per planet. They all share the same shape, so we could use
    // one VAO for all of them -- but the plan is to keep this easy to read,
    // and one VAO per planet is still only 8 draw calls.
    std::vector<PlanetGpu> planets_;

    // The rings, for the planets that have them.
    std::vector<PlanetGpu> rings_;

    // The orbit lines: one VAO per planet.
    std::vector<unsigned int> orbit_vaos_;
    std::vector<unsigned int> orbit_vbos_;
    std::vector<unsigned int> orbit_counts_;

    // The stars: one big buffer of points.
    unsigned int star_vao_ = 0;
    unsigned int star_vbo_ = 0;
    int star_count_ = 0;

    // The asteroid belt: one big buffer of points.
    unsigned int asteroid_vao_ = 0;
    unsigned int asteroid_vbo_ = 0;
    int asteroid_count_ = 0;
};
