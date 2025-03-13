#version 460 core

layout(binding = 0) uniform sampler2D depth_tex;
layout(binding = 1) uniform sampler3D density_tex;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;
layout(location = 3) uniform vec3 player_pos;
layout(location = 4) uniform vec3 axes;
layout(location = 5) uniform float height;

//vec3 atmo_color = vec3(0, 0.4, 1.0);
//vec3 sunset_color = vec3(1.0, 0.6, 0.0);
//vec3 sun_dir = normalize(vec3(0.2, 1, 0.5));
// vec3 YELLOW = vec3(1.0, 0.9, 0.7);

layout(location = 6) uniform vec3 atmo_color;
layout(location = 7) uniform vec3 sunset_color;
layout(location = 8) uniform vec3 specular_color;
layout(location = 9) uniform vec3 sun_dir;

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

struct tr_data {
    float a0;
    float a1;
    float a2;
    vec3 up;
};

vec3 radii = vec3(axes.x - axes.y, axes.y, axes.z);
float thickness = height;

/*2float torus_sdf(vec3 radii, vec3 pos, out vec3 up) {
    vec3 closest_pos = normalize(vec3(pos.x, pos.y, 0)) * radii.x;
    
    float len = length(closest_pos - pos);

    float dist = len - (radii.y + thickness);
    up = pos - closest_pos;

    return dist;
}*/

float torus_sdf(vec3 radii, vec3 pos, out vec3 up) {
    vec3 closest_pos = normalize(vec3(pos.x, pos.y, 0)) * radii.x;

    vec3 diff = pos - closest_pos;

    vec3 norm_cp = normalize(closest_pos);

    vec2 ellipse_pos = vec2(dot(diff, norm_cp), diff.z);

    vec2 rad_v = radii.yz + thickness;

    vec2 rel_pos = ellipse_pos / rad_v;
    vec2 ecp = normalize(rel_pos);
    ecp *= rad_v;
    rel_pos *= rad_v;

    float dist = length(rel_pos - ecp);
    if(length(rel_pos) < length(ecp)) dist = -dist;

    vec2 up_v = normalize(vec2(2 * ellipse_pos.x / (radii.y * radii.y), 2 * ellipse_pos.y / (radii.z * radii.z)));
    up = vec3(0, 0, up_v.y) + norm_cp * up_v.x;
    up *= dist + (radii.y + thickness);
    

    return dist;
}

/*bool torus_raycast(vec3 center, vec3 radii, vec3 origin, vec3 dir) {
    tr_data t;

    vec3 ray_pos = origin - center;
    vec3 ray_dir = dir;

    bool second = false;
    bool inside = false;

    bool intersected = false;

    float prev = 3e+38;

    for(int i = 0; i < 256; ++i) {
        float sdf = torus_sdf(radii, ray_pos);
        ray_pos += ray_dir * sdf;

        if(sdf < min_d) {
            intersected = true;
            break;
        } else if(sdf > max_d && prev < sdf) {
            break;
        }
        prev = sdf;
    }
    return intersected;
}*/

uint num_steps = 256;

tr_data torus_raycast(vec3 start, vec3 end, vec3 radii, float step_ahead, float final_dist) {
    tr_data t;
    t.a0 = 0;
    t.a1 = 0;
    t.a2 = 0;

    vec3 ray_pos = start;
    vec3 ray_dir = end - start;
    float ray_length = length(ray_dir);
    ray_dir /= ray_length;

    float closest = 3e+38;
    bool inside = false;
    bool collect_closest = true;

    for(int i = 0; i < num_steps; ++i) {
        float current_dist = ray_length * (float(i) / num_steps);
        vec3 current_pos = ray_pos + ray_dir * current_dist;

        vec3 up;

        float d = torus_sdf(radii, current_pos, up);
        if(inside) {
            if(d > 0) {
                inside = false;
                t.a2 = current_dist + step_ahead;

                return t;
            }
        } else {
            if(d <= 0) {
                inside = true;
                t.a0 = current_dist + step_ahead;
            }
        }

        //if(d > closest && d <= 0 && collect_closest) collect_closest = false;

        if(d < closest && collect_closest) {
            closest = d;
            t.a1 = current_dist + step_ahead;
            t.up = up;
        }
    }
    t.a2 = final_dist;
    return t;
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

float density = 0.1;

void main() {
    vec3 facing = vec3(inv_view * vec4(0, 0, -1, 1));
    vec3 dir = normalize(frag_pos);

    ivec2 fc = ivec2(floor(gl_FragCoord.xy));
    float depth_value = texelFetch(depth_tex, fc, 0).r;
    if(depth_value == 0) depth_value = 50000000000.0;
    else depth_value = -get_depth(depth_value, dir, facing);


    vec3 ray_dir = normalize(frag_pos);
    vec3 center = vec3(model * vec4(0.0, 0.0, 0.0, 1.0));
    float radius = axes.x + height;

    float pixel_size = radius / 256;
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

    //n_ray_dir = vec3(round((n_ray_dir.xy - center_offset) / pixel_size) * pixel_size + center_offset, n_ray_dir.z);
    n_ray_dir = normalize(n_ray_dir);
    
    vec3 prev_ray_dir = ray_dir;

    ray_dir = n_ray_dir;

    dir = mat3(inv_view) * ray_dir;


    vec3 origin = vec3(0.0);

    vec3 ret0 = sphere_raycast(center, radius, origin, dir);

    
    //bool rayc = torus_raycast(center, radii, origin, dir);

    //if(rayc) frag_color = vec4(1, 1, 1, 1);
    //else frag_color = vec4(0, 0, 0, 0);
    vec3 start = origin + dir * max(ret0.x, 0) - center;
    vec3 end = origin + dir * min(ret0.z, depth_value) - center;

    tr_data ret = torus_raycast(start, end, radii, max(ret0.x, 0), min(ret0.z, depth_value));

    vec3 rayc = vec3(ret.a0, ret.a1, ret.a2);
    vec3 up = ret.up;

    if(rayc.z <= 0) discard;


    if(depth_value < rayc.y) {
        rayc.y = depth_value;
    }



    vec3 y_diff = up;

    float y_len = length(y_diff);

    float i = (y_len - radii.y) / thickness;
    float fi = i;
    i = 1.0 - i;
    i = clamp(i, 0.0, 1.0);

    //i = pow(i, 0.7);

    if(sun_dir == vec3(0.0)) {
        vec3 color = atmo_color;
        if(depth_value < rayc.z) discard;
        else {
            frag_color = vec4(color * i, 0.0);
        }
    } else {
        vec3 normal = normalize(y_diff);

        float ndotl = dot(normal, sun_dir);
        float wrap = 0.35;
        float ndotl_wrap = clamp((ndotl + wrap) / (1.0 + wrap), 0.0, 1.0);

        float rdots = max(0.0, dot(dir, sun_dir));
        float sun_amount = rdots;

        vec3 back_lighting_color = specular_color * 0.1;
        vec3 front_lighting_color = mix(atmo_color, specular_color, pow(sun_amount, 32.0));
        
        vec3 fog_color = mix(back_lighting_color, front_lighting_color, ndotl_wrap);

        float extinction_factor = ndotl_wrap * i;

        float scatter_width = 0.38;
        float scatter = smoothstep(0.0, scatter_width, ndotl_wrap) * smoothstep(scatter_width * 2.0, scatter_width, ndotl_wrap);
        scatter *= sun_amount;

        // sun
        float specular = pow((rdots + 0.5) / 1.5, 64);

        float fresnel = 1.0 - clamp(dot(-dir, normal), 0.0, 1.0);
        fresnel *= fresnel;

        float sun_factor = fi / 5;
        sun_factor = 1.0 - clamp(sun_factor, 0.0, 1.0);
        sun_factor *= sun_factor;
        sun_factor *= sun_factor;
        sun_factor *= specular * fresnel;

        vec3 base_color = mix(fog_color, sunset_color, scatter);

        base_color = base_color * extinction_factor;

        vec3 color = base_color;

        float start = rayc.x;
        float dist = min(depth_value, rayc.z) - rayc.x;

        if(depth_value <= rayc.z) {
            rayc.z = depth_value;

            float thickness2 = min(max(rayc.z - rayc.x, 0.0), thickness);

            float transmittance = 1.0 - exp(-thickness2 / (thickness) * density);
            transmittance *= 1.5;

            frag_color = vec4(color * transmittance, transmittance);
        } else {
            frag_color = vec4(color, 0.0);
        }
    }
}