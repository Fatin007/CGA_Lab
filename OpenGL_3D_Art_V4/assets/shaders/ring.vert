#version 330 core

// ring.vert -- Vertex shader for the planetary rings.
//
// A ring is a flat disc with a hole in the middle. We build it from a strip
// of quads, and the UV coordinate tells the fragment shader where on the
// ring we are, so it can draw the gaps between the bands.
//
// We also transform the ring's normal into world space. The ring leans over
// with the planet's axial tilt, and the fragment shader needs to know which
// way the ring is facing so it can work out how much sunlight it catches.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec2 vUV;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);

    vWorldPosition = worldPosition.xyz;

    // The ring is only ever scaled uniformly and rotated, so we can use
    // mat3(uModel) to turn the normal into world space.
    vWorldNormal = mat3(uModel) * aNormal;

    vUV = aUV;

    gl_Position = uProjection * uView * worldPosition;
}
