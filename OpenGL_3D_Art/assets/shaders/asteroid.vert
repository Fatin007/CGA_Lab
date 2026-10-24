#version 330 core

// asteroid.vert -- Vertex shader for the asteroid belt.
//
// The belt is a cloud of GL_POINTS, just like the stars, but the asteroids
// live in real world space between Mars and Jupiter. That means they use the
// full view matrix (including its translation), because they really are at
// a fixed place in the solar system.
//
// Two things make them read as a belt rather than as scattered dots:
//
//   * Each asteroid keeps its own distance from the sun and its own height
//     above the plane of the orbits, so the cloud is a flattened ring.
//
//   * The whole belt turns slowly around the sun. Real asteroids orbit the
//     sun, and the slow rotation makes it obvious that the ring is spinning
//     rather than being painted on the screen.
//
// The lighting is done here, in the vertex shader, because we know where
// the sun is: it is always at the origin. Each asteroid gets a "lit" value
// that says how much of the sun reaches it. That is what stops the belt
// looking like a flat sheet of grey.

layout(location = 0) in vec3 aPosition;   // where the asteroid is
layout(location = 1) in float aBrightness; // how bright this rock is
layout(location = 2) in float aSize;       // how big this dot should be

uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;

out float vBrightness;
out float vLit;

void main()
{
    // The belt turns around the sun. We rotate the asteroid's position about
    // the Y axis, which is the axis the planets orbit around.
    float angle = uTime * 0.02;
    float c = cos(angle);
    float s = sin(angle);

    vec3 spun = vec3(aPosition.x * c - aPosition.z * s,
                      aPosition.y,
                      aPosition.x * s + aPosition.z * c);

    gl_Position = uProjection * uView * vec4(spun, 1.0);

    // Where the asteroid is, in camera space.
    vec4 camera_position = uView * vec4(spun, 1.0);
    float distance_to_camera = length(camera_position.xyz);

    // The size of the dot.
    //
    // We scale it down with distance, so the far side of the belt does not
    // look chunkier than the near side. But we cap the size as well: an
    // asteroid is a speck of rock, and if it grows to five pixels across it
    // stops looking like a speck and starts looking like a dirt spot on
    // whatever planet happens to be behind it.
    gl_PointSize = clamp(aSize * (26.0 / max(distance_to_camera, 1.0)),
                          1.0, 2.2);

    // Fade the rocks out as they get very close to the camera.
    //
    // A rock that was right in front of the lens would be a huge blur, so
    // drawing it as a dot would look wrong anyway. More practically, the
    // rocks nearest the camera are the ones that overlap the planets, and
    // fading them out keeps a planet's disc clean.
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
    vec3 to_camera = normalize(-camera_position.xyz);

    // The sun is at the world origin, so we transform the origin.
    vec3 sun_position_camera = (uView * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 direction_to_sun = normalize(sun_position_camera - camera_position.xyz);

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
