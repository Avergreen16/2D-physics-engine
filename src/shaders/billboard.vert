#version 460 core

vec3 vertices[] = {
    vec3(0.5, -0.5, 0.0),
    vec3(-0.5, -0.5, 0.0),
    vec3(0.5, 0.5, 0.0),
    vec3(0.5, 0.5, 0.0),
    vec3(-0.5, -0.5, 0.0),
    vec3(-0.5, 0.5, 0.0),
};

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;
layout(location = 3) uniform vec2 size;
layout(location = 4) uniform vec4 select;

out vec2 tex_pos;

void main() {
    vec3 v = vertices[gl_VertexID];
    tex_pos = vec2((v.x + 0.5) * select.z + select.x, (v.y + 0.5) * select.w + select.y);

    gl_Position = view * model * vec4(v * vec3(size, 1.0), 1.0);

    
    gl_Position = gl_Position = proj * gl_Position;
} 