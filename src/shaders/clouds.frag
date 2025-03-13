#version 460 core

layout(binding = 1) uniform sampler2D particle_tex;

in vec2 uv;
in vec4 color;
flat in uint variation;


vec2 sprite_size = vec2(16, 16);
vec2 origins[] = {
    vec2(16, 0),
    vec2(32, 0),
    vec2(48, 0),
    vec2(64, 0),
    vec2(80, 0),
};

layout(location = 0) out vec4 frag_color;

void main() {
    vec2 tex_coord = origins[variation] + sprite_size * uv;
    vec4 tex = texture(particle_tex, tex_coord / textureSize(particle_tex, 0));

    frag_color = color * tex;
}