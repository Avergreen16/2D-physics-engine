#version 460 core

vec2 vertices[4] = {
    vec2(-1, -1),
    vec2(1, -1),
    vec2(-1, 1),
    vec2(1, 1)
};

int indices[12] = {
    0, 1, 3,
    0, 3, 2,
    0, 3, 1,
    0, 2, 3
};

layout(location = 0) uniform mat4 proj_mat;
layout(location = 1) uniform mat4 view_mat;
layout(location = 2) uniform mat3 model_mat;
layout(location = 3) uniform vec3 pos;
layout(location = 4) uniform float radius;

out vec2 p;
out float r;

void main() {
    vec2 v = vertices[indices[gl_VertexID]] * radius;

    p = v;
    r = radius;

    gl_Position = view_mat * vec4(pos + (model_mat * vec3(v, 0.0)), 1.0);
    
    gl_Position = proj_mat * gl_Position;
}