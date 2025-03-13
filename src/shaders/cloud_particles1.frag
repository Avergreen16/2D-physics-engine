#version 460 core

layout(binding = 1) uniform sampler2D tex;
layout(binding = 2) uniform sampler3D density_tex;

out vec4 frag_color;

layout(location = 0) uniform mat4 view_mat;

in vec4 color;
in vec2 tex_coord;
in mat3 inv_view;
in vec3 pos;
in vec3 rel_pos;
in vec3 origin;
in vec2 size;
in vec3 r;
in vec3 up;

vec3 sun_dir = normalize(vec3(0.2, 1.0, 0.5));

int num_pixels = 64;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint hash(uvec2 v) { 
    return hash(v.x ^ hash(v.y)); 
}

uint hash(uvec3 v) { 
    return hash(v.x ^ hash(v.y) ^ hash(v.z)); 
}

uint hash(uvec4 v) {
    return hash(v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w)); 
}

float to_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat(m);       // Range [1:2]
    return f - 1.0;                // Range [0:1]
}


mat3 get_orientation(vec3 u) {
    vec3 x = cross(vec3(0, 1, 0), u);
    if(length(x) < 0.01) x = cross(vec3(0, 0, 1), u);
    x = -normalize(x);

    vec3 y = -normalize(cross(x, u));

    return mat3(x, y, u);
}

mat3 up_matrix = get_orientation(up);

float sample_density(vec3 pos, mat3 up_matrix) {
    vec3 pn = pos / size.x;
    pn = transpose(up_matrix) * pn;

    uint h1 = hash(uint(r.y * 1001));
    uint h2 = hash(uint(r.z * 90283));
    uint h3 = hash(h1);
    vec3 offset = vec3(to_float(h1), to_float(h2), to_float(h3));

    float d = max(0.0, texture(density_tex, fract((pn * 0.5 + 0.5) * 0.4 + offset * vec3(-1.6, 0.5, -5.7))).g * 2.0 - 1.0);
    float d2 = max(0.0, texture(density_tex, fract((pn * 0.5 + 0.5) * 1.5 + offset * vec3(8.0, 4.6, -1.9))).g * 2.0 - 1.0);
    d *= r.x * 5;
    d += d2;

    vec3 v = vec3(1, 1, 4);
    if(h2 % 4 == 0) v = vec3(1, 1, 2);
    else if(h2 % 4 == 1 && size.x < 3500) v = vec3(2, 2, 1);

    d *= 1.0 - length(pn * v);
    //d += (1.0 - length(pn * vec3(1, 1, 2))) * r.y;
    d = max(0, d);

    return d;
}

vec3 sphere_raycast(vec3 center, float radius, vec3 origin, vec3 dir) {
    vec3 rel_center = origin - center;

    float closest_pos = dot(-rel_center, dir);

    vec3 c = rel_center + dir * closest_pos;

    float dist = length(c);

    if(dist > radius) return vec3(0, closest_pos, 0);

    float t = sqrt(radius * radius - dist * dist);

    float t0 = closest_pos - t;
    float t1 = closest_pos + t;
    //t0 = max(t0, 0);

    return vec3(t0, closest_pos, t1);
}

vec3 light_color = vec3(1.0, 0.8, 0.8);
vec3 dark_color = vec3(0.85, 0.7, 0.85) * 0.5;
int num_steps = 5;

float const_size = 2000;

void main() {
    /*vec4 tex_color = texture(tex, tex_coord / textureSize(tex, 0));
    vec4 col = tex_color * color;
    frag_color = col;*/
    vec3 pos_p = inv_view * rel_pos.xyz;
    vec3 pos_inv = inv_view * pos;

    vec3 dir_2 = inv_view * vec3(0, 0, -1);//normalize(rel_pos.xyz);
    vec3 x_2 = inv_view * vec3(1, 0, 0);
    vec3 y_2 = inv_view * vec3(0, 1, 0);

    vec3 ray_origin = pos_p;
    vec3 origin2 = inv_view * origin;

    vec3 dir = normalize(origin2);
    vec3 dir_x = -normalize(cross(y_2, dir));
    vec3 dir_y = -cross(dir, dir_x);


    vec3 rel_ro = ray_origin - origin2;
    rel_ro = mat3(view_mat) * rel_ro;

    rel_ro.xy = round(rel_ro.xy / size * num_pixels) * size / num_pixels;
    rel_ro = dir_x * rel_ro.x + dir_y * rel_ro.y;
    //rel_ro = inv_view * rel_ro;

    vec3 t = sphere_raycast(vec3(0.0), size.x, rel_ro, dir);

    if(length(rel_ro + dir * t.y) >= size.x) discard;

    float t1 = t.z;
    float t0 = t.x;

    float len = t1 - t0;

    float density = 0.0;
    float incoming_light = 0.0;

    int g = 10;
    int h = 5;
    for(int i = 0; i < g; ++i) {
        float ii = (float(i + 0.5)) / g;
        float t2 = t0 + len * ii;

        vec3 pos = rel_ro + dir * t2;

        float d = sample_density(pos, up_matrix);

        
        vec3 u = sphere_raycast(vec3(0.0), size.x, pos, sun_dir);

        float density2 = 0;

        if(d > 0.0) {
            float dd = 10.0;

            float dx = sample_density(pos + vec3(dd, 0, 0), up_matrix);
            float dy = sample_density(pos + vec3(0, dd, 0), up_matrix);
            float dz = sample_density(pos + vec3(0, 0, dd), up_matrix);

            vec3 dv = vec3(dx - d, dy - d, dz - d);
            dv = normalize(-dv);

            float d_dot = dot(sun_dir, dv);

            incoming_light = d_dot;

            density = 1.0;

            break;
        }
    }

    if(density > 0.0) {
        float interpolation = clamp(incoming_light, 0.0, 1.0);

        interpolation = floor(interpolation * (num_steps + 1)) / num_steps;
        interpolation = clamp(interpolation, 0.0, 1.0);

        vec3 color = dark_color + (light_color - dark_color) * interpolation;

        frag_color = vec4(color, 1.0);
    } else discard;

    
    // todo: make sampling direction parallel to particle face
    //if(col.w == 0.0) discard;
}