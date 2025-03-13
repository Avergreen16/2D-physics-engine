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
    vec3(-1, -1, 0),
    vec3(1, -1, 0),
    vec3(1, 1, 0),
    vec3(-1, -1, 0),
    vec3(1, 1, 0),
    vec3(-1, 1, 0)
};

layout(location = 0) uniform mat4 view_mat;
layout(location = 1) uniform mat4 proj_mat;
layout(location = 2) uniform mat4 model_mat;
layout(location = 3) uniform vec3 particle_up;
layout(location = 4) uniform ivec2 screen_size;

out vec4 color;
flat out mat3 inv_view;
flat out mat4 inv_proj;
out vec3 pos;
out vec3 origin;
out vec2 size;
out vec3 light_dir;
out vec3 light_color;

out vec3 r;
out vec3 up;
out float fade_factor;

out vec3 x_v;
out vec3 y_v;
out vec3 z_v;

out vec3 v;
out vec3 w;

flat out uint num;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}


float to_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float f = uintBitsToFloat(m);       // Range [1:2]
    return f - 1.0;                // Range [0:1]
}

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
    Particle_data current_data = particle_data[gl_VertexID / 6];

    light_color = current_data.color.xyz;

    uint h = hash(gl_VertexID / 6);
    
    num = gl_VertexID / 6;

    up = vec3(0, 0, 1);
    bool cont = true;
    for(int i = 0; i < 10; ++i) {
        if(cont) {
            uint h = hash(num);

            uint h1 = hash(i ^ h);
            uint h2 = hash(h ^ h1);
            uint h3 = hash(h2 ^ h);

            vec3 v = vec3(to_float(h1), to_float(h2), to_float(h3));

            float len_v = length(v);

            if(len_v <= 1 && len_v != 0) {
                cont = false;
                up = normalize(v);
            }
        }
    }
    
    mat3 orientation = get_orientation(up);


    vec3 position = vertices[gl_VertexID % 6];

    color = current_data.color;

    vec4 view_pos = view_mat * vec4(current_data.position, 1.0);

    size = current_data.size;
    position *= current_data.size.x;
    
    vec3 dir = normalize(-current_data.position);

    y_v = inv * vec3(0, 1, 0);
    x_v = normalize(cross(y_v, dir));
    y_v = normalize(cross(dir, x_v));

    z_v = dir;

    vec3 v2 = x_v * position.x + y_v * position.y;

   //v2 *= current_data.size.x;

    w = v2;

    pos = mat3(view_mat) * v2;

    vec4 v1 = view_pos + vec4(pos, 0.0);
    
    gl_Position = proj_mat * v1;

    origin = view_pos.xyz;

    inv_view = mat3(transpose(view_mat));
    inv_proj = inverse(proj_mat);

    r = vec3(current_data.tex_coord, current_data.tex_size.x);
    v = position;

    light_dir = -current_data.orient_center;
}