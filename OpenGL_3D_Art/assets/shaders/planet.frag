#version 330 core

// planet.frag -- Fragment shader for the planets.
//
// This shader does most of the visual work:
//
//   1. Procedural surface bands -- gas giants have stripes, rocky planets
//      have craters and continents. We fake both with simple maths on the
//      UV coordinates, so we need no texture files at all.
//
//   2. A day/night terminator -- the sun is a real light source at the
//      centre of the scene, so the side of the planet facing away from it
//      is dark.
//
//   3. An atmospheric rim -- a soft glow around the edge of the planet,
//      which is what makes it look like a ball of gas rather than a flat
//      disc.
//
//   4. A specular highlight for the oceans of Earth-like worlds.

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUV;

uniform vec3  uBaseColour;     // the main colour of the planet
uniform vec3  uBandColour;     // the colour of the bands on it
uniform float uBandStrength;   // how strong the bands are (0 = no bands)
uniform vec3  uSunPosition;    // where the sun is
uniform vec3  uEyePosition;    // where the camera is
uniform float uShininess;      // how sharp the specular highlight is
uniform float uAtmosphere;     // how strong the rim glow is

out vec4 FragColour;

// ---------------------------------------------------------------------------
// A cheap "random" function.
//
// We use it to break up the surface so it does not look like flat bands.
//
// The usual version of this function is
//
//     fract(sin(dot(point, vec2(127.1, 311.7))) * 43758.5453123)
//
// but we do NOT use that here. On some graphics cards (the Intel one in the
// lab, for one) sin() of a large number loses so much precision that the
// result comes out in visible square blocks instead of smooth noise.
//
// So we use an integer hash instead. It does the same job -- it turns a
// 2D point into a number between 0 and 1 that looks random -- but it is
// exact, because integer maths has no rounding error.
// ---------------------------------------------------------------------------
float hash(vec2 point)
{
    // The constants are from a well known "hash without sine" trick.
    // We turn the point into an integer, mix the bits around, and pull a
    // number back out of the result.
    vec3 point3 = fract(vec3(point.xyx) * 0.1031);
    point3 += dot(point3, point3.yzx + 33.33);
    return fract((point3.x + point3.y) * point3.z);
}

// ---------------------------------------------------------------------------
// Smooth noise.
//
// Noise is what makes a surface look natural. A single hash gives random
// dots; smooth noise gives soft, cloudy patches.
//
// The trick is to look at the four corners of a grid cell, give each one a
// random value, and blend them together smoothly. The "smoothstep" makes
// the blend gradual, so the result has no hard edges.
// ---------------------------------------------------------------------------
float noise(vec2 point)
{
    vec2 cell = floor(point);
    vec2 local = fract(point);

    // Smooth the local coordinates so the corners blend in gradually.
    vec2 blend = local * local * (3.0 - 2.0 * local);

    float bottom_left  = hash(cell);
    float bottom_right = hash(cell + vec2(1.0, 0.0));
    float top_left     = hash(cell + vec2(0.0, 1.0));
    float top_right    = hash(cell + vec2(1.0, 1.0));

    // Blend the four corners.
    float bottom = mix(bottom_left, bottom_right, blend.x);
    float top    = mix(top_left, top_right, blend.x);

    return mix(bottom, top, blend.y);
}

// ---------------------------------------------------------------------------
// How many "layers" of noise to add.
//
// Each layer is twice as small and twice as strong as the one below it.
// Adding several layers gives a richer, more natural looking surface.
// ---------------------------------------------------------------------------
float fbm(vec2 point)
{
    float total = 0.0;
    float amplitude = 0.5;

    for (int i = 0; i < 4; ++i)
    {
        total += noise(point) * amplitude;
        point *= 2.0;        // twice as small
        amplitude *= 0.5;    // half as strong
    }

    return total;
}

void main()
{
    // --- The surface colour ---
    //
    // The latitude is v (0 at the north pole, 1 at the south pole), and the
    // longitude is u (0 to 1 around the equator).
    float latitude = vUV.y;
    float longitude = vUV.x;

    // The bands are horizontal stripes. Their width is controlled by this
    // number -- a bigger number means more, thinner stripes.
    float band_pattern = sin(latitude * 28.0 + fbm(vec2(longitude * 4.0, latitude * 8.0)) * 3.0);

    // Turn the sine wave (-1 to 1) into a 0 to 1 value, and sharpen it so
    // the bands have defined edges instead of fading into each other.
    float bands = smoothstep(-0.2, 0.2, band_pattern);

    // The fine detail: noise that breaks up the bands.
    float detail = fbm(vec2(longitude * 12.0, latitude * 12.0));

    // Start with the base colour and mix in the band colour.
    vec3 surface = mix(uBaseColour, uBandColour, bands * uBandStrength);

    // Add a little of the noise detail on top, both to lighten and to
    // darken. This stops the planet looking like flat plastic.
    surface *= 0.85 + detail * 0.35;

    // --- Lighting ---
    vec3 normal = normalize(vNormal);
    vec3 to_sun = uSunPosition - vWorldPosition;
    vec3 sun_direction = normalize(to_sun);

    // How much light this point receives. The day side is bright, the night
    // side is dark, and the edge in between is the terminator.
    float diffuse = max(dot(normal, sun_direction), 0.0);

    // A softer, wider falloff than a hard cut-off. This is what makes the
    // terminator look like a smooth curve instead of a sharp edge.
    diffuse = diffuse * diffuse * (3.0 - 2.0 * diffuse);

    // The ambient light.
    //
    // In space there is almost no ambient light, so a real photograph of a
    // planet shows a very dark night side. That is correct, but it makes
    // the outer planets look like black dots in a wide shot, which is not
    // much fun for an art project. So we use a generous amount of ambient
    // light: enough to see the shape of the planet on the dark side, but
    // still clearly darker than the day side.
    vec3 ambient = vec3(0.10, 0.11, 0.16);

    // The specular highlight. Only strong on shiny surfaces (oceans).
    vec3 view_direction = normalize(uEyePosition - vWorldPosition);
    vec3 half_vector = normalize(sun_direction + view_direction);
    float specular = pow(max(dot(normal, half_vector), 0.0), uShininess);

    // --- The atmosphere ---
    //
    // The rim is brightest where the surface turns away from us, which is
    // exactly the edge of the planet as we see it. We use the dot product
    // between the normal and the view direction: when they are opposite,
    // we are looking at the edge.
    float rim = 1.0 - max(dot(normal, view_direction), 0.0);
    rim = pow(rim, 3.0);

    // Put it all together.
    //
    // The 1.15 gives the planets a little extra brightness. The raw lighting
    // maths is correct but comes out quite dark, and the point of this
    // project is to show the planets off.
    vec3 colour = surface * (ambient + diffuse) * 1.15;

    // --- The fill light ---
    //
    // A planet on the far side of its orbit has its whole night side turned
    // away from the sun, so it would be a black disc. In real space it
    // really is nearly invisible -- but for a picture we want to be able to
    // see that the planet is there.
    //
    // So we add a weak light that comes from the camera's direction. It is
    // much weaker than the sunlight, so the day side still looks properly
    // lit and the terminator still reads clearly.
    float fill = max(dot(normal, view_direction), 0.0) * 0.16;
    colour += surface * fill;

    // The specular highlight, scaled by the diffuse term so it does not
    // appear on the night side.
    colour += vec3(1.0, 0.95, 0.85) * specular * 0.35 * diffuse;

    // The atmospheric rim. It uses the sun colour so the lit edge glows
    // warmly and the dark edge does not.
    colour += uBaseColour * rim * uAtmosphere * (0.25 + diffuse * 0.75);

    FragColour = vec4(colour, 1.0);
}
