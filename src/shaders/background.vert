#version 460 core

layout(location = 0) in vec2 position;

layout(location = 0) uniform mat4 view;
layout(location = 1) uniform mat4 proj;

layout(location = 0) out vec2 world_coords;

void main() {
    gl_Position = vec4(position, 0.5, 1.0);

    mat4 inv_view = inverse(view);
    mat4 inv_proj = inverse(proj);

    world_coords = (inv_view * inv_proj * vec4(position, 0.0, 1.0)).xy;
}