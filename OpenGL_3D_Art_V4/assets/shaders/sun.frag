#version 330 core

// sun.frag -- Fragment shader for the sun.
//
// The sun does not reflect light -- it MAKES light. So it is not lit by the
// same lighting model as the planets. Instead it is fully bright everywhere,
// with a hot, uneven surface.
//
// The same shader also draws the corona (the glow around the sun) when the
// uniform uIsCorona is set. The corona is a flat billboard quad, and we use
// the UV coordinates to work out how far each fragment is from the centre,
// so we can fade it out towards the edges.

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUV;

uniform vec3  uSunColour;      // the colour of the sun
uniform float uTime;           // seconds, for the flickering surface
uniform int   uIsCorona;       // 1 = this is the glow, 0 = this is the sun
uniform float uGlowStrength;   // how bright the corona is
uniform float uSurfaceFraction; // where the sun's edge sits on the quad (0..1)

out vec4 FragColour;

// A cheap "random" function.
//
// This is the same integer-based hash the planet shader uses. We do not use
// the usual sin()-based version, because on some graphics cards it produces
// visible square blocks instead of smooth noise.
float hash(vec2 point)
{
    vec3 point3 = fract(vec3(point.xyx) * 0.1031);
    point3 += dot(point3, point3.yzx + 33.33);
    return fract((point3.x + point3.y) * point3.z);
}

// Smooth noise: look at the four corners of a grid cell, give each one a
// random value, and blend them together smoothly.
float noise(vec2 point)
{
    vec2 cell  = floor(point);
    vec2 local = fract(point);
    vec2 blend = local * local * (3.0 - 2.0 * local);

    float bottom_left  = hash(cell);
    float bottom_right = hash(cell + vec2(1.0, 0.0));
    float top_left     = hash(cell + vec2(0.0, 1.0));
    float top_right    = hash(cell + vec2(1.0, 1.0));

    float bottom = mix(bottom_left, bottom_right, blend.x);
    float top    = mix(top_left, top_right, blend.x);

    return mix(bottom, top, blend.y);
}

void main()
{
    // ------------------------------------------------------------------
    // The corona: a flat glow that fades out towards its edges.
    // ------------------------------------------------------------------
    if (uIsCorona == 1)
    {
        // How far from the centre of the quad are we? The UV runs from 0 to
        // 1, so the centre is at 0.5 and the distance is 0.5 at the edge.
        vec2 centred = vUV - vec2(0.5);
        float distance_from_centre = length(centred) * 2.0;

        // Where does the sun's own surface sit, as a fraction of the quad?
        //
        // The corona quad is bigger than the sun, so the middle part of it
        // is hidden BEHIND the sun's surface. Only the ring of glow outside
        // the sun can actually be seen. uSurfaceFraction tells us where
        // that visible ring starts.
        //
        // The glow therefore fades out from the EDGE OF THE SUN outwards,
        // rather than from the centre. If we faded it out from the centre,
        // most of the light would be spent in the part we cannot see, and
        // the corona would look far too faint.
        float visible = max(distance_from_centre - uSurfaceFraction, 0.0);
        float span = max(1.0 - uSurfaceFraction, 0.001);

        // Fade out towards the edge of the quad.
        //
        // The glow is at its brightest right where it leaves the sun's
        // surface, and fades away smoothly as it spreads out. We use a
        // smoothstep for the fade, and then a gentle power curve to keep
        // the light close to the sun for a little while -- that is what
        // makes it look like a corona rather than a flat disc of light.
        float falloff = 1.0 - smoothstep(0.0, span, visible);
        falloff = pow(falloff, 1.5);

        // A second, wider and much fainter halo, so the glow does not end
        // abruptly at the edge of the quad.
        float halo = 1.0 - smoothstep(0.0, 1.0, distance_from_centre);
        halo = pow(halo, 3.0) * 0.35;

        // A little bit of noise makes the glow look like it is flickering
        // rather than being a perfect circle.
        float flicker = 0.85 + 0.15 * noise(vUV * 8.0 + uTime * 0.5);

        // The centre of the glow is white-hot, and it fades to the sun's
        // colour at the edges.
        vec3 glow = mix(uSunColour, vec3(1.0, 1.0, 0.95), falloff);

        // The glow fades out to fully transparent, so we can blend it over
        // the background without a visible edge.
        float alpha = (falloff + halo) * uGlowStrength * flicker;

        FragColour = vec4(glow * alpha, alpha);
        return;
    }

    // ------------------------------------------------------------------
    // The sun itself.
    // ------------------------------------------------------------------
    //
    // The surface is fully bright -- it does not need a light source. We
    // add some noise so it looks like a churning ball of fire rather than
    // a flat yellow ball.
    float surface = noise(vUV * 9.0 + vec2(uTime * 0.08, uTime * 0.05));
    surface = surface * 0.5
            + noise(vUV * 22.0 - vec2(uTime * 0.13, 0.0)) * 0.3;

    // Brighter in the middle, a bit deeper orange at the edges.
    vec3 hot   = vec3(1.0, 0.98, 0.88);
    vec3 cool  = uSunColour * 0.85;
    vec3 colour = mix(cool, hot, surface);

    FragColour = vec4(colour, 1.0);
}
