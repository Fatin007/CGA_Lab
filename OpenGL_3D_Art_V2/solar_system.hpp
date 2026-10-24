// solar_system.hpp -- The planet data and the simulation.
//
// Everything the renderer needs to know about a planet is in this file: how
// big it is, how far it is from the sun, how fast it moves, and what it looks
// like.
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
