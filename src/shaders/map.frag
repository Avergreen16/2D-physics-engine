#version 460 core

vec4 color = vec4(1.0, 0.0, 0.0, 1.0);

layout(binding = 0) uniform sampler2D noise_tex;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_normal;

layout(location = 0) in vec2 coords;

void main() {
    vec4 c = texture(noise_tex, coords);

    frag_color = c;
    frag_normal = vec4(0.0, 0.0, 0.0, 1.0);
}