#version 460 core

layout(binding = 1) uniform sampler2D tex;

out vec4 frag_color;

in vec4 color;
in vec2 tex_coord;

void main() {
    vec4 tex_color = texture(tex, tex_coord / textureSize(tex, 0));
    vec4 col = tex_color * color;
    frag_color = col;

    if(col.w == 0.0) discard;
}