#version 460 core

const vec3 vertices[] = {
    vec3(-1, -1, -1),
    vec3(1, -1, -1),
    vec3(-1, 1, -1),
    vec3(1, 1, -1),
    vec3(-1, -1, 1),
    vec3(1, -1, 1),
    vec3(-1, 1, 1),
    vec3(1, 1, 1),
};

const int indices[] = {
    0, 2, 4,   4, 2, 6,
    3, 1, 7,   7, 1, 5,
    1, 0, 5,   5, 0, 4,
    2, 3, 6,   6, 3, 7,
    3, 2, 1,   1, 2, 0,
    5, 4, 7,   7, 4, 6
};

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;
layout(location = 3) uniform vec3 player_pos;
layout(location = 4) uniform vec3 axes;

float height = 2.0;

out vec3 frag_pos;
flat out mat4 inv_view;
flat out mat4 inv_model;

void main() {
    vec3 v = vertices[indices[gl_VertexID]] * (axes + height) * 1.5;

    frag_pos = v;

    inv_view = inverse(view);
    inv_model = inverse(model);
    gl_Position = proj * view * model * vec4(v, 1.0);
}