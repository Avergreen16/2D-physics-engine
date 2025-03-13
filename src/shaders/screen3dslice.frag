#version 460 core

layout(binding = 0) uniform sampler3D tex;

in vec2 tex_coord;

out vec4 frag_color;

void main() {
    frag_color = vec4(texture(tex, vec3(tex_coord, 0.5)).rgb, 1.0);
}