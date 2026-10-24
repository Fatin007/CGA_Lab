#version 330 core

// sun.vert -- Vertex shader for the sun and its corona.
//
// The sun itself is an ordinary sphere. The corona (the glow around it) is a
// flat quad that we always turn to face the camera, so it looks like a
// disc of light from every angle.

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vUV;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);

    vWorldPosition = worldPosition.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;

    gl_Position = uProjection * uView * worldPosition;
}
