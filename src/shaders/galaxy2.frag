#version 460 core

out vec4 frag_color;

layout(binding = 0) uniform sampler2D depth_tex;
layout(binding = 1) uniform sampler3D density_tex;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;

layout(location = 3) uniform vec3 player_pos;
layout(location = 4) uniform vec3 axes;
layout(location = 5) uniform float light_factor;

in vec3 frag_pos;
in vec4 view_pos;
flat in mat4 inv_proj;
flat in mat4 inv_view;
flat in mat4 inv_model;

float falloff = 1.0;

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

float get_depth(float d, vec3 direction, vec3 facing) {
    vec4 dv = vec4(0, 0, d, 1.0);
    dv = inv_proj * dv;
    dv /= dv.w;

    float scale = dot(direction, facing);
    dv.z /= scale;

    return dv.z;
}

vec3 get_pos(vec4 p) {
    vec4 a = inv_proj * p;
    a /= a.w;
    a = inv_view * a;

    return vec3(a);
}

float ray_plane(vec3 origin, vec3 direction, vec3 normal) {
    float f0 = dot(origin, normal);
    float f1 = dot(direction, normal);

    return -f0 / f1;
}

bool ray_cube(vec3 origin, vec3 direction, vec3 axes, out float i0, out float i1) {
    float enter = 3.402823466E+38;
    float exit = 3.402823466E+38;

    vec3 positions[6] = {
        vec3(axes.x, 0, 0),
        vec3(0, axes.y, 0),
        vec3(0, 0, axes.z),
        vec3(-axes.x, 0, 0),
        vec3(0, -axes.y, 0),
        vec3(0, 0, -axes.z)
    };

    vec3 normals[6] = {
        vec3(1, 0, 0),
        vec3(0, 1, 0),
        vec3(0, 0, 1),
        vec3(-1, 0, 0),
        vec3(0, -1, 0),
        vec3(0, 0, -1)
    };

    int n[6] = {
        0, 1, 2, 0, 1, 2
    };

    for(int i = 0; i < 6; ++i) {
        float a = dot(origin - positions[i], normals[i]);
        float b = dot(direction, normals[i]);
        float c = -a / b;
        if(c > 0) {
            vec3 v = origin + direction * c;

            int ni = n[i];

            int n0 = int(mod(ni - 1, 3));
            int n1 = int(mod(ni + 1, 3));

            if(abs(v[n0]) < axes[n0] && abs(v[n1]) < axes[n1]) {
                if(b > 0) {
                    if(c < exit) exit = c;
                } else {
                    if(c < enter) enter = c;
                }
            }
        }
    }

    if(enter == 3.402823466E+38) enter = 0;
    
    vec3 v = origin + direction * (enter + exit) * 0.5;

    v = abs(v);

    if(v.x < axes.x && v.y < axes.y && v.z < axes.z) {
        i0 = enter;
        i1 = exit;

        return true;
    } else {
        return false;
    }
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

mat3 z_rot(float r) {
    vec3 x = vec3(cos(r), sin(r), 0);
    vec3 y = vec3(-sin(r), cos(r), 0);
    vec3 z = vec3(0, 0, 1);

    return mat3(x, y, z);
}

float sample_octaves(vec3 v, int octaves, float roughness) {
    vec3 a = v;

    float tex = 0.0;
    for(int i = 0; i < octaves; ++i) {
        float tex_sample = texture(density_tex, fract(a)).x * 2.0 - 1.0;
        tex += tex_sample * pow(roughness, i);
        a /= 2;
    }
    
    return tex;
}

float sample_texture(vec3 v) {
    float l = length(v.xy);

    mat3 zr = z_rot(l * 5);

    vec3 ff = zr * v;

    vec2 f = ff.xy / l;

    vec3 vv = vec3(ff.xy, 0.0);
    vv *= 0.5;

    float ss_scale = 2;

    float scale_sample_x = sample_octaves((v + vec3(0.8, 0.2, 0.3)) * ss_scale, 2, 0.8);
    float scale_sample_y = sample_octaves((v + vec3(0.6, 0.9, 0.7)) * ss_scale, 2, 0.8);
    float scale_sample_z = sample_octaves((v + vec3(0.1, 0.6, 0.5)) * ss_scale, 2, 0.8);

    float mix_v = 0.25;

    vv = mix(vv, vv + vec3(scale_sample_x, scale_sample_y, scale_sample_z), mix_v);

    float tex_sample = sample_octaves(vv, 1, 0.5);

    return tex_sample;
}

float density_value = 2.0;

void main() {
    vec4 vp = view_pos;
    //pp.x = round(pp.x * (screen_size.x / 4) / pp.w) / ((screen_size.x / 4) / pp.w);
    //pp.y = round(pp.y * (screen_size.y / 4) / pp.w) / ((screen_size.y / 4) / pp.w);
    vec3 frag_dir = vec3(inv_view * vp);
    frag_dir = normalize(frag_dir);
    vec3 center = vec3(model * vec4(0.0, 0.0, 0.0, 1.0));

    float pixel_size = axes.x / 24;
    vec3 n_frag_dir = (mat3(view) * frag_dir);
    n_frag_dir /= abs(n_frag_dir.z);
    float center_dist = length(center);

    vec4 ps = proj * vec4(pixel_size, 0, -center_dist, 1.0);
    ps /= ps.w;
    pixel_size = ps.x;

    vec4 center_pos = proj * vec4(center, 1.0);
    center_pos /= center_pos.w;
    vec2 center_offset = center_pos.xy;

    pixel_size = min(pixel_size, 1.0 / 100);

    n_frag_dir = vec3(round((n_frag_dir.xy - center_offset) / pixel_size) * pixel_size + center_offset, n_frag_dir.z);
    n_frag_dir = normalize(n_frag_dir);
    
    vec3 prev_frag_dir = frag_dir;

    frag_dir = mat3(inv_view) * n_frag_dir;


    vec3 r_origin = -center;

    vec3 facing = vec3(inv_view * vec4(0, 0, -1, 1));

    ivec2 fc = ivec2(floor(gl_FragCoord.xy));

    float depth = texelFetch(depth_tex, fc, 0).x;
    if(depth == 0) depth = pow(2, 72);
    else depth = -get_depth(depth, prev_frag_dir, facing);


    frag_dir = mat3(inv_model) * frag_dir;
    r_origin = mat3(inv_model) * r_origin;

    vec3 axes_v = axes;
    //axes_v.z = pow(2, 44);



    float i0;
    float i1;

    mat3 matv;
    matv = mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

    bool intersection = ray_cube(r_origin, frag_dir, axes_v, i0, i1);
    /*bool intersection = (v.x != v.z);
    i0 = v.x;
    i1 = v.z;*/

    float enter = min(max(i0, 0.0), depth);
    float exit = min(i1, depth);

    float transmittance = 0;
    vec3 accum_color = vec3(0, 0, 0);

    vec3 far = vec3(0.45, 0.2, 1.0);
    //vec3 far = vec3(1.0, 0.45, 0.45);
    vec3 near = vec3(1.0, 0.75, 0.5);
    vec3 dust = vec3(0.2, 0.1, 0.1);

    if(intersection) {
        int steps_through = 100;
        float thickness = exit - enter;

        for(int i = 0; i < steps_through; ++i) {
            float r = to_float(hash(uvec3(frag_dir * i * 500 + 0x7FFFFFFF)));
            float dist = enter + thickness * ((float(i + 0.5)) / (steps_through));
            float width = thickness / steps_through / (pow(2, 44) * 10);

            if(dist >= 0) {
                vec3 pos = r_origin + frag_dir * dist;

                vec3 pos2 = pos / axes_v;

                vec3 vv = vec3(pos2.xy, 0);

                float bulge_factor = length(pos2);
                bulge_factor = abs(bulge_factor);
                bulge_factor = pow(bulge_factor + 1.0, -4) * (1.0 - bulge_factor);
                bulge_factor = max(bulge_factor, 0.0);

                bulge_factor *= 0.3;
                float density = bulge_factor;
                vec3 c = vec3(1.0, 0.75, 0.5);
                
                //transmittance += density * (width);
                accum_color += c * density * (width) * exp(-transmittance);
            }
        }
    } else accum_color = vec3(0, 0, 0);

    transmittance = exp(-transmittance);

    //accum_color = vec3(pow(accum_color.x, 0.5), pow(accum_color.y, 0.5), pow(accum_color.z, 0.5));

    //accum_color = round(accum_color * 16) / 16;

    frag_color = vec4(accum_color * light_factor * 0.2, clamp(1-transmittance, 0.0, 1.0));

    //frag_color = vec4(depth / pow(2, 50), 0.0, 0.0, 1.0);
}