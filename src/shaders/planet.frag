#version 460 core

layout(binding = 0) uniform sampler2D planet_texture;

in vec2 frag_tex;
in float light;

layout(location = 0) out vec4 frag_color;

void main() {
    vec4 color = texture(planet_texture, frag_tex);
    frag_color = vec4(color.xyz * light, color.w);
}