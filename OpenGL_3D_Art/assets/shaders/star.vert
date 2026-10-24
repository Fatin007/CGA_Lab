#version 330 core

// star.vert -- Vertex shader for the background stars.
//
// The stars are a cloud of GL_POINTS. Each one is a single vertex, and the
// size of the dot is worked out here.
//
// Two things matter:
//
//   * The stars must be VERY far away, so they never appear to move as the
//     camera moves. We put them on a huge sphere and draw them with the
//     view matrix but no translation.
//   * Each star should be a slightly different brightness, so the sky
//     looks like a real sky instead of a sheet of white dots.

layout(location = 0) in vec3 aPosition;   // where the star is
layout(location = 1) in float aBrightness; // how bright this star is
layout(location = 2) in float aSize;       // how big this dot should be

uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;

out float vBrightness;

void main()
{
    // The stars are placed on a sphere of a fixed huge radius. The camera
    // sits in the middle of it, so we take the view matrix but throw away
    // its translation -- that keeps the sky infinitely far away.
    //
    // We do the same thing for the rotation: the sky should not spin round
    // with the camera. We build the view matrix from its rotation part only.
    mat4 rotation_only = uView;
    rotation_only[3].xyz = vec3(0.0, 0.0, 0.0);

    vec4 view_position = rotation_only * vec4(aPosition, 1.0);
    gl_Position = uProjection * view_position;

    // The size of the dot. A little twinkle makes it feel alive: each
    // star breathes at its own rate.
    float twinkle = 0.85 + 0.15 * sin(uTime * 1.7 + aPosition.x * 3.1 + aPosition.z * 2.3);
    gl_PointSize = aSize * twinkle;

    vBrightness = aBrightness;
}
