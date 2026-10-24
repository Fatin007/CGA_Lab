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
// That is the WHOLE camera. It always looks at the sun, and it can only be
// orbited and zoomed.
// ---------------------------------------------------------------------------
struct Camera
{
    // The starting view. Neptune's orbit is about 49 units away, so 95 units
    // puts the whole solar system comfortably in frame.
    float distance = 95.0f;
    float yaw      = 35.0f;
    float pitch    = 28.0f;

    // The limits.
    //
    // The lower limit is -18 degrees rather than -85 for a good reason. At
    // a shallow angle the orbits flatten into overlapping bands, and the
    // planets that are further out pile up on top of the ones nearer the
    // sun. Venus ends up looking as if it is touching Earth, and Jupiter
    // as if it is sitting in the asteroid belt. Both are illusions caused by
    // the viewing angle, but they are very confusing ones, so we simply do
    // not let the camera get that low.
    static constexpr float MIN_DISTANCE = 3.0f;
    static constexpr float MAX_DISTANCE = 400.0f;
    static constexpr float MIN_PITCH     = -18.0f;
    static constexpr float MAX_PITCH     =  85.0f;

    // Where the camera actually is, in world space.
    math3d::Vec3 eye_position() const;

    // The matrix that turns world space into camera space.
    math3d::Mat4 view_matrix() const;
};

// ---------------------------------------------------------------------------
// One piece of geometry on the GPU.
//
// A VAO (vertex array object) remembers which buffers and which attribute
// settings belong together, so we only have to bind it once per draw.
// ---------------------------------------------------------------------------
struct GpuMesh
{
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    unsigned int index_count = 0;
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

private:
    // --- Setting up ---

    // Each of these builds one part of the scene. They are separate
    // functions so that `initialise()` reads as a short list of steps
    // instead of one enormous block.
    bool load_shaders();
    void build_planets(const SolarSystem& system);
    void build_orbits(const SolarSystem& system);
    void build_stars();
    void build_asteroids(const SolarSystem& system);

    // --- Helper functions ---

    // Creates a VAO, VBO and EBO for a mesh and stores them.
    void create_mesh(const struct Mesh& mesh, GpuMesh& out);

    // Creates a VAO and VBO for a cloud of points (the stars and the
    // asteroids). Each point is position (3), brightness (1), size (1).
    void create_points(const std::vector<float>& data, unsigned int& vao,
                       unsigned int& vbo);

    // The model matrix for a planet: its size, its tilt, its spin and its
    // place on the orbit. The rings use the same matrix, which is why they
    // spin and lean along with the planet.
    math3d::Mat4 planet_model_matrix(const Planet& planet,
                                     const math3d::Vec3& position) const;

    // Draws one planet.
    void draw_planet(const Planet& planet, const SolarSystem& system,
                     const math3d::Mat4& view, const math3d::Mat4& projection,
                     const math3d::Vec3& eye);

    // Draws the sun and its corona.
    void draw_sun(const SolarSystem& system,
                  const math3d::Mat4& view, const math3d::Mat4& projection,
                  const math3d::Vec3& eye, float time);

    // Draws the background stars.
    void draw_stars(const math3d::Mat4& view, const math3d::Mat4& projection,
                    float time);

    // Draws the asteroid belt.
    void draw_asteroids(const math3d::Mat4& view, const math3d::Mat4& projection,
                        float time);

    // Draws the orbit lines.
    void draw_orbits(const SolarSystem& system,
                     const math3d::Mat4& view, const math3d::Mat4& projection);

    // Draws the rings of a planet that has them.
    void draw_rings(const Planet& planet, int index, const SolarSystem& system,
                    const math3d::Mat4& view, const math3d::Mat4& projection,
                    const math3d::Vec3& eye);

    // --- The shaders ---

    Shader planet_shader_;
    Shader sun_shader_;
    Shader ring_shader_;
    Shader line_shader_;
    Shader point_shader_;   // draws both the stars and the asteroids

    // --- The geometry ---

    // ONE sphere, shared by the sun and every planet. They are all the same
    // shape -- the difference between them is only the model matrix and the
    // colours, so there is no point in making eight copies of it.
    GpuMesh sphere_;

    // The corona: a flat quad we always turn towards the camera.
    GpuMesh corona_;

    // The rings. Only the planets that have them get one.
    std::vector<GpuMesh> rings_;

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
