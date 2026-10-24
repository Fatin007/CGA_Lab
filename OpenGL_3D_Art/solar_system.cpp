// solar_system.cpp -- The data of our solar system.

#include "solar_system.hpp"

#include <cmath>

namespace
{
    // A small helper so the table below stays readable.
    constexpr float DEG = 3.14159265f / 180.0f;

    // Real orbital periods, in Earth years, and real relative distances.
    //
    // We store the real numbers here and compress them in the constructor,
    // so the table reads like a fact sheet rather than a pile of magic
    // numbers.
    struct PlanetData
    {
        const char* name;
        float radius_ratio;        // radius compared to Earth
        float distance_au;         // distance from the sun, in AU
        float year_length;          // how long one orbit takes, in Earth years
        float day_hours;            // how long one rotation takes, in hours
        float axial_tilt;           // degrees
        bool  has_rings;

        math3d::Vec3 base_colour;
        math3d::Vec3 band_colour;
        float band_strength;
    };

    // The order is Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus,
    // Neptune -- from the inside out.
    const PlanetData PLANET_DATA[] = {
        // name       radius  dist  year   day     tilt   rings  base colour        band colour        band
        { "Mercury", 0.383f, 0.39f, 0.241f, 1408.0f,  0.0f, false,
          {0.55f, 0.50f, 0.45f}, {0.42f, 0.38f, 0.35f}, 0.20f },

        { "Venus",   0.949f, 0.72f, 0.615f, -5832.0f,  2.6f, false,
          {0.85f, 0.72f, 0.45f}, {0.95f, 0.85f, 0.60f}, 0.55f },

        { "Earth",   1.000f, 1.00f, 1.000f,   24.0f, 23.4f, false,
          {0.20f, 0.42f, 0.85f}, {0.30f, 0.60f, 0.35f}, 0.60f },

        { "Mars",    0.532f, 1.52f, 1.881f,   24.6f, 25.2f, false,
          {0.78f, 0.36f, 0.20f}, {0.55f, 0.25f, 0.15f}, 0.45f },

        { "Jupiter", 11.21f, 5.20f, 11.86f,    9.9f,  3.1f, false,
          {0.85f, 0.72f, 0.55f}, {0.65f, 0.50f, 0.38f}, 0.65f },

        { "Saturn",  9.45f, 9.58f, 29.46f,   10.7f, 26.7f, true,
          {0.85f, 0.75f, 0.52f}, {0.70f, 0.62f, 0.45f}, 0.55f },

        { "Uranus",  4.01f, 19.20f, 84.01f,   17.2f, 97.8f, true,
          {0.45f, 0.75f, 0.80f}, {0.50f, 0.80f, 0.85f}, 0.30f },

        { "Neptune", 3.88f, 30.05f, 164.8f,   16.1f, 28.3f, false,
          {0.25f, 0.35f, 0.85f}, {0.35f, 0.45f, 0.95f}, 0.35f },
    };

    constexpr int PLANET_COUNT = sizeof(PLANET_DATA) / sizeof(PLANET_DATA[0]);

    // How long one Earth year takes in simulated seconds.
    //
    // At this speed Neptune (which takes 165 Earth years) needs about 165 *
    // 6 seconds = 990 seconds to go round once, which is far too slow to
    // look good. So we speed the outer planets up. The order stays the
    // same (Mercury is still the fastest, Neptune still the slowest), but
    // the animation is watchable.
    constexpr float YEAR_IN_SECONDS = 6.0f;

    // How long one Earth day takes in simulated seconds.
    constexpr float DAY_IN_SECONDS = 1.6f;
}

SolarSystem::SolarSystem()
{
    sun_.radius      = 1.6f;
    sun_.colour      = math3d::Vec3(1.0f, 0.93f, 0.70f);

    // How far the corona (the glow around the sun) reaches, in the same
    // units as the sun's radius. It is much bigger than the sun itself,
    // which is what gives the sun its soft halo.
    sun_.glow_radius = 8.5f;

    planets_.reserve(PLANET_COUNT);

    for (int i = 0; i < PLANET_COUNT; ++i)
    {
        const PlanetData& data = PLANET_DATA[i];

        Planet planet;
        planet.name             = data.name;
        planet.base_colour      = data.base_colour;
        planet.band_colour      = data.band_colour;
        planet.band_strength    = data.band_strength;
        planet.axial_tilt_degrees = data.axial_tilt;
        planet.has_rings        = data.has_rings;

        // --- Compress the size ---
        //
        // radius = pow(ratio, SIZE_POWER)
        //
        // The power makes the difference between the planets smaller. With
        // the power at 0.45, Jupiter (11 times Earth's radius) comes out
        // about 3 times bigger on screen instead of 11 times, which keeps
        // Mercury visible next to it.
        planet.radius = std::pow(data.radius_ratio, solar_scale::SIZE_POWER);

        // --- Compress the orbit distance ---
        //
        // distance = sqrt(distance_in_AU) * ORBIT_SCALE
        //
        // The square root pulls the outer planets in. Neptune is 30 AU away
        // and Mercury is 0.39; after the square root that becomes about
        // 5.5 and 0.6, so the spacing looks reasonable.
        planet.orbit_radius = std::sqrt(data.distance_au) * solar_scale::ORBIT_SCALE;

        // --- The speeds ---
        //
        // An orbit takes year_length Earth years, so one full turn is
        // 2 * PI radians over (year_length * YEAR_IN_SECONDS) seconds.
        const float orbit_seconds = data.year_length * YEAR_IN_SECONDS;
        planet.orbit_speed = (2.0f * 3.14159265f) / orbit_seconds;

        // A rotation takes day_hours hours. Negative values (Venus and
        // Uranus) mean the planet spins backwards, which is real -- so we
        // keep the sign.
        const float rotation_seconds = data.day_hours * (DAY_IN_SECONDS / 24.0f);
        planet.rotation_speed = (2.0f * 3.14159265f) / rotation_seconds;

        // --- Starting positions ---
        //
        // We spread the planets out around the sun so they do not all start
        // in a line. Each one starts at a slightly different angle.
        planet.orbit_angle    = static_cast<float>(i) * 1.1f;
        planet.rotation_angle = static_cast<float>(i) * 0.7f;
        planet.laps_completed = 0.0f;

        planets_.push_back(planet);
    }
}

void SolarSystem::update(float delta_seconds)
{
    const float full_turn = 2.0f * 3.14159265f;

    for (Planet& planet : planets_)
    {
        // Remember where we were before, so we can tell when the planet
        // has just finished a lap and crossed back over the start.
        const float angle_before = planet.orbit_angle;

        planet.orbit_angle += planet.orbit_speed * delta_seconds;
        planet.rotation_angle += planet.rotation_speed * delta_seconds;

        // Count a completed lap when the angle wraps past a full turn.
        if (planet.orbit_angle - angle_before >= full_turn)
            planet.laps_completed += 1.0f;

        // Wrap the angles back into the range 0 .. 2*PI. Without this they
        // would grow for as long as the program runs, and a float slowly
        // loses accuracy as it gets bigger -- the planets would start to
        // judder.
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

math3d::Vec3 SolarSystem::planet_position(const Planet& planet) const
{
    // The planet sits on its orbit circle, in the XZ plane (the plane of
    // the ecliptic).
    return math3d::Vec3(
        planet.orbit_radius * std::cos(planet.orbit_angle),
        0.0f,
        planet.orbit_radius * std::sin(planet.orbit_angle));
}
