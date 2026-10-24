// solar_system.hpp -- The planet data and the simulation.
//
// This is the whole model of our solar system, in one file. There is no .cpp
// for it, because everything here is small enough to live in the header.
//
// A note on the numbers
// --------------------
// The real solar system is a terrible shape for a picture. Neptune is about
// 30 times further from the sun than Earth, but its radius is only about 4
// times Earth's. Neptune would be a speck, and the inner planets would be an
// invisible pile in the middle.
//
// So we keep the REAL numbers in the table below, and compress them in ONE
// place -- the constructor. That way the table still reads like a fact sheet,
// and the "cheating" is all in one spot we can point at.
//
//   radius        = pow(real_ratio, 0.45)      // sizes
//   orbit_radius  = sqrt(real_AU) * 9.0        // distances
//
// The square root pulls the outer planets in. The power makes the size
// differences smaller, so Jupiter comes out about three times bigger than
// Earth on screen instead of eleven times.

#pragma once

#include "math3d.hpp"

#include <cmath>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// One planet.
// ---------------------------------------------------------------------------
struct Planet
{
    std::string name;

    // --- How it looks ---
    math3d::Vec3 base_colour;      // the main colour
    math3d::Vec3 band_colour;      // the colour of the stripes on it
    float        band_strength;    // how strong the stripes are (0 = none)
    float        axial_tilt;       // how far it leans over, in degrees

    bool         has_rings;
    math3d::Vec3 ring_colour;
    float        ring_brightness;  // 0 = no rings
    float        ring_inner;       // inner edge, in planet radii
    float        ring_outer;       // outer edge, in planet radii

    // --- How big and where ---
    float        radius;           // its size on screen
    float        orbit_radius;     // how far it is from the sun, on screen

    // --- How it moves ---
    float orbit_speed;             // radians per second around the sun
    float rotation_speed;           // radians per second on its own axis

    // --- Where it is right now ---
    float orbit_angle;              // current position on the orbit
    float rotation_angle;           // current spin
};

// ---------------------------------------------------------------------------
// The sun. It sits in the middle of everything and does not move.
// ---------------------------------------------------------------------------
struct Sun
{
    float        radius;
    math3d::Vec3 colour;
    float        glow_radius;       // how far the corona reaches
};

// ---------------------------------------------------------------------------
// The asteroid belt.
//
// The belt is the ring of rocky debris between Mars and Jupiter. It is not a
// planet -- it is thousands of separate lumps of rock, each on its own
// orbit. We draw it as a cloud of points, because the real asteroids are far
// too small to see as shapes.
// ---------------------------------------------------------------------------
struct AsteroidBelt
{
    int   count;             // how many asteroids
    float inner_radius;      // the inner edge of the belt
    float outer_radius;      // the outer edge
    float half_thickness;     // how thick the belt is
    float point_size;         // the size of each dot, in pixels
    float brightness;         // how bright the rocks are
};

// ---------------------------------------------------------------------------
// Builds the whole solar system.
// ---------------------------------------------------------------------------
class SolarSystem
{
public:
    SolarSystem();

    const std::vector<Planet>& planets() const { return planets_; }
    const Sun& sun() const { return sun_; }
    const AsteroidBelt& asteroid_belt() const { return asteroid_belt_; }

    // Advances the simulation by the given number of seconds.
    void update(float delta_seconds);

    // Where a planet's centre is in the world right now.
    math3d::Vec3 planet_position(const Planet& planet) const;

private:
    std::vector<Planet> planets_;
    Sun        sun_;
    AsteroidBelt asteroid_belt_;
};

// ---------------------------------------------------------------------------
// The implementation.
//
// Everything below is short enough to sit in the header. Putting it here
// means the whole model of the solar system can be read in one go, without
// having to jump between two files.
// ---------------------------------------------------------------------------
inline SolarSystem::SolarSystem()
{
    // How long one Earth year takes in simulated seconds.
    //
    // At this speed Neptune (which takes 165 Earth years) would need about
    // 990 seconds to go round once, which is far too slow to look good. So
    // we speed the outer planets up. The order stays the same -- Mercury is
    // still the fastest, Neptune still the slowest -- but the animation is
    // watchable.
    constexpr float YEAR_IN_SECONDS = 6.0f;

    // How long one Earth day takes in simulated seconds.
    constexpr float DAY_IN_SECONDS = 1.6f;

    // The compression factors. These are the only "magic numbers" in the
    // project, and they live together so they are easy to change.
    constexpr float ORBIT_SCALE = 9.0f;   // orbit distance multiplier
    constexpr float SIZE_POWER  = 0.45f;  // size compression exponent

    // ---------------------------------------------------------------------
    // THE PLANET TABLE
    //
    // This is the real data: real radii, real distances, real year lengths,
    // real day lengths, real axial tilts. Venus and Uranus really do spin
    // backwards, and the code keeps the negative sign.
    //
    //   name      radius  dist   year    day      tilt  rings
    //   |         |       |      |       |        |     |
    //   |         |       |      |       |        |     +-- has rings?
    //   |         |       |      |       |        +-------- lean, degrees
    //   |         |       |      |       |        +----------------- day length, hours
    //   |         |       |      |       +------------------------- year length, Earth years
    //   |         |       |      +------------------------------- distance, AU
    //   |         |       +--------------------------------------- radius, in Earth radii
    //   +------------------------------------------------- the name
    // ---------------------------------------------------------------------
    struct PlanetData
    {
        const char* name;
        float radius_ratio;    // radius compared to Earth
        float distance_au;     // distance from the sun, in AU
        float year_length;     // one orbit, in Earth years
        float day_hours;       // one rotation, in hours
        float axial_tilt;      // degrees
        bool  has_rings;

        math3d::Vec3 base_colour;
        math3d::Vec3 band_colour;
        float band_strength;

        // The rings. Only used when has_rings is true.
        math3d::Vec3 ring_colour;
        float ring_brightness;
        float ring_inner;
        float ring_outer;
    };

    // The order is Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus,
    // Neptune -- from the inside out.
    const PlanetData PLANET_DATA[] = {
        // name       radius  dist  year   day     tilt   rings  base colour        band colour        band   ring colour         ring   inner  outer
        { "Mercury", 0.383f, 0.39f, 0.241f, 1408.0f,  0.0f, false,
          {0.55f, 0.50f, 0.45f}, {0.42f, 0.38f, 0.35f}, 0.20f,
          {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f },

        { "Venus",   0.949f, 0.72f, 0.615f, -5832.0f,  2.6f, false,
          {0.85f, 0.72f, 0.45f}, {0.95f, 0.85f, 0.60f}, 0.55f,
          {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f },

        { "Earth",   1.000f, 1.00f, 1.000f,   24.0f, 23.4f, false,
          {0.20f, 0.42f, 0.85f}, {0.30f, 0.60f, 0.35f}, 0.60f,
          {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f },

        { "Mars",    0.532f, 1.52f, 1.881f,   24.6f, 25.2f, false,
          {0.78f, 0.36f, 0.20f}, {0.55f, 0.25f, 0.15f}, 0.45f,
          {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f },

        { "Jupiter", 11.21f, 5.20f, 11.86f,    9.9f,  3.1f, false,
          {0.85f, 0.72f, 0.55f}, {0.65f, 0.50f, 0.38f}, 0.65f,
          {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f },

        { "Saturn",  9.45f, 9.58f, 29.46f,   10.7f, 26.7f, true,
          {0.85f, 0.75f, 0.52f}, {0.70f, 0.62f, 0.45f}, 0.55f,
          {0.88f, 0.80f, 0.62f}, 1.00f, 1.25f, 2.20f },

        { "Uranus",  4.01f, 19.20f, 84.01f,   17.2f, 97.8f, true,
          {0.45f, 0.75f, 0.80f}, {0.50f, 0.80f, 0.85f}, 0.30f,
          {0.50f, 0.53f, 0.58f}, 0.42f, 1.55f, 2.05f },

        { "Neptune", 3.88f, 30.05f, 164.8f,   16.1f, 28.3f, false,
          {0.25f, 0.35f, 0.85f}, {0.35f, 0.45f, 0.95f}, 0.35f,
          {0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f },
    };

    constexpr int PLANET_COUNT = sizeof(PLANET_DATA) / sizeof(PLANET_DATA[0]);

    // --- The sun ---
    sun_.radius      = 1.6f;
    sun_.colour      = math3d::Vec3(1.0f, 0.93f, 0.70f);
    sun_.glow_radius = 8.5f;

    // --- The planets ---
    planets_.reserve(PLANET_COUNT);

    for (int i = 0; i < PLANET_COUNT; ++i)
    {
        const PlanetData& data = PLANET_DATA[i];

        Planet planet;
        planet.name              = data.name;
        planet.base_colour       = data.base_colour;
        planet.band_colour       = data.band_colour;
        planet.band_strength     = data.band_strength;
        planet.axial_tilt        = data.axial_tilt;
        planet.has_rings         = data.has_rings;
        planet.ring_colour       = data.ring_colour;
        planet.ring_brightness   = data.ring_brightness;
        planet.ring_inner        = data.ring_inner;
        planet.ring_outer        = data.ring_outer;

        // --- Compress the size ---
        //
        // radius = pow(real_ratio, SIZE_POWER)
        //
        // With the power at 0.45, Jupiter (11 times Earth's radius) comes
        // out about 3 times bigger on screen instead of 11 times, which
        // keeps Mercury visible next to it.
        planet.radius = std::pow(data.radius_ratio, SIZE_POWER);

        // --- Compress the orbit distance ---
        //
        // distance = sqrt(real_distance_in_AU) * ORBIT_SCALE
        //
        // The square root pulls the outer planets in. Neptune is 30 AU away
        // and Mercury is 0.39; after the square root that becomes about 5.5
        // and 0.6, so the spacing looks reasonable.
        planet.orbit_radius = std::sqrt(data.distance_au) * ORBIT_SCALE;

        // --- The speeds ---
        //
        // An orbit takes year_length Earth years, so one full turn is
        // 2 * PI radians over (year_length * YEAR_IN_SECONDS) seconds.
        planet.orbit_speed = (2.0f * 3.14159265f) / (data.year_length * YEAR_IN_SECONDS);

        // A rotation takes day_hours hours. Negative values (Venus and
        // Uranus) mean the planet spins backwards, which is real -- so we
        // keep the sign.
        planet.rotation_speed =
            (2.0f * 3.14159265f) / (data.day_hours * (DAY_IN_SECONDS / 24.0f));

        // --- Where they start ---
        //
        // We spread the planets out around the sun using the GOLDEN ANGLE,
        // 137.5 degrees. It is called golden because it gives the most even
        // spread when you keep stepping by it.
        //
        // This matters more than it sounds. If two planets start at nearly
        // the same angle they line up with the sun, and from a low camera
        // angle they collapse into the same few pixels -- they look like
        // they are touching even though they are millions of miles apart.
        // Stepping by 137.5 degrees means no two of the eight planets are
        // anywhere near each other.
        planet.orbit_angle    = static_cast<float>(i) * 2.39996323f;
        planet.rotation_angle = static_cast<float>(i) * 0.7f;

        planets_.push_back(planet);
    }

    // --- The asteroid belt ---
    //
    // It fills the gap between Mars and Jupiter. We take the two orbit
    // radii we have just worked out and fill the space between them with a
    // flattened ring of rock.
    const float mars_orbit    = planets_[3].orbit_radius;
    const float jupiter_orbit = planets_[4].orbit_radius;

    asteroid_belt_.count          = 9000;
    asteroid_belt_.inner_radius   = mars_orbit + (jupiter_orbit - mars_orbit) * 0.22f;
    asteroid_belt_.outer_radius   = mars_orbit + (jupiter_orbit - mars_orbit) * 0.72f;
    asteroid_belt_.half_thickness = 0.55f;
    asteroid_belt_.point_size     = 1.3f;
    asteroid_belt_.brightness     = 0.85f;
}

inline void SolarSystem::update(float delta_seconds)
{
    const float full_turn = 2.0f * 3.14159265f;

    for (Planet& planet : planets_)
    {
        planet.orbit_angle    += planet.orbit_speed * delta_seconds;
        planet.rotation_angle += planet.rotation_speed * delta_seconds;

        // Wrap the angles back into the range 0 .. 2*PI.
        //
        // Without this they would grow for as long as the program runs, and
        // a float slowly loses accuracy as it gets bigger -- the planets
        // would start to judder.
        if (planet.orbit_angle > full_turn)
            planet.orbit_angle -= full_turn;
        if (planet.rotation_angle > full_turn)
            planet.rotation_angle -= full_turn;

        // The same for a negative angle (a backwards-spinning planet).
        if (planet.orbit_angle < 0.0f)
            planet.orbit_angle += full_turn;
        if (planet.rotation_angle < 0.0f)
            planet.rotation_angle += full_turn;
    }
}

inline math3d::Vec3 SolarSystem::planet_position(const Planet& planet) const
{
    // The planet sits on its orbit circle, in the XZ plane (the plane of
    // the ecliptic).
    return math3d::Vec3(
        planet.orbit_radius * std::cos(planet.orbit_angle),
        0.0f,
        planet.orbit_radius * std::sin(planet.orbit_angle));
}
