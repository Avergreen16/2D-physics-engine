#version 460 core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec4 color;
layout(location = 3) in uint data;
layout(location = 4) in ivec4 range;

layout(location = 0) uniform mat3 view_mat;
layout(location = 1) uniform mat3 trans_mat;

layout(location = 0) out vec2 tex_coord;
layout(location = 1) out vec4 col;
layout(location = 2) flat out uint ddd;
layout(location = 3) flat out ivec4 range_;
layout(location = 4) out vec2 position;

void main() {
    tex_coord = tex;
    ddd = data;
    col = color;
    range_ = range;
    position = pos.xy;

    gl_Position = vec4((view_mat * trans_mat * vec3(pos.xy, 1.0)).xy, pos.z, 1.0);
}