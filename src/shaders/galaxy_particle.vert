#version 460 core

struct Particle_data {
    vec3 position;
    vec4 color;
    vec2 size;
    vec2 tex_coord;
    vec2 tex_size;
    vec3 orient_center;
    uint settings;
};

layout(std140, binding = 0) buffer Particle_buffer {
    Particle_data particle_data[];
};

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

layout(location = 0) uniform mat4 view_mat;
layout(location = 1) uniform mat4 proj_mat;
layout(location = 2) uniform mat4 model_mat;
layout(location = 3) uniform vec3 particle_up;
layout(location = 4) uniform ivec2 screen_size;
layout(location = 5) uniform vec3 sun_dir;

out vec4 color;
flat out mat3 inv_view;
flat out mat4 inv_proj;
out vec3 pos;
out vec3 origin;
out vec2 size;

out vec3 r;
out vec3 up;
out float fade_factor;

out vec3 x_v;
out vec3 y_v;
out vec3 z_v;



float far_plane = pow(2, 37) * 1000000;

mat3 inv = inverse(mat3(view_mat));

mat3 get_orientation(vec3 u) {
    vec3 x = cross(vec3(0, 1, 0), u);
    x = normalize(x);

    vec3 y = normalize(cross(u, x));

    return mat3(x, y, u);
}

float ray_plane(vec3 ray_origin, vec3 ray_direction, vec3 plane_origin, vec3 plane_normal) {
    float denom = dot(plane_normal, ray_direction);
    return -dot(plane_origin - ray_origin, plane_normal) / denom;
}

void main() {
    Particle_data current_data = particle_data[gl_VertexID / 36];
    
    up = normalize(current_data.orient_center);
    mat3 orientation = get_orientation(up);


    vec3 position = orientation * (vertices[indices[gl_VertexID % 36]] * vec3(1.0, 1.0, 1.0));

    color = current_data.color;

    vec4 view_pos = view_mat * vec4(current_data.position, 1.0);

    size = current_data.size * 0.5;
    position *= current_data.size.x;
    
    vec3 dir = normalize(-current_data.position - position);

    y_v = inv * vec3(0, 1, 0);
    x_v = normalize(cross(y_v, dir));
    y_v = normalize(cross(dir, x_v));

    z_v = dir;

    dir = normalize(position + current_data.position);

    //loat rx = ray_plane(vec3(0), dir, current_data.position, z_v);
    //vec3 rv = dir * rx;
    /*vec3 rr = rv - current_data.position;
    float x = dot(rr, x_v);
    float y = dot(rr, y_v);
    vec3 v2 = x_v * x + y_v * y;*/

    vec4 v = view_pos + view_mat * vec4(position, 0.0);
    
    gl_Position = proj_mat * v;

    origin = view_pos.xyz;

    inv_view = mat3(transpose(view_mat));
    inv_proj = inverse(proj_mat);

    r = vec3(current_data.tex_coord, current_data.tex_size.x);
}