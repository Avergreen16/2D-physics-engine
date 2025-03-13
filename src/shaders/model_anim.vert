#version 460 core

uint indices[6] = {
    0, 1, 0, 2, 0, 3
};

vec3 colors[6] = {
    vec3(1, 0, 0),
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, 1, 0),
    vec3(0, 0, 1),
    vec3(0, 0, 1)
};

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;

layout(location = 3) uniform vec3 positions[64];
layout(location = 67) uniform mat4 bone_matrices[32];

layout(location = 0) out vec3 color;

void main() {
    int i = gl_VertexID;

    int i2 = i % 6;
    int i3 = i / 6;

    color = colors[i2];

    vec3 v = positions[i3 * 4 + indices[i2]];

    vec4 vec = view * model * bone_matrices[i3] * vec4(v, 1.0);

    gl_Position = proj * vec;
}