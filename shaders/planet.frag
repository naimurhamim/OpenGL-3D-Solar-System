#version 330 core

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D texture1;
uniform vec3  lightPos;
uniform vec3  lightColor;
uniform bool  isSun;
uniform bool  isEarth;
uniform bool  isClouds;

void main()
{
    vec3 texColor = texture(texture1, TexCoord).rgb;

    if(isSun)
    {
        // Sun is always fully bright
        FragColor = vec4(texColor * 1.2, 1.0);
        return;
    }

    // Cloud layer — white with transparency based on texture brightness
    if(isClouds)
    {
        float brightness = (texColor.r + texColor.g + texColor.b) / 3.0;
        FragColor = vec4(1.0, 1.0, 1.0, brightness * 0.6);
        return;
    }

    // Ambient
    float ambientStrength = 0.25;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse — light direction from Sun
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = diff * lightColor;

    vec3 result = (ambient + diffuse) * texColor;

    // Earth atmosphere rim glow
    if(isEarth)
    {
        vec3  viewDir = normalize(-FragPos);
        float rim     = 1.0 - max(dot(viewDir, norm), 0.0);
        rim           = pow(rim, 3.0);
        result       += vec3(0.1, 0.3, 0.8) * rim * 0.6;
    }

    FragColor = vec4(result, 1.0);
}