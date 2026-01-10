#version 330 core

out vec4 FragColor;
in vec2 oUV;

uniform sampler2D u_Tex;

void main() {
    FragColor = texture(u_Tex, oUV);
}
