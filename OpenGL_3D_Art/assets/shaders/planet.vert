#version 330 core

// planet.vert -- Vertex shader for the planets.
//
// A planet is a sphere that is:
//   1. moved to its place on the orbit      (uModel)
//   2. leaned over by its axial tilt        (uAxialTilt)
//   3. spun around its own axis             (uRotation)
//
// We combine all three into one model matrix on the CPU, so this shader
// only has to do the standard job of transforming a vertex into clip space.

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
    // Move the vertex from its local space into the world.
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);

    vWorldPosition = worldPosition.xyz;

    // The normal is rotated by the same model matrix.
    //
    // We use mat3(uModel) rather than the full 4x4 because a normal is a
    // direction, not a position -- it has no "from here to there" part, so
    // the translation must not be applied to it.
    //
    // This is only correct when the model matrix has no non-uniform scale,
    // which is true here: we only ever scale planets uniformly.
    vNormal = mat3(uModel) * aNormal;

    vUV = aUV;

    gl_Position = uProjection * uView * worldPosition;
}
