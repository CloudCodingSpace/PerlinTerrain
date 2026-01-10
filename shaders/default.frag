#version 330 core

out vec4 FragColor;

uniform vec3 u_LightPos;

in vec3 oNormal;
in vec3 oPos;

void main() {
    vec4 color = vec4(0.0, 1.0, 0.0, 1.0);
    vec3 lightDir = normalize(u_LightPos - oPos);
    float f = max(dot(oNormal, lightDir), 0.0);
    FragColor = color * f;
}
