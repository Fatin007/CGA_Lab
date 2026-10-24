#version 330 core

// star.frag -- Fragment shader for the background stars.
//
// Each star is a single GL_POINT. The fragment shader decides what colour
// that dot is.
//
// gl_PointCoord tells us where inside the dot this fragment is: it runs
// from (0,0) at the bottom-left of the dot to (1,1) at the top-right. We
// use it to make the dot round and soft, instead of the square that GL
// would draw by default.

in float vBrightness;

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
    falloff = 0.35 + 0.65 * falloff;

    // Stars are not pure white. Some are a little blue, some a little
    // orange. We pick a colour based on where the star is, so the sky has
    // a natural mix of colours.
    vec3 blue_white = vec3(0.75, 0.85, 1.0);
    vec3 warm_white = vec3(1.0, 0.90, 0.75);
    vec3 white      = vec3(1.0, 1.0, 1.0);

    float tint = fract(sin(centred.x * 91.7 + centred.y * 47.3) * 4137.13);
    vec3 colour = mix(blue_white, warm_white, tint * tint);
    colour = mix(colour, white, 0.4);

    FragColour = vec4(colour * vBrightness * falloff, falloff);
}
