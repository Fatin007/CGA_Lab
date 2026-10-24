#version 330 core

// ring.frag -- Fragment shader for the planetary rings.
//
// The rings are made of ice and rock in slightly different sizes, so they
// show bands of light and dark. We draw those bands with a noise function,
// exactly like the planet shader.
//
// The rings also cast a shadow on the planet, and the planet casts a shadow
// on the rings. We work out both effects here.

in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec2 vUV;

uniform vec3  uRingColour;
uniform vec3  uSunPosition;
uniform vec3  uEyePosition;
uniform float uPlanetRadius;  // the radius of the planet in the middle
uniform vec3  uPlanetPosition;
uniform float uRingBrightness; // how bright the rings are (0 = none)

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

float noise(vec2 point)
{
    vec2 cell = floor(point);
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
    // --- The bands ---
    //
    // The UV coordinate runs from the inner edge of the ring (0) to the
    // outer edge (1). We sample noise along it to make the bands.
    float bands = noise(vec2(vUV.x * 60.0, 0.5)) * 0.6
                + noise(vec2(vUV.x * 180.0, 2.5)) * 0.4;

    // The famous gap in Saturn's rings -- the Cassini Division.
    //
    // We carve it out by making a narrow band of the ring transparent. The
    // "outside the gap" factor is 1.0 everywhere except in the middle of
    // the division, where it drops to 0.0.
    //
    // The smoothstep goes from 0 to 1 as we move away from the centre of
    // the gap, so:
    //
    //   inside the gap   -> smoothstep = 0 -> the ring is hidden
    //   outside the gap  -> smoothstep = 1 -> the ring is fully visible
    //
    // NOTE: this must NOT be written as (1.0 - smoothstep(...)), or the
    // whole ring turns transparent and only the gap is drawn.
    float gap_centre = 0.62;
    float gap_width  = 0.030;
    float in_gap = smoothstep(gap_width, gap_width * 2.5,
                              abs(vUV.x - gap_centre));

    // The rings fade out towards their inner and outer edges.
    float edge_fade = smoothstep(0.0, 0.06, vUV.x) *
                      (1.0 - smoothstep(0.90, 1.0, vUV.x));

    // --- Lighting ---
    vec3 to_sun = normalize(uSunPosition - vWorldPosition);
    vec3 view_direction = normalize(uEyePosition - vWorldPosition);

    // The ring is a flat sheet of ice. It catches the sunlight on its
    // upper face, so we use the ring's own normal (which leans over with
    // the planet's axial tilt) to work out how much light reaches it.
    //
    // We take the absolute value so that the underside of the ring is lit
    // too -- the sun lights the whole sheet from one side, and we are
    // looking at whichever face happens to be turned towards us.
    vec3 ring_normal = normalize(vWorldNormal);
    float lighting = abs(dot(ring_normal, to_sun));

    // A ring that is edge-on to the sun would be almost black with that
    // alone. Rings are made of millions of separate icy particles that
    // scatter light sideways, so we keep a decent amount of light even when
    // the ring is nearly edge-on.
    lighting = 0.55 + 0.45 * lighting;

    // --- The shadow of the planet on the rings ---
    //
    // We check whether the line from this point to the sun passes through
    // the planet. If it does, the ring is in shadow.
    //
    // The maths: we find the point where the ray to the sun is closest to
    // the centre of the planet. If that closest point is inside the planet
    // and it is between us and the sun, we are in shadow.
    vec3 to_sun_unnormalized = uSunPosition - vWorldPosition;
    float distance_to_sun = length(to_sun_unnormalized);
    vec3 sun_direction = to_sun_unnormalized / distance_to_sun;

    vec3 to_planet = uPlanetPosition - vWorldPosition;

    // How far along the ray to the sun do we get closest to the planet?
    float along = dot(to_planet, sun_direction);

    float shadow = 1.0;

    if (along > 0.0 && along < distance_to_sun)
    {
        // The closest approach to the planet's centre.
        vec3 closest = vWorldPosition + sun_direction * along;
        float distance_to_centre = length(closest - uPlanetPosition);

        if (distance_to_centre < uPlanetRadius)
        {
            // Inside the planet's shadow. The edge is soft.
            //
            // The soft edge is wide on purpose. The sun is not a point but a
            // disc, so the real shadow has a fuzzy edge (the penumbra),
            // and a hard edge looks like a cut-out rather than a shadow.
            shadow = smoothstep(uPlanetRadius * 0.75, uPlanetRadius * 1.10,
                                distance_to_centre);
        }
    }

    // The shadow on the rings is a soft darkening, not a hole.
    //
    // The rings are made of ice particles that scatter light in every
    // direction, so even the shadowed part of the ring still catches some
    // light. If we let the shadow go all the way to black, the ring appears
    // to have a wedge cut out of it, which looks like a bug rather than a
    // shadow.
    shadow = mix(0.35, 1.0, shadow);

    // --- Put it together ---
    //
    // uRingBrightness scales the whole ring. Saturn's rings are bright
    // water ice, so they get 1.0. Uranus's rings are narrow, dark bands
    // of rock, so they get much less -- which is why you can barely see
    // them, just as you cannot in a telescope.
    float brightness = (0.60 + bands * 0.55) * lighting * edge_fade * in_gap * shadow
                     * uRingBrightness;

    // The rings are thin, so we can see through them. The alpha comes from
    // how much ring material is along the line of sight.
    float alpha = clamp(brightness * 1.35, 0.0, 0.92);

    // The colour is the ring's own colour, brightened by the sunlight.
    vec3 colour = uRingColour * brightness * 1.35;

    FragColour = vec4(colour, alpha);
}
