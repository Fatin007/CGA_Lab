#version 330 core

// line.vert -- Vertex shader for the orbit lines.
//
// The orbits are simple GL_LINE_STRIP chains, so all we need is a position
// and the two matrices.

layout(location = 0) in vec3 aPosition;

uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    gl_Position = uProjection * uView * vec4(aPosition, 1.0);
}
