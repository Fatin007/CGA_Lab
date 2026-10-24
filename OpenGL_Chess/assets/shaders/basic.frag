#version 330 core

// basic.frag -- Fragment shader implementing simple Phong lighting.
//
// Lighting model:
//   final = ambient * colour
//         + diffuse  * lightColour * colour
//         + specular * lightColour
//         + rim      * lightColour
//
// This is a deliberately simple but complete lighting model: one directional
// light plus ambient light, a Blinn-Phong specular highlight, and a subtle
// rim term that makes the pieces stand out against the board.

in vec3 vWorldPosition;
in vec3 vNormal;
in vec3 vColor;

uniform vec3  uLightDirection;   // direction the light travels (pointing down-ish)
uniform vec3  uLightColour;
uniform vec3  uAmbientColour;
uniform vec3  uEyePosition;      // camera position, used for specular + rim
uniform float uShininess;        // higher = smaller, sharper highlight

out vec4 FragColour;

void main()
{
    vec3 normal   = normalize(vNormal);
    vec3 lightDir = normalize(uLightDirection);
    vec3 viewDir  = normalize(uEyePosition - vWorldPosition);

    // halfway vector between the light and the view, used by Blinn-Phong.
    vec3 halfVector = normalize(lightDir + viewDir);

    // Diffuse: how much the surface faces the light.
    float diffuse = max(dot(normal, lightDir), 0.0);

    // Specular: a small bright highlight on shiny surfaces.
    float specular = pow(max(dot(normal, halfVector), 0.0), uShininess);

    // Rim: brightens the silhouette edges. Cheap, and it makes the pieces pop.
    float rim = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);

    vec3 result = uAmbientColour * vColor
                + uLightColour * diffuse * vColor
                + uLightColour * specular * 0.35
                + uLightColour * rim * 0.08;

    FragColour = vec4(result, 1.0);
}
