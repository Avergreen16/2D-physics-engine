#version 460 core

struct char_d {
    vec4 color;
    bool bold;
};

layout(binding = 1) uniform char_data {
    char_d character_data[256];
};

layout(binding = 0) uniform sampler2D text_texture;

in vec2 tex_coord;

out vec4 frag_color;

void main() {
    char_d current_data = character_data[gl_PrimitiveID / 2];
    vec4 color;
    if(current_data.bold) {
        vec4 c = texelFetch(text_texture, ivec2(tex_coord), 0);
        vec4 c1 = texelFetch(text_texture, ivec2(tex_coord.x - 0.51, tex_coord.y), 0);
        if(c.a != 0.0) {
            color = c;
        } else if(c1.a != 0.0) {
            color = c1;
        } else {
            discard;
        }
    } else {
        color = texelFetch(text_texture, ivec2(tex_coord), 0);
        if(color.a == 0.0) discard;
    }
    
    frag_color = current_data.color * color;
}