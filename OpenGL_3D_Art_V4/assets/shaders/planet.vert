#version 330 core

// planet.vert -- Vertex shader for the planets and the sun.
//
// A planet is a sphere that is:
//   1. moved to its place on the orbit   (uModel)
//   2. leaned over by its axial tilt     (uModel again)
//   3. spun around its own axis          (uModel again)
//
// All three are combined into one model matrix on the CPU, so all this
// shader has to do is the standard job of moving a vertex into clip space.

layout(location = 0) in vec3 aPosition;   // where the vertex is on the sphere
layout(location = 1) in vec3 aNormal;     // which way the surface faces
layout(location = 2) in vec2 aUV;         // where it is on the sphere's "map"

uniform mat4 uModel;         // the planet's place, tilt and spin
uniform mat4 uView;          // world -> camera
uniform mat4 uProjection;    // camera -> clip space

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUV;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);

    vWorldPosition = worldPosition.xyz;

    // The normal is a direction, not a position, so we use mat3(uModel) and
    // not the full 4x4 -- that way the translation is left out of it.
    // This is only correct when the scale is uniform, and planets are only
    // ever scaled uniformly.
    vNormal = mat3(uModel) * aNormal;

    vUV = aUV;

    gl_Position = uProjection * uView * worldPosition;
}
