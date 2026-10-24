#version 330 core

// point.vert -- Vertex shader for every dot in the scene.
//
// The scene has two kinds of dot:
//
//   * The background stars. They live on a huge sphere around the camera, so
//     they never appear to move as the camera moves.
//
//   * The asteroids in the belt. They live in real world space between Mars
//     and Jupiter, and the whole belt turns slowly around the sun.
//
// One shader draws both, and a single uniform (uIsStar) says which one we
// are drawing. That keeps the number of shader programs down.
//
// The layout is the same for both: position, brightness and size.

layout(location = 0) in vec3 aPosition;   // where the dot is
layout(location = 1) in float aBrightness; // how bright it is
layout(location = 2) in float aSize;       // how big the dot should be

uniform mat4  uView;
uniform mat4  uProjection;
uniform float uTime;
uniform int   uIsStar;        // 1 = background star, 0 = asteroid

out float vBrightness;
out float vLit;              // how much sun reaches it (asteroids only)

void main()
{
    vec3 position = aPosition;

    // --- Stars ---
    //
    // The stars are placed on a sphere of a fixed huge radius, with the
    // camera in the middle of it. So we take the view matrix but throw away
    // its translation -- that keeps the sky infinitely far away.
    //
    // We do the same for the rotation: the sky should not spin round with
    // the camera, so we build the view matrix from its rotation part only.
    mat4 rotation_only = uView;
    rotation_only[3].xyz = vec3(0.0, 0.0, 0.0);

    // --- Asteroids ---
    //
    // The belt turns around the sun. We rotate the asteroid's position about
    // the Y axis, which is the axis the planets orbit around.
    float angle = uTime * 0.02;
    float c = cos(angle);
    float s = sin(angle);

    position = vec3(aPosition.x * c - aPosition.z * s,
                     aPosition.y,
                     aPosition.x * s + aPosition.z * c);

    // A star keeps its place on the sky; an asteroid is in world space.
    vec4 view_position = uIsStar == 1
                       ? rotation_only * vec4(position, 1.0)
                       : uView * vec4(position, 1.0);

    gl_Position = uProjection * view_position;

    // --- The size of the dot ---
    if (uIsStar == 1)
    {
        // A little twinkle makes the sky feel alive: each star breathes at
        // its own rate.
        float twinkle = 0.85 + 0.15 * sin(uTime * 1.7
                                        + aPosition.x * 3.1
                                        + aPosition.z * 2.3);
        gl_PointSize = aSize * twinkle;
    }
    else
    {
        // Asteroids shrink with distance, so the far side of the belt does
        // not look chunkier than the near side. But we cap the size as well:
        // an asteroid is a speck of rock, and if it grows to five pixels
        // across it stops looking like a speck and starts looking like a
        // dirt spot on whatever planet happens to be behind it.
        float distance_to_camera = length(view_position.xyz);
        gl_PointSize = clamp(aSize * (26.0 / max(distance_to_camera, 1.0)),
                              1.0, 2.2);
    }

    // --- The brightness ---
    if (uIsStar == 1)
    {
        vBrightness = aBrightness;
        vLit = 1.0;
        return;
    }

    // Fade the rocks out as they get very close to the camera.
    //
    // A rock that was right in front of the lens would be a huge blur, so
    // drawing it as a dot would look wrong anyway. More practically, the
    // rocks nearest the camera are the ones that overlap the planets, and
    // fading them out keeps a planet's disc clean.
    float distance_to_camera = length(view_position.xyz);
    float near_fade = smoothstep(6.0, 16.0, distance_to_camera);

    // How lit is this asteroid?
    //
    // The sun is at the origin, so the direction from the sun to the
    // asteroid is just the normalised position.
    //
    // The direction from the asteroid to the camera is the view direction.
    // We have to be careful here: the view matrix turns world space into
    // camera space, and in camera space the camera looks down the -Z axis.
    // So we compare the sun direction AFTER putting it into camera space
    // too, otherwise we are comparing two directions that live in
    // different coordinate systems and the answer is meaningless.
    //
    // A dot has no real surface, so we cannot use a proper normal. But we
    // can still tell which side of the belt the sun is on: the rocks on the
    // near side catch more light than the ones on the far side.
    vec3 to_camera = normalize(-view_position.xyz);

    // The sun is at the world origin, so we transform the origin.
    vec3 sun_position_camera = (uView * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 direction_to_sun = normalize(sun_position_camera - view_position.xyz);

    // How much of the sun is in front of the rock, as seen from the camera.
    // 1.0 means the sun is directly behind the rock from our point of view,
    // so the rock is fully lit.
    float lit = 0.5 + 0.5 * dot(direction_to_sun, to_camera);

    // Squaring it makes the falloff towards the dark side steeper, which
    // gives the belt a rounded, three-dimensional look instead of a flat
    // disc.
    lit = lit * lit;

    vBrightness = aBrightness * near_fade;
    vLit = lit;
}
