#version 460 core

layout(binding = 0) uniform sampler2D depth_tex;
layout(binding = 1) uniform sampler3D density_tex;
layout(binding = 2) uniform sampler2D color_tex;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;
layout(location = 3) uniform vec3 player_pos;
layout(location = 4) uniform vec3 axes;
layout(location = 5) uniform float height;
layout(location = 6) uniform vec2 aspect_ratio;

//vec3 atmo_color = vec3(0, 0.4, 1.0);
//vec3 sunset_color = vec3(1.0, 0.6, 0.0);
//vec3 sun_dir = normalize(vec3(0.2, 1, 0.5));
// vec3 YELLOW = vec3(1.0, 0.9, 0.7);

in vec3 frag_pos;
in vec4 view_pos;
in vec2 screen_pos;
flat in mat4 inv_proj;
flat in mat4 inv_view;
flat in mat4 inv_model;

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

vec3 sphere_raycast(vec3 center, float radius, vec3 origin, vec3 dir) {
    vec3 rel_center = origin - center;

    float closest_pos = dot(-rel_center, dir);

    vec3 c = rel_center + dir * closest_pos;

    float dist = length(c);

    if(dist > radius) return vec3(0, closest_pos, 0);

    float t = sqrt(radius * radius - dist * dist);

    float t0 = closest_pos - t;
    float t1 = closest_pos + t;
    t0 = max(t0, 0);

    return vec3(t0, closest_pos, t1);
}

float get_depth(float d, vec3 direction, vec3 facing) {
    vec4 dv = vec4(0, 0, d, 1.0);
    dv = inv_proj * dv;
    dv /= dv.w;

    float scale = dot(direction, facing);
    dv.z /= scale;

    return dv.z;
}

layout(location = 0) out vec4 frag_color;

float soft_light(float a, float b) {
    return (b < 0.5 ?
        (2.0 * a * b + a * a * (1.0 - 2.0 * b)) :
        (2.0 * a * (1.0 - b) + sqrt(a) * (2.0 * b - 1.0))
    );
}

vec3 soft_light2(vec3 a, vec3 b) {
    return vec3(soft_light(a.x, b.x), soft_light(a.y, b.y), soft_light(a.z, b.z));
}

vec2 sample_texture(vec3 pos) {
    return texture(density_tex, pos).rg * 2.0 - 1.0;
}


float sample_density(vec3 pos) {
    float density = 0;

    float w = (length(pos) - axes.x) / height;
    w = clamp(w, 0.0, 1.0);

    w -= 0.4;
    w = abs(w);
    w /= 0.2;
    w = clamp(1.0 - w, 0.0, 1.0);

    if(w > 0.0) {
        float base_frequency = 2.0;
        float detail_frequency = 8.0;
        float detail_frequency2 = 32.0;
        vec3 base_pos = (pos / axes) * base_frequency;

        float main_tex = sample_texture(fract(base_pos)).g;
        float detail = sample_texture(fract(base_pos * detail_frequency)).g;
        float detail2 = sample_texture(fract(base_pos * detail_frequency2)).g;
        float tex = main_tex + detail * 0.5 + detail2 * 0.05;
        density = clamp(0.0, 1.0, tex * w);
    }
    
    return density;
}

float max_iterations = 1;
float step_size = 0.1;
/*for(int i = 0; i < max_iterations; ++i) {
        float dist = length(current_pos);
        float h2 = pow(length(cross(current_pos, current_dir)), 2.0);
        current_dir += -1.5 * h2 * (current_pos) / pow(pow(dist, 2.0), 2.5) * step_size;
        current_dir = normalize(current_dir);
        if(dist < 1) eh = true;

        current_pos += current_dir;
    }*/

float size = axes.x;

vec4 ray_trace(vec3 ray_dir, vec3 ray_pos, vec2 aspect, vec2 dir, float screen_fraction) {
    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);

    bool eh = false;

    vec3 current_dir = ray_dir;
    vec3 current_pos = ray_pos / axes.x * 2;

    vec3 i = sphere_raycast(vec3(0.0), size, ray_pos, ray_dir);
    vec3 j = sphere_raycast(vec3(0.0), size * 3, ray_pos, ray_dir);

    i.y = max(0.0, i.y);
    j.y = max(0.0, j.y);
    
    float L = length(ray_pos + ray_dir * j.y);
    float fade = (L - size) / (size * 0.1);
    fade = clamp(0.0, 1.0, fade);

    L = 1.0 - (L - size) / (size * 2);
    //fresnel = acos(fresnel);

    if(i.z == 0 || i.z < 0.0) {
        vec2 new_pos = (current_dir.xy / -current_dir.z / aspect) * 0.5 + 0.5;
        if(j.z != 0 && j.z > 0.0) new_pos -= dir * pow(L, 3) * 5 * screen_fraction;

        if(!eh) {
            int x_n = 1;
            int y_n = 1;
            //if(new_pos.x < 0) x_n = 0;
            //if(new_pos.y < 0) y_n = 0;

            if(abs(int(floor(new_pos.x)) % 2) == x_n) new_pos.x = 1.0 - fract(new_pos.x);
            else new_pos.x = fract(new_pos.x);
            if(abs(int(floor(new_pos.y)) % 2) == y_n) new_pos.y = 1.0 - fract(new_pos.y);
            else new_pos.y = fract(new_pos.y);

            color = texture(color_tex, new_pos);

            color.xyz = mix(color.xyz * 2.5, color.xyz, fade);
        }

        //color.r += L;
    }/* else {
        float k = length(ray_pos + ray_dir * i.y);
        k = clamp(0.0, 1.0, 1.0 - (5000 - k) / 1000);
        k = pow(k, 2);
        color.xyz = vec3(1.0, 0.2, 0.8) * 0.2 * k;
    }*/

    return color;
}

void main() {
    vec4 x = inv_proj * vec4(1, 1, 0.5, 1);
    x /= x.w;
    vec3 v = x.xyz;//normalize(x.xyz);
    v /= v.z;
    v = -v;


    vec3 ray_dir = normalize(frag_pos);
    vec3 center = vec3(model * vec4(0.0, 0.0, 0.0, 1.0));

    /*float pixel_size = 50000 / 32;
    vec3 n_ray_dir = (mat3(view) * ray_dir);
    n_ray_dir /= abs(n_ray_dir.z);
    float center_dist = length(center);

    vec4 ps = proj * vec4(pixel_size, 0, -center_dist, 1.0);
    ps /= ps.w;
    pixel_size = ps.x;

    vec4 center_p = proj * vec4(center, 1.0);
    center_p /= center_p.w;
    vec2 center_offset = center_p.xy;

    pixel_size = min(pixel_size, 1.0 / 100);

    n_ray_dir = vec3(round((n_ray_dir.xy - center_offset) / pixel_size) * pixel_size + center_offset, n_ray_dir.z);
    n_ray_dir = normalize(n_ray_dir);
    
    vec3 prev_ray_dir = ray_dir;

    ray_dir = n_ray_dir;*/
    ray_dir = mat3(view) * ray_dir;


    vec3 r_origin = -center;



    //vec3 ray_dir = mat3(view) * normalize(frag_pos);
    vec3 ray_origin = -(mat3(view) * vec3(model * vec4(0.0, 0.0, 0.0, 1.0)));
    vec3 black_hole = vec3(0.0);

    //float radius = axes.x + height;

    vec4 center_pos = proj * vec4(-ray_origin, 1.0);
    center_pos /= center_pos.w;

    vec4 frag_pos_p = proj * view * vec4(frag_pos, 1.0);
    frag_pos_p /= frag_pos_p.w;

    vec2 center_screen_pos = center_pos.xy;
    vec2 frag_screen_pos = frag_pos_p.xy;

    vec2 diff = center_screen_pos - frag_screen_pos;
    diff = normalize(diff);

    float screen_fraction = -ray_origin.z * tan(45.0 * (3.14159 / 180));
    screen_fraction = size / screen_fraction;

    vec4 color = ray_trace(ray_dir, ray_origin, v.xy, diff, screen_fraction);

    frag_color = color;
    frag_color.w = 1.0;
}