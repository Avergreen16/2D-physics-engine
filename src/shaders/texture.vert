#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coords;

layout(location = 0) uniform mat4 view;
layout(location = 1) uniform mat4 proj;
layout(location = 2) uniform mat4 model;

layout(location = 0) out vec2 t;

void main() {
    gl_Position = view * model * vec4(position, 1.0);

    gl_Position = proj * gl_Position;
    t = tex_coords;
}