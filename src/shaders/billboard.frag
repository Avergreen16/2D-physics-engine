#version 460 core

layout(binding = 0) uniform sampler2D tex;

in vec2 tex_pos;

out vec4 frag_color;

void main() {
    frag_color = texture(tex, tex_pos);
    if(frag_color.w == 0.0) discard;
}