#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 uv;

out vec2 oUV;

uniform mat4 u_Proj;
uniform mat4 u_View;

void main() {
    gl_Position = u_Proj * u_View * vec4(pos, 1.0);
    oUV = uv;
}
