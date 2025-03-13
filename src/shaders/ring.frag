#version 460 core

struct Ring_data {
    vec4 color;
    float width;
};

layout(std140, binding = 0) buffer ring_buffer {
    Ring_data ring_data[];
};

layout(location = 5) uniform int ring_buffer_length;

out vec4 frag_color;

in vec2 p;
in float r;

void main() {
    float dist = r - length(p);

    if(dist > 0) {
        float collector = 0;
        vec4 color = vec4(0, 0, 0, 0);
        for(int i = 0; i < ring_buffer_length; ++i) {
            Ring_data data = ring_data[i];
            collector += data.width;
            if(collector >= dist) {
                color = data.color;
                break;
            } else if(data.width == 0) {
                discard;
            }
        }
        
        if(color.w == 0.0) discard;
        frag_color = color;
    } else discard;
}