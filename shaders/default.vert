#version 330 core

layout (location = 0) in vec3 pos;

uniform mat4 u_Proj;
uniform mat4 u_View;

uniform sampler2D u_Tex;
uniform vec2 u_TexRes;
uniform float u_MaxHeight;

out vec3 oNormal;
out vec3 oPos;

float getHeight(vec2 uv) {
    return texture(u_Tex, uv).r;
}

void main() {
    vec2 uv = pos.xz/u_TexRes + 0.5;
    gl_Position = u_Proj * u_View * vec4(pos, 1.0);

    vec2 texel = vec2(1.0/u_TexRes.x, 1.0/u_TexRes.y);
    uv = clamp(uv, texel, vec2(1.0) - texel);

    float L = getHeight(uv - vec2(texel.x, 0.0)) * u_MaxHeight;
    float R = getHeight(uv + vec2(texel.x, 0.0)) * u_MaxHeight;
    float D = getHeight(uv - vec2(0.0, texel.y)) * u_MaxHeight;
    float U = getHeight(uv + vec2(0.0, texel.y)) * u_MaxHeight;

    vec3 Tx = vec3(2.0, R - L, 0.0);
    vec3 Tz = vec3(0.0, U - D, 2.0);

    oNormal = normalize(cross(Tz, Tx));
    oPos = pos;
}
