#version 460 core

layout(binding = 0) uniform sampler2D ttexture;

layout(location = 0) in vec2 tex_coords;
layout(location = 1) in vec3 color;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_normal;

void main() {
    frag_color = texture(ttexture, tex_coords);
    if(frag_color.w == 0) discard;
    frag_normal = vec4(0.0, 0.0, 0.0, 1.0);
    frag_color.xyz *= color;
}