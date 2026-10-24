#version 330 core

// line.frag -- Fragment shader for the orbit lines.
//
// The orbits are drawn as faint lines so we can see the path each planet
// takes. They are deliberately dim: they are a guide, not the subject.

uniform vec3 uLineColour;

out vec4 FragColour;

void main()
{
    FragColour = vec4(uLineColour, 1.0);
}
