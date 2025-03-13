#version 460 core

layout(binding = 1) uniform matrices {
    mat4 model[512];
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in uint mesh_id;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;

//layout(location = 5) uniform mat4 model[512];

layout(location = 0) out vec2 o_tex_coord;
layout(location = 1) out vec3 o_normal;
layout(location = 2) out vec3 o_pos;

void main() {
    o_tex_coord = tex_coord;

    vec4 pos = vec4(position, 1.0);

    mat4 model_v = model[mesh_id];

    mat3 m = mat3(model_v);

    m = mat3(normalize(m[0]), normalize(m[1]), normalize(m[2]));

    o_normal = m * normal;

    gl_Position = (view * model_v * pos);

    o_pos = gl_Position.xyz;

    gl_Position = proj * gl_Position;
}