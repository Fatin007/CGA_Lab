// solar_system.hpp -- The data that describes our solar system.
//
// Everything the renderer needs to know about a planet lives in this file:
// how big it is, how far it is from the sun, how fast it moves, and what it
// looks like. Keeping it all in one table means the renderer itself stays
// simple, and the data is easy to read and explain.
//
// A note on the numbers
// --------------------
// The real distances and sizes are wildly different from each other. Neptune
// is about 30 times further from the sun than Earth is, but its diameter is
// only about 4 times Earth's. Neptune would be a speck.
//
// If we used the real numbers the inner planets would be an invisible pile
// in the middle and the outer ones would be far away dots. So we use
// COMPRESSED numbers that look right on screen:
//
//   * orbit distance is compressed with a square root, which pulls the
//     outer planets closer in while keeping the order the same
//   * planet size is compressed with a power, so the big planets stay big
//     but the small ones can still be seen
//
// The compression factors are named constants below, so it is easy to change
// them and see what happens.

#pragma once

#include "math3d.hpp"

#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// One planet.
// ---------------------------------------------------------------------------
struct Planet
{
    std::string name;            // "Earth", "Mars", ...

    // --- Appearance ---
    math3d::Vec3 base_colour;    // the main colour of the planet
    math3d::Vec3 band_colour;    // the colour of the stripes/bands on it
    float  band_strength;        // how strong the bands are (0 = none)
    float  axial_tilt_degrees;   // how much it leans over
    bool   has_rings;            // Saturn (and a faint hint for Uranus)

    // The look of the rings.
    //
    // Saturn's rings are bright and golden, because they are made of
    // water ice. Uranus's rings are much darker and greyer -- they are
    // narrow bands of dark rock, and you can barely see them.
    math3d::Vec3 ring_colour;
    float  ring_brightness;      // how bright the rings are (0 = none)
    float  ring_inner_radius;    // where the rings start, in planet radii
    float  ring_outer_radius;    // where the rings end, in planet radii

    // --- Size and position ---
    float  radius;               // its size on screen
    float  orbit_radius;         // how far it is from the sun, on screen

    // --- Movement ---
    // These are the two speeds that make the animation interesting:
    //   orbit_speed    -- how fast it travels around the sun
    //   rotation_speed -- how fast it spins on its own axis
    // Both are in radians per second of simulated time.
    float  orbit_speed;
    float  rotation_speed;

    // --- Where it is right now ---
    float  orbit_angle;          // current position on the orbit (radians)
    float  rotation_angle;       // current spin (radians)
    float  laps_completed;      // how many full orbits it has finished
};

// ---------------------------------------------------------------------------
// The sun. It is at the centre of everything and does not move.
// ---------------------------------------------------------------------------
struct Sun
{
    float radius;
    math3d::Vec3 colour;
    float glow_radius;           // how far the corona extends
};

// ---------------------------------------------------------------------------
// The asteroid belt.
//
// The belt is the ring of rocky debris that orbits the sun between Mars
// and Jupiter. It is not a planet and it has no sphere of its own -- it is
// a vast cloud of separate lumps of rock, each on its own orbit.
//
// We draw it as a cloud of points rather than as real geometry, because
// there are tens of thousands of them and each one is far too small to see
// as a shape. As points they read exactly the way the real belt looks:
// a faint, dusty band across the sky.
// ---------------------------------------------------------------------------
struct AsteroidBelt
{
    // How many asteroids to scatter.
    int count = 0;

    // The inner and outer edge of the belt, in the same world units as the
    // planet orbits. The gap between Mars and Jupiter is wide, and the belt
    // does not fill it edge to edge.
    float inner_radius = 0.0f;
    float outer_radius = 0.0f;

    // How thick the belt is. The real belt is very flat -- most of the
    // asteroids stay within a few million kilometres of the plane of the
    // orbits -- so this is small compared with the radius.
    float half_thickness = 0.0f;

    // The point size of each asteroid, and how bright it is. These are the
    // two things that make the belt visible without making it look like a
    // ring of fairy lights.
    float point_size = 1.0f;
    float brightness = 0.5f;
};

// ---------------------------------------------------------------------------
// Builds the whole solar system.
//
// The order is always: sun first, then the planets from Mercury outwards.
// ---------------------------------------------------------------------------
class SolarSystem
{
public:
    SolarSystem();

    const std::vector<Planet>& planets() const { return planets_; }
    const Sun& sun() const { return sun_; }
    const AsteroidBelt& asteroid_belt() const { return asteroid_belt_; }

    // Advances the simulation by the given number of seconds.
    //
    // The angles are wrapped around after a full turn, so they never grow
    // large enough to lose precision (a float loses accuracy after about
    // 16 million, and we might run for hours).
    void update(float delta_seconds);

    // The world-space position of a planet's centre.
    math3d::Vec3 planet_position(const Planet& planet) const;

    // How many times around the sun each planet has completed.
    //
    // We use this for a nice effect: a completed orbit leaves a faint trail
    // of "dust" behind the planet.
    float orbit_laps(const Planet& planet) const { return planet.laps_completed; }

private:
    std::vector<Planet> planets_;
    Sun sun_;
    AsteroidBelt asteroid_belt_;
};

// The compression factors, exposed so the renderer and the console printout
// can refer to them.
namespace solar_scale
{
    // Orbit distances are multiplied by this after the square root.
    inline constexpr float ORBIT_SCALE = 9.0f;

    // Planet radii use this power: radius = pow(real_ratio, SIZE_POWER).
    inline constexpr float SIZE_POWER = 0.45f;

    // The size of the sun relative to a "real" Earth-sized planet.
    inline constexpr float SUN_SCALE = 3.2f;
}
