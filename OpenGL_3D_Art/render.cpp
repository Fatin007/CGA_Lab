// render.cpp -- The camera, the OpenGL objects and the drawing code.

#include "render.hpp"
#include "geometry.hpp"

// IMPORTANT: glew.h must be included BEFORE glfw3.h anywhere in the project.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdlib>
#include <random>
#include <iostream>

namespace
{
    // How smooth the planets are.
    //
    // A UV sphere is made of a grid of quads. More segments and more rings
    // means a smoother sphere, but also more vertices and a slower draw.
    //
    // We use enough that a planet looks perfectly round when the camera is
    // right next to it. A sphere is about 3,000 vertices, which is nothing.
    constexpr int SPHERE_SEGMENTS = 96;
    constexpr int SPHERE_RINGS    = 64;
}

// ---------------------------------------------------------------------------
// The camera.
// ---------------------------------------------------------------------------
math3d::Vec3 Camera::eye_position() const
{
    // We work out where the camera is from the three numbers we keep.
    //
    // The pitch and yaw are converted to radians, and then:
    //   x = distance * cos(pitch) * sin(yaw)
    //   y = distance * sin(pitch)
    //   z = distance * cos(pitch) * cos(yaw)
    //
    // The cos(pitch) shrinks the horizontal distance as we look from
    // directly above, which keeps the camera on a sphere around the target.

    const float yaw_radians   = math3d::to_radians(yaw);
    const float pitch_radians = math3d::to_radians(pitch);

    const float horizontal = distance * std::cos(pitch_radians);

    return math3d::Vec3(
        target.x + horizontal * std::sin(yaw_radians),
        target.y + distance * std::sin(pitch_radians),
        target.z + horizontal * std::cos(yaw_radians));
}

math3d::Mat4 Camera::view_matrix() const
{
    return math3d::look_at(eye_position(), target, math3d::Vec3(0.0f, 1.0f, 0.0f));
}

// ---------------------------------------------------------------------------
// The renderer.
// ---------------------------------------------------------------------------
Renderer::~Renderer()
{
    // Delete all the OpenGL objects we created.
    //
    // The order does not matter much, but we do it in the reverse of the
    // order we created them, which is tidy.

    if (sun_.vao) glDeleteVertexArrays(1, &sun_.vao);
    if (sun_.vbo) glDeleteBuffers(1, &sun_.vbo);
    if (sun_.ebo) glDeleteBuffers(1, &sun_.ebo);

    if (corona_vao_) glDeleteVertexArrays(1, &corona_vao_);
    if (corona_vbo_) glDeleteBuffers(1, &corona_vbo_);
    if (corona_ebo_) glDeleteBuffers(1, &corona_ebo_);

    for (PlanetGpu& planet : planets_)
    {
        if (planet.vao) glDeleteVertexArrays(1, &planet.vao);
        if (planet.vbo) glDeleteBuffers(1, &planet.vbo);
        if (planet.ebo) glDeleteBuffers(1, &planet.ebo);
    }

    for (PlanetGpu& ring : rings_)
    {
        if (ring.vao) glDeleteVertexArrays(1, &ring.vao);
        if (ring.vbo) glDeleteBuffers(1, &ring.vbo);
        if (ring.ebo) glDeleteBuffers(1, &ring.ebo);
    }

    for (unsigned int vao : orbit_vaos_) glDeleteVertexArrays(1, &vao);
    for (unsigned int vbo : orbit_vbos_) glDeleteBuffers(1, &vbo);

    if (star_vao_) glDeleteVertexArrays(1, &star_vao_);
    if (star_vbo_) glDeleteBuffers(1, &star_vbo_);
}

// ---------------------------------------------------------------------------
// Creating the GPU objects for a mesh.
// ---------------------------------------------------------------------------
void Renderer::create_mesh(const Mesh& mesh,
                           unsigned int& vao, unsigned int& vbo,
                           unsigned int& ebo, unsigned int& index_count)
{
    // A vertex array object remembers which buffers and which attribute
    // settings belong together. We only have to bind it once per draw.
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // --- The vertex buffer ---
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)),
                 mesh.vertices.data(),
                 GL_STATIC_DRAW);

    // --- The index buffer ---
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(unsigned int)),
                 mesh.indices.data(),
                 GL_STATIC_DRAW);

    // --- The attributes ---
    //
    // Our vertices are interleaved: position (3 floats), normal (3 floats),
    // UV (2 floats). That is 8 floats per vertex, or 32 bytes.
    //
    // The stride is how many bytes we jump from one vertex to the next.
    // The offsets say where each attribute starts inside a vertex.
    const GLsizei stride = 8 * sizeof(float);

    // Attribute 0: the position.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    // Attribute 1: the normal.
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Attribute 2: the UV coordinate.
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    index_count = static_cast<unsigned int>(mesh.indices.size());
}

// ---------------------------------------------------------------------------
// Setting everything up.
// ---------------------------------------------------------------------------
bool Renderer::initialise()
{
    // --- The shaders ---
    //
    // The asset paths are relative to the .exe, so the program works when
    // it is started from anywhere.
    const std::string shader_folder = "assets/shaders/";

    const bool planet_ok = planet_shader_.load_from_files(
        find_asset_path(shader_folder + "planet.vert"),
        find_asset_path(shader_folder + "planet.frag"));

    const bool sun_ok = sun_shader_.load_from_files(
        find_asset_path(shader_folder + "sun.vert"),
        find_asset_path(shader_folder + "sun.frag"));

    const bool star_ok = star_shader_.load_from_files(
        find_asset_path(shader_folder + "star.vert"),
        find_asset_path(shader_folder + "star.frag"));

    const bool ring_ok = ring_shader_.load_from_files(
        find_asset_path(shader_folder + "ring.vert"),
        find_asset_path(shader_folder + "ring.frag"));

    const bool line_ok = line_shader_.load_from_files(
        find_asset_path(shader_folder + "line.vert"),
        find_asset_path(shader_folder + "line.frag"));

    shaders_ok_ = planet_ok && sun_ok && star_ok && ring_ok && line_ok;

    if (!shaders_ok_)
    {
        std::cerr << "[Renderer] One or more shaders failed to load." << std::endl;
        return false;
    }

    // --- OpenGL state ---
    //
    // The depth test is what stops the far side of a planet from drawing
    // over the near side. Without it the scene looks like a flat mess.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // We do not cull faces for the planets, because we want to see the
    // inside of the ring discs and the inside of the corona.

    // --- The sun's sphere ---
    {
        Mesh mesh = make_sphere(1.0f, SPHERE_SEGMENTS, SPHERE_RINGS);
        create_mesh(mesh, sun_.vao, sun_.vbo, sun_.ebo, sun_.index_count);
    }

    // --- The corona quad ---
    {
        Mesh mesh = make_quad(2.0f);
        create_mesh(mesh, corona_vao_, corona_vbo_, corona_ebo_, corona_index_count_);
    }

    // --- The planets ---
    //
    // Every planet is the same shape, but we give each one its own buffers
    // so that they can be drawn independently.
    const SolarSystem system;      // a temporary one, just to know the count

    const int planet_count = static_cast<int>(system.planets().size());
    planets_.resize(planet_count);
    rings_.resize(planet_count);

    for (int i = 0; i < planet_count; ++i)
    {
        const Mesh sphere = make_sphere(1.0f, SPHERE_SEGMENTS, SPHERE_RINGS);
        create_mesh(sphere, planets_[i].vao, planets_[i].vbo,
                    planets_[i].ebo, planets_[i].index_count);

        // The rings of the planets that have them.
        const Planet& planet = system.planets()[i];

        if (planet.has_rings)
        {
            // The ring mesh is built for a planet of radius 1, so these
            // numbers are in planet radii.
            //
            // Saturn's rings actually start about 1.11 radii out and end
            // about 2.27 radii out. We use a slightly tighter inner edge so
            // the rings stand clear of the planet.
            const Mesh ring = make_ring(1.25f, 2.20f, 128);
            create_mesh(ring, rings_[i].vao, rings_[i].vbo,
                        rings_[i].ebo, rings_[i].index_count);
        }
        else
        {
            rings_[i].vao = 0;
        }
    }

    // --- The orbit lines ---
    //
    // Each orbit is a GL_LINE_STRIP: a connected chain of line segments.
    orbit_vaos_.resize(planet_count);
    orbit_vbos_.resize(planet_count);
    orbit_counts_.resize(planet_count);

    for (int i = 0; i < planet_count; ++i)
    {
        const Planet& planet = system.planets()[i];

        const std::vector<float> points = make_orbit_points(planet.orbit_radius, 128);

        glGenVertexArrays(1, &orbit_vaos_[i]);
        glBindVertexArray(orbit_vaos_[i]);

        glGenBuffers(1, &orbit_vbos_[i]);
        glBindBuffer(GL_ARRAY_BUFFER, orbit_vbos_[i]);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(points.size() * sizeof(float)),
                     points.data(),
                     GL_STATIC_DRAW);

        // Orbits only need a position -- three floats per point.
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);

        orbit_counts_[i] = static_cast<unsigned int>(points.size() / 3);
    }

    // --- The stars ---
    //
    // We scatter the stars on a very large sphere. Each one gets a random
    // brightness and a random size, so the sky looks natural.
    {
        const int star_count = 12000;

        // Interleaved: position (3), brightness (1), size (1) = 5 floats.
        std::vector<float> star_data;
        star_data.reserve(static_cast<size_t>(star_count) * 5);

        std::random_device random_device;
        std::mt19937 random_engine(random_device());

        // The radius of the sphere the stars sit on. It has to be far
        // enough away that the stars never appear to move, but still
        // inside the far clipping plane.
        std::uniform_real_distribution<float> radius_distribution(800.0f, 900.0f);

        // Most stars are dim, a few are bright. We use a power to make the
        // distribution favour dim stars, like the real sky.
        std::uniform_real_distribution<float> brightness_distribution(0.0f, 1.0f);

        // The size of the dot, in pixels. Real stars are points of light,
        // so these are small -- just one or two pixels across. Anything
        // bigger starts to look like snow rather than a night sky.
        std::uniform_real_distribution<float> size_distribution(1.0f, 2.2f);

        // A number between -1 and 1, and a number between 0 and 2*PI.
        //
        // IMPORTANT: we must use std::uniform_real_distribution for this.
        // The obvious-looking shortcut -- dividing random_engine() by
        // RAND_MAX -- is WRONG, because std::mt19937 produces 32-bit
        // numbers while RAND_MAX is only 32767 on MinGW. The result is a
        // number far outside -1..1, which makes the star positions NaN and
        // the whole sky disappears.
        std::uniform_real_distribution<float> height_distribution(-1.0f, 1.0f);
        std::uniform_real_distribution<float> angle_distribution(0.0f, 2.0f * 3.14159265f);

        for (int i = 0; i < star_count; ++i)
        {
            // A random point on a sphere.
            //
            // We use the "z" method: pick z uniformly between -1 and 1, then
            // pick an angle. This spreads the stars evenly over the sphere
            // instead of bunching them up at the poles.
            const float unit_z = height_distribution(random_engine);
            const float sin_theta = std::sqrt(1.0f - unit_z * unit_z);
            const float phi = angle_distribution(random_engine);

            const float radius = radius_distribution(random_engine);

            star_data.push_back(radius * sin_theta * std::cos(phi));
            star_data.push_back(radius * unit_z);
            star_data.push_back(radius * sin_theta * std::sin(phi));

            // The brightness: mostly dim, occasionally bright.
            const float brightness_value = brightness_distribution(random_engine);
            star_data.push_back(0.25f + brightness_value * brightness_value * 0.55f);

            // The size: a bit bigger for the brighter stars.
            //
            // The minimum is 1.0 pixel. Most stars are just a single pixel
            // of light, which is what makes a night sky look right.
            star_data.push_back(1.0f + size_distribution(random_engine) *
                                (0.5f + brightness_value * 0.6f));
        }

        star_count_ = star_count;

        glGenVertexArrays(1, &star_vao_);
        glBindVertexArray(star_vao_);

        glGenBuffers(1, &star_vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, star_vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(star_data.size() * sizeof(float)),
                     star_data.data(),
                     GL_STATIC_DRAW);

        // Attribute 0: position (3 floats).
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Attribute 1: brightness (1 float).
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Attribute 2: size (1 float).
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Drawing the stars.
// ---------------------------------------------------------------------------
void Renderer::draw_stars(const math3d::Mat4& view, const math3d::Mat4& projection, float time)
{
    star_shader_.use();

    star_shader_.set_mat4("uView", view);
    star_shader_.set_mat4("uProjection", projection);
    star_shader_.set_float("uTime", time);

    glBindVertexArray(star_vao_);
    glDrawArrays(GL_POINTS, 0, star_count_);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Drawing the orbit lines.
// ---------------------------------------------------------------------------
void Renderer::draw_orbits(const SolarSystem& system,
                           const math3d::Mat4& view, const math3d::Mat4& projection,
                           const math3d::Vec3& eye)
{
    // The orbits get their own tiny shader. We could have reused the planet
    // shader, but that one expects a normal and a UV coordinate, and an
    // orbit line has neither. A two-line shader is clearer than bending
    // another one to fit.
    (void)eye;

    line_shader_.use();

    line_shader_.set_mat4("uView", view);
    line_shader_.set_mat4("uProjection", projection);

    const int count = static_cast<int>(system.planets().size());

    for (int i = 0; i < count; ++i)
    {
        // A faint blue-grey line. The outer orbits are drawn slightly
        // brighter, so we can tell them apart.
        const float brightness = 0.10f + 0.05f * (static_cast<float>(i) / count);
        line_shader_.set_vec3("uLineColour",
                              math3d::Vec3(brightness * 0.7f,
                                           brightness * 0.85f,
                                           brightness * 1.3f));

        glBindVertexArray(orbit_vaos_[i]);
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(orbit_counts_[i]));
    }

    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Drawing the sun and its corona.
// ---------------------------------------------------------------------------
void Renderer::draw_sun(const SolarSystem& system,
                        const math3d::Mat4& view, const math3d::Mat4& projection,
                        const math3d::Vec3& eye, float time)
{
    const Sun& sun = system.sun();

    sun_shader_.use();

    sun_shader_.set_mat4("uView", view);
    sun_shader_.set_mat4("uProjection", projection);
    sun_shader_.set_float("uTime", time);
    sun_shader_.set_vec3("uSunColour", sun.colour);

    // --- The corona ---
    //
    // We draw it as a flat quad that always faces the camera. To do that we
    // build a model matrix that moves the quad to the sun and then turns it
    // towards the camera.
    //
    // The trick is: we take the direction from the sun to the camera, and
    // build a rotation that points the quad's +Z axis along it.
    {
        const math3d::Vec3 to_eye = (eye - math3d::Vec3(0.0f, 0.0f, 0.0f)).normalized();

        // We want to rotate the +Z axis onto the direction to the camera.
        // A full "look at" rotation is more than we need, so we build it by
        // hand: the quad's up direction is whatever is perpendicular to the
        // view direction and the world up.
        const math3d::Vec3 right = to_eye.cross(math3d::Vec3(0.0f, 1.0f, 0.0f)).normalized();
        const math3d::Vec3 up    = right.cross(to_eye);

        math3d::Mat4 model;
        // Put the three axes of the rotation straight into the matrix.
        model.at(0, 0) = right.x; model.at(0, 1) = right.y; model.at(0, 2) = right.z;
        model.at(1, 0) = up.x;    model.at(1, 1) = up.y;    model.at(1, 2) = up.z;
        model.at(2, 0) = to_eye.x; model.at(2, 1) = to_eye.y; model.at(2, 2) = to_eye.z;
        // The size of the glow.
        model.at(0, 0) *= sun.glow_radius;
        model.at(1, 1) *= sun.glow_radius;
        model.at(2, 2) *= sun.glow_radius;

        sun_shader_.set_int("uIsCorona", 1);
        sun_shader_.set_float("uGlowStrength", 0.85f);
        sun_shader_.set_mat4("uModel", model);

        // The glow is transparent, so we blend it with what is behind it.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);   // the glow does not write to the depth buffer

        glBindVertexArray(corona_vao_);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(corona_index_count_),
                       GL_UNSIGNED_INT, (void*)0);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    // --- The sun itself ---
    {
        const math3d::Mat4 model = math3d::scale(sun.radius);

        sun_shader_.set_int("uIsCorona", 0);
        sun_shader_.set_float("uGlowStrength", 0.0f);
        sun_shader_.set_mat4("uModel", model);

        glBindVertexArray(sun_.vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sun_.index_count),
                       GL_UNSIGNED_INT, (void*)0);
        glBindVertexArray(0);
    }
}

// ---------------------------------------------------------------------------
// Drawing one planet.
// ---------------------------------------------------------------------------
void Renderer::draw_planet(const Planet& planet, const SolarSystem& system,
                           const math3d::Mat4& view, const math3d::Mat4& projection,
                           const math3d::Vec3& eye, float time)
{
    const math3d::Vec3 position = system.planet_position(planet);

    // --- The model matrix ---
    //
    // Three things happen to the planet:
    //   1. it spins on its axis      (rotate about Y)
    //   2. it leans over             (rotate about X, the axial tilt)
    //   3. it moves to its orbit spot (translate)
    //
    // The order matters a great deal, because each step is applied to the
    // result of the previous one.
    //
    // We read it from the inside out:
    //
    //   scale(radius)      makes the unit sphere the right size
    //   rotate_x(tilt)     leans it over
    //   rotate_y(spin)     spins it
    //   translate(position) moves it to its place on the orbit
    //
    // The two things that catch people out:
    //
    //   * We spin BEFORE we tilt, so the tilt does not spin round with the
    //     planet. If we tilted first and then spun, the tilt would rotate
    //     away, which is not what we want.
    //
    //   * We scale BEFORE we translate. If we translated first and then
    //     scaled, the scale would multiply the planet's position as well
    //     as its size, and Jupiter would be flung off to a completely
    //     wrong place on its orbit.
    math3d::Mat4 model = math3d::rotate_y(planet.rotation_angle);

    const float tilt = math3d::to_radians(planet.axial_tilt_degrees);
    model = math3d::multiply(math3d::rotate_x(tilt), model);

    // The size of the planet.
    model = math3d::multiply(math3d::scale(planet.radius), model);

    // ...and finally, where it is on its orbit.
    model = math3d::multiply(math3d::translate(position.x, position.y, position.z), model);

    planet_shader_.use();

    planet_shader_.set_mat4("uModel", model);
    planet_shader_.set_mat4("uView", view);
    planet_shader_.set_mat4("uProjection", projection);

    planet_shader_.set_vec3("uBaseColour", planet.base_colour);
    planet_shader_.set_vec3("uBandColour", planet.band_colour);
    planet_shader_.set_float("uBandStrength", planet.band_strength);

    planet_shader_.set_vec3("uSunPosition", math3d::Vec3(0.0f, 0.0f, 0.0f));
    planet_shader_.set_vec3("uEyePosition", eye);

    // Shiny planets (the ones with water) get a sharper highlight.
    planet_shader_.set_float("uShininess", 40.0f);

    // The atmosphere: the gas giants have a thick one, the rocky planets
    // almost none.
    const float atmosphere = (planet.radius > 1.6f) ? 0.35f : 0.10f;
    planet_shader_.set_float("uAtmosphere", atmosphere);

    (void)time;

    glBindVertexArray(planets_[0].vao);   // all the spheres share a shape
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(planets_[0].index_count),
                   GL_UNSIGNED_INT, (void*)0);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Drawing the rings of a planet.
// ---------------------------------------------------------------------------
void Renderer::draw_rings(const Planet& planet, const SolarSystem& system,
                          const math3d::Mat4& view, const math3d::Mat4& projection,
                          const math3d::Vec3& eye)
{
    // Find which planet this is, so we know which ring buffers to use.
    const std::vector<Planet>& planets = system.planets();

    int index = 0;
    for (int i = 0; i < static_cast<int>(planets.size()); ++i)
    {
        if (planets[i].name == planet.name) { index = i; break; }
    }

    if (rings_[index].vao == 0) return;

    const math3d::Vec3 position = system.planet_position(planet);

    // The rings belong to the planet, so they spin with it and lean with
    // it, but they do NOT move around the sun on their own.
    //
    // The order is the same as for the planet itself: spin, tilt, scale,
    // and only then move it to its place on the orbit.
    math3d::Mat4 model = math3d::rotate_y(planet.rotation_angle);

    const float tilt = math3d::to_radians(planet.axial_tilt_degrees);
    model = math3d::multiply(math3d::rotate_x(tilt), model);

    // The ring mesh is built for a planet of radius 1, so we scale it to
    // match the planet's size.
    model = math3d::multiply(math3d::scale(planet.radius), model);

    model = math3d::multiply(math3d::translate(position.x, position.y, position.z), model);

    ring_shader_.use();

    ring_shader_.set_mat4("uModel", model);
    ring_shader_.set_mat4("uView", view);
    ring_shader_.set_mat4("uProjection", projection);

    ring_shader_.set_vec3("uRingColour", math3d::Vec3(0.85f, 0.78f, 0.62f));
    ring_shader_.set_vec3("uSunPosition", math3d::Vec3(0.0f, 0.0f, 0.0f));
    ring_shader_.set_vec3("uEyePosition", eye);
    ring_shader_.set_vec3("uPlanetPosition", position);

    ring_shader_.set_float("uPlanetRadius", planet.radius);

    // The rings are see-through, so we blend them.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(rings_[index].vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(rings_[index].index_count),
                   GL_UNSIGNED_INT, (void*)0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Drawing one whole frame.
// ---------------------------------------------------------------------------
void Renderer::draw(const SolarSystem& system, const Camera& camera,
                    float time_seconds, int window_width, int window_height)
{
    // --- The projection matrix ---
    //
    // The aspect ratio is the width divided by the height. If we did not
    // correct for it, the scene would look stretched when the window is not
    // square.
    const float aspect = (window_height > 0)
        ? static_cast<float>(window_width) / static_cast<float>(window_height)
        : 1.0f;

    const math3d::Mat4 projection = math3d::perspective(45.0f, aspect, 0.1f, 2000.0f);
    const math3d::Mat4 view = camera.view_matrix();
    const math3d::Vec3 eye = camera.eye_position();

    // --- Clear the screen ---
    //
    // A very dark blue, not pure black. It looks better behind the planets
    // and matches the ambient light in the planet shader.
    glClearColor(0.008f, 0.010f, 0.022f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- The background stars ---
    //
    // These go first, with the depth test off, so they are always behind
    // everything else.
    glDisable(GL_DEPTH_TEST);
    draw_stars(view, projection, time_seconds);
    glEnable(GL_DEPTH_TEST);

    // --- The orbit lines ---
    draw_orbits(system, view, projection, eye);

    // --- The planets and their rings ---
    for (const Planet& planet : system.planets())
    {
        // The rings go first, because they are partly transparent and the
        // planet has to be drawn over them where it is in front.
        if (planet.has_rings)
            draw_rings(planet, system, view, projection, eye);

        draw_planet(planet, system, view, projection, eye, time_seconds);
    }

    // --- The sun ---
    //
    // The sun goes last, so its glow blends over everything.
    draw_sun(system, view, projection, eye, time_seconds);
}
