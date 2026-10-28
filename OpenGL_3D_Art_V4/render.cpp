// render.cpp -- The camera, the OpenGL objects and the drawing code.

#include "render.hpp"
#include "geometry.hpp"

// IMPORTANT: glew.h must be included BEFORE glfw3.h anywhere in the project.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <random>
#include <iostream>

namespace
{
    // How smooth the planets are.
    //
    // A UV sphere is made of a grid of quads. More segments and more rings
    // means a smoother sphere, but also more vertices and a slower draw.
    constexpr int SPHERE_SEGMENTS = 96;
    constexpr int SPHERE_RINGS    = 64;

    // How many stars are in the sky.
    constexpr int STAR_COUNT = 12000;

    // The radius of the sphere the stars sit on. It has to be far enough
    // away that the stars never appear to move, but still inside the far
    // clipping plane.
    constexpr float STAR_RADIUS_MIN = 800.0f;
    constexpr float STAR_RADIUS_MAX = 900.0f;
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
    // directly above, which keeps the camera on a sphere around the sun.
    const float yaw_radians   = math3d::to_radians(yaw);
    const float pitch_radians = math3d::to_radians(pitch);

    const float horizontal = distance * std::cos(pitch_radians);

    return math3d::Vec3(
        horizontal * std::sin(yaw_radians),
        distance * std::sin(pitch_radians),
        horizontal * std::cos(yaw_radians));
}

math3d::Mat4 Camera::view_matrix() const
{
    // The camera always looks at the sun, which sits at the origin.
    return math3d::look_at(eye_position(),
                            math3d::Vec3(0.0f, 0.0f, 0.0f),
                            math3d::Vec3(0.0f, 1.0f, 0.0f));
}

// ---------------------------------------------------------------------------
// The renderer.
// ---------------------------------------------------------------------------
Renderer::~Renderer()
{
    // Delete all the OpenGL objects we created.
    if (sphere_.vao) glDeleteVertexArrays(1, &sphere_.vao);
    if (sphere_.vbo) glDeleteBuffers(1, &sphere_.vbo);
    if (sphere_.ebo) glDeleteBuffers(1, &sphere_.ebo);

    if (corona_.vao) glDeleteVertexArrays(1, &corona_.vao);
    if (corona_.vbo) glDeleteBuffers(1, &corona_.vbo);
    if (corona_.ebo) glDeleteBuffers(1, &corona_.ebo);

    for (GpuMesh& ring : rings_)
    {
        if (ring.vao) glDeleteVertexArrays(1, &ring.vao);
        if (ring.vbo) glDeleteBuffers(1, &ring.vbo);
        if (ring.ebo) glDeleteBuffers(1, &ring.ebo);
    }

    for (unsigned int vao : orbit_vaos_) glDeleteVertexArrays(1, &vao);
    for (unsigned int vbo : orbit_vbos_) glDeleteBuffers(1, &vbo);

    if (star_vao_) glDeleteVertexArrays(1, &star_vao_);
    if (star_vbo_) glDeleteBuffers(1, &star_vbo_);

    if (asteroid_vao_) glDeleteVertexArrays(1, &asteroid_vao_);
    if (asteroid_vbo_) glDeleteBuffers(1, &asteroid_vbo_);
}

// ---------------------------------------------------------------------------
// Creating the GPU objects for a mesh.
// ---------------------------------------------------------------------------
void Renderer::create_mesh(const Mesh& mesh, GpuMesh& out)
{
    // A vertex array object remembers which buffers and which attribute
    // settings belong together. We only have to bind it once per draw.
    glGenVertexArrays(1, &out.vao);
    glBindVertexArray(out.vao);

    // --- The vertex buffer ---
    glGenBuffers(1, &out.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(float)),
                 mesh.vertices.data(),
                 GL_STATIC_DRAW);

    // --- The index buffer ---
    glGenBuffers(1, &out.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
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

    out.index_count = static_cast<unsigned int>(mesh.indices.size());
}

// ---------------------------------------------------------------------------
// Creating the GPU objects for a cloud of points.
//
// The stars and the asteroids are both drawn as GL_POINTS, and both use the
// same layout: position (3), brightness (1), size (1) -- 5 floats per point.
// ---------------------------------------------------------------------------
void Renderer::create_points(const std::vector<float>& data,
                             unsigned int& vao, unsigned int& vbo)
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                 data.data(),
                 GL_STATIC_DRAW);

    const GLsizei stride = 5 * sizeof(float);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
                          (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Setting everything up.
//
// This is deliberately a short list of steps. Each step has its own function
// further down, so you can read the whole setup here and then look at the
// one part you are interested in.
// ---------------------------------------------------------------------------
bool Renderer::initialise()
{
    if (!load_shaders())
        return false;

    // The depth test is what stops the far side of a planet from drawing
    // over the near side. Without it the scene looks like a flat mess.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // We do not cull faces, because we want to see the inside of the ring
    // discs and the inside of the corona.

    // A temporary solar system, just so we know how many planets there are
    // and what their rings look like.
    const SolarSystem system;

    // ONE sphere, shared by the sun and all eight planets. They are all the
    // same shape -- only the model matrix and the colours differ.
    create_mesh(make_sphere(1.0f, SPHERE_SEGMENTS, SPHERE_RINGS), sphere_);

    // The corona: a flat quad we always turn towards the camera.
    create_mesh(make_quad(2.0f), corona_);

    build_planets(system);
    build_orbits(system);
    build_stars();
    build_asteroids(system);

    return true;
}

// ---------------------------------------------------------------------------
// Loading and compiling the five shader programs.
// ---------------------------------------------------------------------------
bool Renderer::load_shaders()
{
    // The asset paths are relative to the .exe, so the program works when
    // it is started from anywhere.
    const std::string folder = "assets/shaders/";

    const bool planet_ok = planet_shader_.load_from_files(
        find_asset_path(folder + "planet.vert"),
        find_asset_path(folder + "planet.frag"));

    const bool sun_ok = sun_shader_.load_from_files(
        find_asset_path(folder + "sun.vert"),
        find_asset_path(folder + "sun.frag"));

    const bool ring_ok = ring_shader_.load_from_files(
        find_asset_path(folder + "ring.vert"),
        find_asset_path(folder + "ring.frag"));

    const bool line_ok = line_shader_.load_from_files(
        find_asset_path(folder + "line.vert"),
        find_asset_path(folder + "line.frag"));

    const bool point_ok = point_shader_.load_from_files(
        find_asset_path(folder + "point.vert"),
        find_asset_path(folder + "point.frag"));

    if (!planet_ok || !sun_ok || !ring_ok || !line_ok || !point_ok)
    {
        std::cerr << "[Renderer] One or more shaders failed to load." << std::endl;
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Building the rings.
//
// Only the planets that have rings get one. The ring mesh is built for a
// planet of radius 1, so the inner and outer numbers are in planet radii.
//
// Saturn's rings start about 1.25 radii out and end about 2.20 radii out.
// Uranus's rings are narrower -- they are narrow bands of rock rather than a
// wide sheet of ice -- so each planet gets its own mesh.
// ---------------------------------------------------------------------------
void Renderer::build_planets(const SolarSystem& system)
{
    const int planet_count = static_cast<int>(system.planets().size());
    rings_.resize(planet_count);

    for (int i = 0; i < planet_count; ++i)
    {
        const Planet& planet = system.planets()[i];

        if (planet.has_rings)
        {
            create_mesh(make_ring(planet.ring_inner, planet.ring_outer, 128),
                        rings_[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// Building the orbit lines.
//
// Each orbit is a GL_LINE_STRIP: a connected chain of line segments.
// ---------------------------------------------------------------------------
void Renderer::build_orbits(const SolarSystem& system)
{
    const int planet_count = static_cast<int>(system.planets().size());

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
}

// ---------------------------------------------------------------------------
// Building the background stars.
//
// We scatter the stars on a very large sphere. Each one gets a random
// brightness and a random size, so the sky looks natural.
// ---------------------------------------------------------------------------
void Renderer::build_stars()
{
    std::vector<float> star_data;
    star_data.reserve(static_cast<size_t>(STAR_COUNT) * 5);

    std::random_device random_device;
    std::mt19937 random_engine(random_device());

    std::uniform_real_distribution<float> radius_distribution(
        STAR_RADIUS_MIN, STAR_RADIUS_MAX);

    // Most stars are dim, a few are bright. We use a power to make the
    // distribution favour dim stars, like the real sky.
    std::uniform_real_distribution<float> brightness_distribution(0.0f, 1.0f);

    // The size of the dot, in pixels. Real stars are points of light, so
    // these are small -- just one or two pixels across. Anything bigger
    // starts to look like snow rather than a night sky.
    std::uniform_real_distribution<float> size_distribution(1.0f, 2.2f);

    // A number between -1 and 1, and a number between 0 and 2*PI.
    //
    // IMPORTANT: we must use std::uniform_real_distribution for this. The
    // obvious-looking shortcut -- dividing random_engine() by RAND_MAX --
    // is WRONG, because std::mt19937 produces 32-bit numbers while RAND_MAX
    // is only 32767 on MinGW. The result is a number far outside -1..1,
    // which makes the star positions NaN and the whole sky disappears.
    std::uniform_real_distribution<float> height_distribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> angle_distribution(0.0f, 2.0f * 3.14159265f);

    for (int i = 0; i < STAR_COUNT; ++i)
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
        // The minimum is 1.0 pixel. Most stars are just a single pixel of
        // light, which is what makes a night sky look right.
        star_data.push_back(1.0f + size_distribution(random_engine) *
                            (0.5f + brightness_value * 0.6f));
    }

    star_count_ = STAR_COUNT;
    create_points(star_data, star_vao_, star_vbo_);
}

// ---------------------------------------------------------------------------
// Building the asteroid belt.
//
// The belt is another cloud of points, but unlike the stars these live in
// real world space, between Mars and Jupiter.
// ---------------------------------------------------------------------------
void Renderer::build_asteroids(const SolarSystem& system)
{
    const AsteroidBelt& belt = system.asteroid_belt();

    std::vector<float> belt_data;
    belt_data.reserve(static_cast<size_t>(belt.count) * 5);

    std::random_device random_device;
    std::mt19937 random_engine(random_device());

    // The distance from the sun. We use a square root so that the asteroids
    // are spread evenly along the belt rather than bunching up at the inner
    // edge -- the same trick we use for the planets.
    std::uniform_real_distribution<float> radial_distribution(
        std::sqrt(belt.inner_radius), std::sqrt(belt.outer_radius));

    // How high above the plane of the orbits the asteroid floats, and where
    // round the sun it is.
    std::uniform_real_distribution<float> height_distribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> angle_distribution(0.0f, 2.0f * 3.14159265f);

    // How bright each asteroid is. The belt looks best when there are a lot
    // of very faint rocks and a few brighter ones, so we square the random
    // number to push the distribution towards the dim end.
    std::uniform_real_distribution<float> brightness_distribution(0.0f, 1.0f);

    // The size of the dot. A few of the asteroids are noticeably bigger
    // than the rest.
    std::uniform_real_distribution<float> size_distribution(0.5f, 2.0f);

    for (int i = 0; i < belt.count; ++i)
    {
        // The distance from the sun.
        //
        // We take the square root of the random number, which spreads the
        // asteroids evenly ALONG the belt instead of bunching them up at
        // the inner edge. (The same reason we use a square root for the
        // planet orbits.)
        const float radius = radial_distribution(random_engine);
        const float radius_squared = radius * radius;

        // Where round the sun it is.
        const float angle = angle_distribution(random_engine);

        // How high above the plane it floats.
        //
        // We square the height so that the asteroids cluster near the
        // plane, which is what gives the belt its flat, disc-like shape
        // when you look at it edge on.
        const float height = height_distribution(random_engine) *
                             height_distribution(random_engine) *
                             belt.half_thickness;

        belt_data.push_back(radius_squared * std::cos(angle));
        belt_data.push_back(height);
        belt_data.push_back(radius_squared * std::sin(angle));

        // The brightness.
        //
        // Most of the rocks are dim and a few are brighter. We square the
        // random number to push the distribution towards the dim end -- but
        // not so far that the rocks in front of a bright planet turn into
        // black specks.
        const float brightness_value = brightness_distribution(random_engine);
        belt_data.push_back(belt.brightness *
                            (0.45f + brightness_value * brightness_value * 0.55f));

        // The size. The biggest rocks stand out a little.
        belt_data.push_back(belt.point_size * size_distribution(random_engine));
    }

    asteroid_count_ = belt.count;
    create_points(belt_data, asteroid_vao_, asteroid_vbo_);
}

// ---------------------------------------------------------------------------
// Drawing the background stars.
// ---------------------------------------------------------------------------
void Renderer::draw_stars(const math3d::Mat4& view, const math3d::Mat4& projection,
                          float time)
{
    point_shader_.use();

    point_shader_.set_mat4("uView", view);
    point_shader_.set_mat4("uProjection", projection);
    point_shader_.set_float("uTime", time);
    point_shader_.set_int("uIsStar", 1);

    glBindVertexArray(star_vao_);
    glDrawArrays(GL_POINTS, 0, star_count_);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Drawing the asteroid belt.
//
// The belt is a cloud of points between Mars and Jupiter. The points are in
// real world space, so they use the full view matrix and they turn round
// with the sun in the way a real belt would.
//
// They are drawn with the depth test ON, so a planet in front of the belt
// correctly hides the rocks behind it.
// ---------------------------------------------------------------------------
void Renderer::draw_asteroids(const math3d::Mat4& view, const math3d::Mat4& projection,
                              float time)
{
    point_shader_.use();

    point_shader_.set_mat4("uView", view);
    point_shader_.set_mat4("uProjection", projection);
    point_shader_.set_float("uTime", time);
    point_shader_.set_int("uIsStar", 0);

    glBindVertexArray(asteroid_vao_);
    glDrawArrays(GL_POINTS, 0, asteroid_count_);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Drawing the orbit lines.
// ---------------------------------------------------------------------------
void Renderer::draw_orbits(const SolarSystem& system,
                           const math3d::Mat4& view, const math3d::Mat4& projection)
{
    // The orbits get their own tiny shader. We could have reused the planet
    // shader, but that one expects a normal and a UV coordinate, and an
    // orbit line has neither. A two-line shader is clearer than bending
    // another one to fit.
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

    // ---------------------------------------------------------------------
    // The sun itself, FIRST.
    //
    // The order of these two draws matters a lot.
    //
    // The corona is a FLAT quad that sits at the sun's centre. The sun is a
    // SPHERE. The flat quad is nearer to the camera than the far half of
    // the sphere, and -- because a sphere curves away at its edge -- the
    // quad pokes out in front of the sphere's silhouette.
    //
    // If we drew the corona first, it would blend over the orbit lines and
    // over the edge of the sun, leaving a dark arc across the sun's face.
    //
    // So we draw the opaque sphere FIRST. It writes its depth, and then the
    // corona is depth-tested against it: the part of the quad in front of
    // the sphere is rejected, and only the ring of glow around the outside
    // survives.
    // ---------------------------------------------------------------------
    {
        const math3d::Mat4 model = math3d::scale(sun.radius);

        sun_shader_.set_int("uIsCorona", 0);
        sun_shader_.set_float("uGlowStrength", 0.0f);
        sun_shader_.set_mat4("uModel", model);

        glBindVertexArray(sphere_.vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sphere_.index_count),
                       GL_UNSIGNED_INT, (void*)0);
        glBindVertexArray(0);
    }

    // --- The corona ---
    //
    // We draw it as a flat quad that always faces the camera. To do that we
    // build a model matrix that moves the quad to the sun and then turns it
    // towards the camera.
    //
    // The trick is: we take the direction from the sun to the camera, and
    // build a rotation that points the quad's +Z axis along it.
    {
        const math3d::Vec3 to_eye = eye.normalized();

        // We want to rotate the +Z axis onto the direction to the camera.
        // A full "look at" rotation is more than we need, so we build it by
        // hand: the quad's up direction is whatever is perpendicular to the
        // view direction and the world up.
        const math3d::Vec3 right = to_eye.cross(math3d::Vec3(0.0f, 1.0f, 0.0f)).normalized();
        const math3d::Vec3 up    = right.cross(to_eye);

        math3d::Mat4 model;
        // Put the three axes of the rotation straight into the matrix.
        model.at(0, 0) = right.x;  model.at(0, 1) = right.y;  model.at(0, 2) = right.z;
        model.at(1, 0) = up.x;     model.at(1, 1) = up.y;     model.at(1, 2) = up.z;
        model.at(2, 0) = to_eye.x; model.at(2, 1) = to_eye.y; model.at(2, 2) = to_eye.z;
        // The size of the glow.
        model.at(0, 0) *= sun.glow_radius;
        model.at(1, 1) *= sun.glow_radius;
        model.at(2, 2) *= sun.glow_radius;

        sun_shader_.set_int("uIsCorona", 1);
        sun_shader_.set_float("uGlowStrength", 0.85f);
        sun_shader_.set_mat4("uModel", model);

        // Where the sun's own edge sits on the corona quad, as a fraction
        // of the quad's half-width.
        //
        // The quad is `make_quad(2.0f)`, so its half-width is 1.0, and we
        // scale it by the glow radius -- which makes the half-width
        // `glow_radius` world units. The sun's surface is at `radius` world
        // units, so the fraction is simply radius / glow_radius.
        //
        // The shader needs this so that the glow fades out from the EDGE
        // of the sun outwards. Without it, most of the glow's light would
        // be spent behind the sun's own disc, where nobody can see it.
        const float surface_fraction =
            (sun.glow_radius > 0.0f) ? (sun.radius / sun.glow_radius) : 0.0f;
        sun_shader_.set_float("uSurfaceFraction", surface_fraction);

        // The glow is transparent, so we blend it with what is behind it.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);   // the glow does not write to the depth buffer

        glBindVertexArray(corona_.vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(corona_.index_count),
                       GL_UNSIGNED_INT, (void*)0);

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

// ---------------------------------------------------------------------------
// Drawing one planet.
// ---------------------------------------------------------------------------
void Renderer::draw_planet(const Planet& planet, const SolarSystem& system,
                           const math3d::Mat4& view, const math3d::Mat4& projection,
                           const math3d::Vec3& eye)
{
    const math3d::Vec3 position = system.planet_position(planet);
    const math3d::Mat4 model = planet_model_matrix(planet, position);

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

    glBindVertexArray(sphere_.vao);   // every planet is the same sphere
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sphere_.index_count),
                   GL_UNSIGNED_INT, (void*)0);
    glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// The model matrix for a planet (and for its rings).
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
//
// The rings use this SAME matrix, which is why they spin and lean with the
// planet. They do not move around the sun on their own -- the translate
// puts them in the planet's place, which is exactly right.
// ---------------------------------------------------------------------------
math3d::Mat4 Renderer::planet_model_matrix(const Planet& planet,
                                           const math3d::Vec3& position) const
{
    math3d::Mat4 model = math3d::rotate_y(planet.rotation_angle);

    const float tilt = math3d::to_radians(planet.axial_tilt);
    model = math3d::multiply(math3d::rotate_x(tilt), model);

    // The size of the planet.
    model = math3d::multiply(math3d::scale(planet.radius), model);

    // ...and finally, where it is on its orbit.
    model = math3d::multiply(math3d::translate(position.x, position.y, position.z), model);

    return model;
}

// ---------------------------------------------------------------------------
// Drawing the rings of a planet.
// ---------------------------------------------------------------------------
void Renderer::draw_rings(const Planet& planet, int index, const SolarSystem& system,
                          const math3d::Mat4& view, const math3d::Mat4& projection,
                          const math3d::Vec3& eye)
{
    if (rings_[index].vao == 0) return;

    const math3d::Vec3 position = system.planet_position(planet);
    const math3d::Mat4 model = planet_model_matrix(planet, position);

    ring_shader_.use();

    ring_shader_.set_mat4("uModel", model);
    ring_shader_.set_mat4("uView", view);
    ring_shader_.set_mat4("uProjection", projection);

    ring_shader_.set_vec3("uRingColour", planet.ring_colour);
    ring_shader_.set_vec3("uSunPosition", math3d::Vec3(0.0f, 0.0f, 0.0f));
    ring_shader_.set_vec3("uEyePosition", eye);
    ring_shader_.set_vec3("uPlanetPosition", position);

    ring_shader_.set_float("uPlanetRadius", planet.radius);

    // How bright the rings are. Saturn's are bright water ice; Uranus's are
    // narrow, dark bands of rock that you can barely make out.
    ring_shader_.set_float("uRingBrightness", planet.ring_brightness);

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
    draw_orbits(system, view, projection);

    // --- The asteroid belt ---
    //
    // The belt goes after the orbit lines but before the planets, so the
    // planets (and the sun's glow) are drawn over it.
    draw_asteroids(view, projection, time_seconds);

    // --- The planets and their rings ---
    for (int i = 0; i < static_cast<int>(system.planets().size()); ++i)
    {
        const Planet& planet = system.planets()[i];

        // The planet goes FIRST and writes to the depth buffer.
        draw_planet(planet, system, view, projection, eye);

        // The rings go AFTER, with the depth test still on.
        //
        // This is what makes the ring and the planet sort correctly:
        //
        //   * the part of the ring behind the planet is rejected by the
        //     depth test, because the planet is nearer
        //   * the part of the ring in front of the planet is drawn over it
        //
        // If we drew the rings first they would have nothing to test
        // against, and the planet would paint over the near half of the
        // ring -- which looked like the ring had a chunk missing from it.
        if (planet.has_rings)
            draw_rings(planet, i, system, view, projection, eye);
    }

    // --- The sun ---
    //
    // The sun goes last, so its glow blends over everything.
    draw_sun(system, view, projection, eye, time_seconds);
}
