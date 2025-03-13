#version 460 core

out vec4 frag_color;

//layout(binding = 1) uniform sampler2D depth_tex;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;

layout(location = 0) flat in mat4 inv_proj;
layout(location = 4) flat in mat4 inv_view;
layout(location = 8) flat in mat4 inv_model;
layout(location = 12) in vec3 normal;

vec3 sun_dir = vec3(0, 0, 1);

void main() {
    discard;
    float m = dot(normal, sun_dir);
    m = max(0.0, m);

    frag_color = vec4(0.0, 0.0, m, 1.0);
}