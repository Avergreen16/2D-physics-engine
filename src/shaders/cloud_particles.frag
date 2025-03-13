#version 460 core

layout(binding = 1) uniform sampler2D tex;
layout(binding = 2) uniform sampler3D density_tex;

out vec4 frag_color;

layout(location = 0) uniform mat4 view_mat;
layout(location = 1) uniform mat4 proj_mat;
layout(location = 2) uniform mat4 model_mat;
layout(location = 5) uniform vec3 sun_dir;

in vec4 color;
flat in mat3 inv_view;
flat in mat4 inv_proj;
in vec3 pos;
in vec3 origin;
in vec2 size;
in vec3 r;
in vec3 up;
in float fade_factor;

in vec3 x_v;
in vec3 y_v;
in vec3 z_v;

in vec3 v;
in vec3 w;

flat in uint num;

mat3 o = {x_v, y_v, z_v};

int num_pixels = int(64 * size.x / 2000);

float size_s = size.x;


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

    float f = uintBitsToFloat(m);       // Range [1:2]
    return f - 1.0;                // Range [0:1]
}


mat3 get_orientation(vec3 u) {
    vec3 x = cross(vec3(0, 1, 0), u);
    x = normalize(x);

    vec3 y = normalize(cross(u, x));

    return mat3(x, y, u);
}

float sample_density(vec3 pos, mat3 up_matrix, vec3 ratio) {
    vec3 pn = pos / size.x;
    pn = transpose(up_matrix) * pn;

    uint h1 = hash(uint(r.y * 1001));
    uint h2 = hash(uint(r.z * 90283));
    uint h3 = hash(h1);
    vec3 offset = vec3(to_float(h1), to_float(h2), to_float(h3));

    float a = texture(density_tex, fract((pn * 0.5 + 0.5) * 1.0 + offset * vec3(-1.6, 0.5, -5.7))).g * 2.0 - 1.0;
    float b = max(0.0, texture(density_tex, fract((pn * 0.5 + 0.5) * 0.3 + offset * vec3(8.0, 4.6, -1.9))).g * 2.0 - 1.0);

    float extinction = 1.0 - pow(length(pn / ratio), 0.4);

    float v = max(0.0, b + a * 0.5);
    v *= extinction;
    v *= 3;

    return v;
}

vec3 sphere_raycast(vec3 center, float radius, vec3 origin, vec3 dir) {
    vec3 rel_center = center - origin;

    float closest_pos = dot(rel_center, dir);

    vec3 c = -rel_center + dir * closest_pos;

    float dist = length(c);

    if(dist > radius) return vec3(0, closest_pos, 0);

    float t = sqrt(radius * radius - dist * dist);

    float t0 = closest_pos - t;
    float t1 = closest_pos + t;
    //t0 = max(t0, 0);

    return vec3(t0, closest_pos, t1);
}

vec3 ellipsoid_raycast(vec3 center, vec3 radius, vec3 origin, vec3 dir, mat3 orientation) {
    mat3 inv_ori = transpose(orientation);
    vec3 new_origin = origin - center;

    vec3 new_dir = inv_ori * dir;
    new_origin = inv_ori * new_origin;

    vec3 rotated_dir = new_dir;

    new_dir = normalize(new_dir / radius);
    new_origin = new_origin / radius;

    vec3 r = sphere_raycast(vec3(0.0), 1, new_origin, new_dir);

    vec3 t0 = new_dir * r.x;
    vec3 tc = new_dir * r.y;
    vec3 t1 = new_dir * r.z;

    //vec3 new_o = new_origin * radius;

    vec3 t;
    t.x = dot(t0 * radius, rotated_dir);
    t.y = dot(tc * radius, rotated_dir);
    t.z = dot(t1 * radius, rotated_dir);

    return t;
}

vec3 light_color_a = vec3(1.0, 1.0, 1.0);
vec3 dark_color_a = vec3(0.85, 0.7, 1.0) * 0.5;

vec3 light_color_b = vec3(1.0, 0.5, 0.3);
vec3 dark_color_b = vec3(0.6, 0.3, 0.4) * 0.5;

vec3 color_c = vec3(0.0, 0.0, 0.02);

int num_steps = 4;

float const_size = 1500;

float ray_plane(vec3 ray_origin, vec3 ray_direction, vec3 plane_origin, vec3 plane_normal) {
    float denom = dot(plane_normal, ray_direction);
    return -dot(plane_origin - ray_origin, plane_normal) / denom;
}

int pixel_size = int(float(size_s) / 48);

void main() {
    mat3 inv_model = inverse(mat3(model_mat));

    vec3 pos_inv = inv_view * pos;

    vec3 origin2 = inv_view * origin;

    vec3 rel_ro = origin2 + pos_inv;

    vec3 dir = z_v;

    float x = dot(w, x_v);
    float y = dot(w, y_v);

    x = (floor(x / pixel_size) + 0.5) * pixel_size;
    y = (floor(y / pixel_size) + 0.5) * pixel_size;

    vec3 w2 = x_v * x + y_v * y;

    /*float r = ray_plane(vec3(0), dir, origin2, z_v);108.176.87.243108.176.87.243
    rel_ro = dir * r;
    vec3 rr = rel_ro - origin2;
    float x = dot(rr, x_v);
    float y = dot(rr, y_v);
    float z = dot(rr, z_v);
    x = floor(x / 100) * 100;
    y = floor(y / 100) * 100;
    rel_ro = origin2 - x_v * x - y_v * y;
    dir = normalize(rel_ro);*/


    //vec3 view_dir = inv_view * vec3(0, 0, -1);

    //if(dot(dir, view_dir) < 0.0) dir *= -1;

    vec3 ratio = vec3(1.0, 1.0, 0.1);

    if(size.x < 3000 && (hash(num) % 2 == 1)) ratio = vec3(1.0, 1.0, 0.2);

    vec3 size_v = ratio * size.x;
    size_s = max(size_v.x, max(size_v.z, size_v.y));


    
    mat3 up_matrix = mat3(model_mat) * get_orientation(up);

    vec3 t = ellipsoid_raycast(vec3(0), size.x * ratio, w2, -dir, up_matrix);
    //t.x = max(t.x, 0.0);


    float t1 = t.z;
    float t0 = t.x;

    float len = t1 - t0;
    
    if(len <= 0.0) discard;

    //frag_color = vec4(len, len, len, 1.0);

    float density = 0.0;
    float incoming_light = 0.0;

    int g = 5;
    int h = 2;
    for(int i = 0; i < g; ++i) {
        float ii = (float(i) + 0.5) / g;
        float t2 = t0 + len * ii;

        vec3 pos = w2 - dir * t2;

        float d = sample_density(pos, up_matrix, ratio);

        d = d * (len / const_size) / g * 50;
        
        vec3 u = ellipsoid_raycast(vec3(0), size.x * ratio, pos, sun_dir, up_matrix);

        float density2 = 0;

        h = int(u.z / const_size * 5);

        for(int j = 1; j < h + 1; ++j) {
            float jj = (float(j) + 0.5) / (h + 1);
            float u2 = u.z * jj;

            vec3 pos2 = pos + sun_dir * u2;

            float d2 = sample_density(pos2, up_matrix, ratio);

            density2 += d2 * ((u.z) / const_size) / (h + 1);
        }

        float transmittance2 = exp(-density2 * 100);
        float transmittance = exp(-density);

        incoming_light += transmittance * transmittance2 * d;
        
        density += d;
    }

    float transmittance = exp(-density);

    float ndotl = dot(mat3(model_mat) * normalize(up), sun_dir);
    float wrap = 0.35;
    float ndotl_wrap = clamp((ndotl + wrap) / (1.0 + wrap), 0.0, 1.0);
    
    float scatter_width = 0.38;
    float scatter = smoothstep(0.0, scatter_width, ndotl_wrap) * smoothstep(scatter_width * 2.0, scatter_width, ndotl_wrap);
    //scatter *= sun_amount;

    vec3 light_color = mix(light_color_a, light_color_b, scatter);
    vec3 dark_color = mix(dark_color_a, dark_color_b, scatter);


    if(1.0 - transmittance > 0.5) {
        float interpolation = clamp(incoming_light, 0.0, 1.0) / (1.0 - transmittance);

        interpolation = floor(interpolation * (num_steps + 1)) / num_steps;
        interpolation = clamp(interpolation, 0.0, 1.0);

        vec3 color = dark_color + (light_color - dark_color) * interpolation;

        color = mix(color_c, color, ndotl_wrap);

        frag_color = vec4(color, 1.0);        
    } else discard;
    //frag_color = vec4(1.0);

    
    // todo: make sampling direction parallel to particle face
    //if(col.w == 0.0) discard;
}