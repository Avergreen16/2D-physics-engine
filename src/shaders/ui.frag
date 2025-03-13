#version 460 core

layout(binding = 0) uniform sampler2D text_texture;

in vec2 tex_coord;

out vec4 frag_color;

void main() {
    vec4 c = texelFetch(text_texture, ivec2(tex_coord), 0);
    
    frag_color = c;

    //if(c.w == 0.0) discard;
}