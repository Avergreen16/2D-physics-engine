#version 460 core

layout(binding = 0) uniform sampler2D tex;

in vec2 tex_coord;

out vec4 frag_color;

ivec2 n[4] = {
    ivec2(1, 0),
    ivec2(-1, 0),
    ivec2(0, 1),
    ivec2(0, -1)
};

void main() {
    ivec2 texture_size = textureSize(tex, 0);
    ivec2 tex_pos = ivec2(tex_coord * texture_size);
    vec4 color = texelFetch(tex, tex_pos, 0);
    vec4 neighbors[4];

    for(int i = 0; i < 4; ++i) {
        ivec2 offset = n[i];

        ivec2 tex_coords = tex_pos + offset;

        if(tex_coords.x >= texture_size.x || tex_coords.x < 0 || tex_coords.y >= texture_size.y || tex_coords.y < 0) {
            neighbors[i] = color;
        } else {
            neighbors[i] = texelFetch(tex, tex_coords, 0);
        }
    }

    vec4 average = (neighbors[0] + neighbors[1] + neighbors[2] + neighbors[3]) / 4.0;

    vec4 difference = abs(color - average);

    float diff_sum = max(max(difference.x, difference.y), difference.z);

    color = average * diff_sum + color * (1 - diff_sum);

    frag_color = color;

    if(frag_color.w == 0.0) discard;
}