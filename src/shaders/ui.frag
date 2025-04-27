#version 460 core

layout(binding = 0) uniform sampler2D gui_texture;
layout(binding = 1) uniform sampler2D text_texture;

layout(location = 0) in vec2 tex_coord;
layout(location = 1) in vec4 color;
layout(location = 2) flat in uint data;
layout(location = 3) flat in ivec4 range;
layout(location = 4) in vec2 position;

out vec4 frag_color;

void main() {
    if(position.x < range.x || position.x > range.z || position.y < range.y || position.y > range.w) discard;
    else {
        vec4 c;
        if((data & 0x2) == 0x2) {
            c = vec4(1.0);
        } else if((data & 0x1) != 0x0) {
            c = texelFetch(text_texture, ivec2(tex_coord), 0);
        } else {
            c = texelFetch(gui_texture, ivec2(tex_coord), 0);
        }
    
        frag_color = c * color;
    }
}