#version 330 core

// basic.vert -- Vertex shader for all geometry in the chess project.
//
// The pipeline for every object is the standard one:
//   uModel  (object -> world)  ->  uView (world -> camera)  ->  uProj (camera -> clip)
//
// We send the world-space position to the fragment shader because the lighting
// is computed in world space.

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

// A colour multiplier applied to everything we draw.
//
// The mesh data holds each object's own colour. The tint lets us draw the
// SAME mesh in a different colour without making a second copy of it -- for
// example the highlight square, which we draw in green for the selected
// square, yellow for the last move and red for a king in check.
uniform vec3 uTint;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec3 vColor;

void main()
{
    // Transform the vertex from object space into world space.
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;

    // Normals are transformed with the upper-left 3x3 part of the model matrix.
    // This is correct as long as we only use uniform scaling, which we do.
    vNormal = mat3(uModel) * aNormal;

    // Colours are stored per-vertex, which lets us give each piece and each
    // board square its own colour without any extra textures. The tint is
    // then applied on top.
    vColor = aColor * uTint;

    // Finally project into clip space.
    gl_Position = uProjection * uView * worldPosition;
}
