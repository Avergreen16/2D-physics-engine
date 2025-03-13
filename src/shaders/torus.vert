#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in ivec4 bone_ids;
layout(location = 4) in vec4 bone_weights;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;

layout(location = 0) out vec3 o_planet_pos;
layout(location = 1) out vec3 o_normal;
layout(location = 2) out vec3 o_pos;

void main() {
    vec4 pos = vec4(position, 1.0);
    o_planet_pos = pos.xyz;

    mat3 m = mat3(model);

    m = mat3(normalize(m[0]), normalize(m[1]), normalize(m[2]));

    o_normal = m * normal;

    gl_Position = (view * model * pos);

    o_pos = gl_Position.xyz;

    gl_Position = proj * gl_Position;
}