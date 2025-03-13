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
layout(location = 5) uniform float height;

out vec3 frag_pos;
out vec4 view_pos;
out vec2 screen_pos;
flat out mat4 inv_proj;
flat out mat4 inv_view;
flat out mat4 inv_model;

void main() {
    vec4 v = model * vec4(vertices[indices[gl_VertexID]] * (axes + height) * 3, 1.0);

    frag_pos = v.xyz;

    inv_proj = inverse(proj);
    inv_view = inverse(view);
    inv_model = inverse(model);

    gl_Position = view * v;

    view_pos = gl_Position;

    gl_Position = proj * gl_Position;

    screen_pos = (gl_Position / gl_Position.w).xy;
}