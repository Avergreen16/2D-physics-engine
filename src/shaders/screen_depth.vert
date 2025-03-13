#version 460 core

vec3 grid_plane[6] = vec3[](
    vec3(1, 1, 0), vec3(-1, 1, 0), vec3(-1, -1, 0),
    vec3(-1, -1, 0), vec3(1, -1, 0), vec3(1, 1, 0)
);

layout(location = 3) uniform mat4 sun_view;

out vec2 tex_coord;
out vec3 light_dir;

void main() {
    tex_coord = (grid_plane[gl_VertexID].xy + 1.0) * 0.5;
    gl_Position = vec4(grid_plane[gl_VertexID].xyz, 1.0);

    mat3 inv_view = mat3(inverse(sun_view));

    light_dir = normalize(inv_view * vec3(0, 0, 1));
}