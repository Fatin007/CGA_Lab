#version 330 core

// point.frag -- Fragment shader for every dot in the scene.
//
// Each dot is a single GL_POINT. OpenGL would draw that point as a square,
// so we use gl_PointCoord to make it round instead.
//
// gl_PointCoord tells us where inside the dot this fragment is: it runs
// from (0,0) at the bottom-left of the dot to (1,1) at the top-right.
//
// The stars and the asteroids share this shader, and uIsStar says which one
// we are drawing.

in float vBrightness;
in float vLit;

uniform int uIsStar;   // 1 = background star, 0 = asteroid

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
    //
    // We do NOT square this. A star is only one or two pixels across, so
    // squaring would leave almost every pixel of it nearly black, and the
    // sky would look empty.
    float falloff = 1.0 - smoothstep(0.0, 0.5, distance_from_centre);

    if (uIsStar == 1)
    {
        falloff = 0.35 + 0.65 * falloff;

        // Stars are not pure white. Some are a little blue, some a little
        // orange. We pick a colour based on where the star is, so the sky
        // has a natural mix of colours.
        vec3 blue_white = vec3(0.75, 0.85, 1.0);
        vec3 warm_white = vec3(1.0, 0.90, 0.75);
        vec3 white      = vec3(1.0, 1.0, 1.0);

        float tint = fract(sin(centred.x * 91.7 + centred.y * 47.3) * 4137.13);
        vec3 colour = mix(blue_white, warm_white, tint * tint);
        colour = mix(colour, white, 0.4);

        FragColour = vec4(colour * vBrightness * falloff, falloff);
        return;
    }

    // --- Asteroids ---
    falloff = 0.40 + 0.60 * falloff;

    // Asteroids are grey-brown rock. We vary it slightly from rock to rock
    // so the belt does not look like a flat grey sheet.
    vec3 rock      = vec3(0.62, 0.55, 0.46);
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
