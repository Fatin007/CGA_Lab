#version 330 core

// asteroid.frag -- Fragment shader for the asteroid belt.
//
// Each asteroid is a single GL_POINT. We make the dot round and soft, the
// same way we do for the stars, but the colour is different: asteroids are
// grey-brown rock, lit by the sun.
//
// gl_PointCoord tells us where inside the dot this fragment is: it runs
// from (0,0) at the bottom-left of the dot to (1,1) at the top-right. We
// use it to make the dot round and soft, instead of the square that GL
// would draw by default.

in float vBrightness;
in float vLit;

out vec4 FragColour;

void main()
{
    // Where are we inside the dot?
    vec2 centred = gl_PointCoord - vec2(0.5);
    float distance_from_centre = length(centred);

    // If we are outside the dot, throw this fragment away. This is the
    // standard trick for turning a square point into a round one.
    if (distance_from_centre > 0.5)
        discard;

    // Fade out towards the edge of the dot, so it has soft edges.
    float falloff = 1.0 - smoothstep(0.0, 0.5, distance_from_centre);
    falloff = 0.40 + 0.60 * falloff;

    // Asteroids are grey-brown rock. We vary it slightly from rock to rock
    // so the belt does not look like a flat grey sheet.
    vec3 rock = vec3(0.62, 0.55, 0.46);
    vec3 dark_rock = vec3(0.38, 0.33, 0.29);

    float variation = fract(vBrightness * 13.37);
    vec3 colour = mix(dark_rock, rock, variation);

    // The lighting. vLit says how much of the sun reaches this rock: 1.0 on
    // the side of the belt that faces the sun, less on the far side.
    //
    // We keep a generous floor under it. The rocks on the far side of the
    // belt are still catching some scattered light, and -- just as
    // importantly -- a rock that is directly in front of a bright planet
    // must not look like a black hole punched through the planet.
    float lighting = 0.55 + 0.45 * vLit;

    FragColour = vec4(colour * vBrightness * lighting * falloff, 1.0);
}
