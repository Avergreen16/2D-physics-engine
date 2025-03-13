#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in ivec4 bone_ids;
layout(location = 4) in vec4 bone_weights;

layout(location = 2) uniform mat4 model_0;
layout(location = 6) uniform mat4 bone_matrices[32];

layout(location = 0) out vec2 o_tex_coord;

void main() {
    o_tex_coord = tex_coord;

    vec4 pos = vec4(0.0, 0.0, 0.0, 0.0);

    for(int i = 0; i < 4; ++i) {
        if(bone_ids[i] != -1) {
            vec4 local_pos = bone_matrices[bone_ids[i]] * vec4(position, 1.0);
            pos += local_pos * bone_weights[i];
        }
    }

    if(pos.w == 0) pos = vec4(position, 1.0);
    else pos /= pos.w;

    gl_Position = model_0 * pos;
}