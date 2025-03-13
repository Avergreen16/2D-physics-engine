#version 460 core

out vec4 frag_color;

layout(binding = 0) uniform sampler2D color_tex;
layout(binding = 1) uniform sampler2D depth_tex;
layout(binding = 2) uniform sampler3D density_tex;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;

layout(location = 3) uniform vec3 player_pos;
layout(location = 4) uniform vec3 axes;
layout(location = 5) uniform float height;
layout(location = 6) uniform float density;
layout(location = 7) uniform bool emits_light;
layout(location = 8) uniform vec3 color;
layout(location = 11) uniform vec3 sun_pos;
layout(location = 12) uniform ivec2 screen_size;

in vec3 frag_pos;
in vec4 view_pos;
in vec2 screen_pos;
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

bool ray_sphere(vec3 origin, vec3 direction, vec3 size, out float i0, out float i1) {
    vec3 o = origin / size;
    vec3 d = normalize(direction / size);

    float projection = dot(d, -o);
    vec3 c = d * projection + o;
    float a = length(c);
    if(a > 1.0) return false;
    float b = sqrt(1 - a * a);

    float dist_c = length((c - o) * size) * sign(projection);
    float b1 = length(direction * b * size);
    i0 = max(0.0, dist_c - b1);
    i1 = dist_c + b1;
    if(i1 < 0) return false;
    return true;
}

float atmosphere_falloff = 3;
float atmosphere_multiplier = 1;

float base_frequency = 2;
float detail_frequency = 6;
float void_frequency = 0.25;

vec2 sample_texture(vec3 pos) {
    return texture(density_tex, pos).rg * 2.0 - 1.0;
}

float get_cloud_density(vec3 a) {
    float length_a = length(a);
    vec3 norm_a = a / length_a;
    float t = sqrt(1.0 / (pow(norm_a.x / (axes.x), 2) + pow(norm_a.y / (axes.y), 2) + pow(norm_a.z / (axes.z), 2)));
    float dist = length_a - t;
    float fraction = dist / height;
    
    vec3 base_pos = a / axes.x * base_frequency;
    float density_falloff = smoothstep(0.0, 1.0, 1.0 - abs((fraction - 0.4) / 0.2));
    if(density_falloff <= 0) return 0;

    float main_tex = sample_texture(fract(base_pos)).g;
    float detail = sample_texture(fract(base_pos * detail_frequency)).g;
    float void_v = sample_texture(fract(base_pos * void_frequency)).g;
    float tex = (main_tex + detail * 0.5) * max(0, void_v);
    tex = clamp(tex * density_falloff, 0.0, 1.0);
    return tex;
}

float get_atmosphere_density(vec3 a) {
    float length_a = length(a);
    vec3 norm_a = a / length_a;
    float t = sqrt(1.0 / (pow(norm_a.x / (axes.x), 2) + pow(norm_a.y / (axes.y), 2) + pow(norm_a.z / (axes.z), 2)));
    float dist = length_a - t;
    float fraction = dist / height;
    fraction = clamp(0.0, 1.0, fraction);
    float density = exp(-fraction * atmosphere_falloff) * (1 - fraction);

    return density * atmosphere_multiplier;
}

float hg(float cos_theta, float g) {
    return ((1 - g * g) / pow(1 + g * g - 2 * g * cos_theta, 3.0 / 2));
}

vec3 light_color = vec3(1.0, 1.0, 1.0);

float scattering_strength = 3;

vec3 wavelengths = vec3(700, 600, 550);
vec3 scattering_coefficients = vec3(pow(400 / wavelengths.r, 4), pow(400 / wavelengths.g, 4), pow(400 / wavelengths.b, 4)) * scattering_strength;

void main() {
    vec4 vp = view_pos;
    //pp.x = round(pp.x * (screen_size.x / 4) / pp.w) / ((screen_size.x / 4) / pp.w);
    //pp.y = round(pp.y * (screen_size.y / 4) / pp.w) / ((screen_size.y / 4) / pp.w);
    vec3 frag_dir = vec3(inv_view * vp);
    frag_dir = normalize(frag_dir);
    vec3 center = vec3(model * vec4(0.0, 0.0, 0.0, 1.0));
    vec3 r_origin = -center;
    vec3 sun_dir = mat3(inv_model) * normalize(sun_pos);

    vec3 facing = vec3(inv_view * vec4(0, 0, -1, 1));

    ivec2 fc = ivec2(floor(gl_FragCoord.xy));
    vec3 d = texelFetch(color_tex, fc, 0).xyz;
    float depth = texelFetch(depth_tex, fc, 0).x;
    depth = -get_depth(depth, frag_dir, facing);

    frag_dir = mat3(inv_model) * frag_dir;
    r_origin = mat3(inv_model) * r_origin;

    float i0;
    float i1;

    bool intersection = ray_sphere(r_origin, frag_dir, axes + height, i0, i1);
    float a = 0;
    float aa = 0;
    vec3 incoming_light = {0, 0, 0};
    vec3 sky_accum = vec3(0, 0, 0);
    vec3 cloud_accum = vec3(0, 0, 0);

    float enter = min(i0, depth);
    float exit = min(i1, depth);

    if(intersection) {

        float thickness = exit - enter;

        int steps_through = 20;
        int steps_through_sun = 5;
        float factor = 1.5;
        float prev_L = 0;
        for(int i = 0; i < steps_through; ++i) {
            float L = pow(float(i + 1) / steps_through, factor);
            float range = L - prev_L;
            float dist = float(enter + thickness * (prev_L + range * to_float(hash(uvec3((frag_dir + 1) * 8726)))));
            prev_L = L;
            vec3 pos = r_origin + frag_dir * dist;

            float density = get_cloud_density(pos);
            float atmo_density = get_atmosphere_density(pos);
            
            float step_width = (thickness / steps_through / height);

            float b = 0;
            float ba = 0;

            float i0;
            float i1;
            float i2;
            float i3;

            bool intersection_0 = ray_sphere(pos, sun_dir, axes + height, i0, i1);
            bool intersection_1 = ray_sphere(pos, sun_dir, axes, i2, i3);

            if(!intersection_1) {
                float exit = i1;
                //if(exit / height > 0.01) {

                    for(int j = 1; j < steps_through_sun + 1; ++j) {
                        vec3 pos2 = pos + sun_dir * (exit * (float(j) + to_float(hash(uvec3((frag_dir + 1) * 8726) * j))) / (steps_through_sun + 1));

                        float density_2 = get_cloud_density(pos2);
                        float atmo_density_2 = get_atmosphere_density(pos2);

                        if(density_2 > 0) b += density_2 * (exit / (steps_through_sun + 1) / height);
                        if(atmo_density_2 > 0) ba += atmo_density_2 * (exit / (steps_through_sun + 1) / height);
                    }
                //}
            } else {
                ba = 1000000;
            }

            vec3 a_exp = vec3(exp(-a * 50 + -b * 50)) * exp((-aa + -ba) * scattering_coefficients);

            vec3 cloud_color = a_exp * (clamp(density, 0.0, 1.0) * 50) * (range * thickness / height) * light_color;
            vec3 sky_color = a_exp * (max(atmo_density, 0.0)) * (range * thickness / height) * scattering_coefficients * light_color;

            cloud_accum += cloud_color;
            sky_accum += sky_color;
            
            if(density > 0) a += density * (range * thickness / height);
            if(atmo_density > 0) aa += atmo_density * (range * thickness / height);
        }
    }
    
    a = 1 - exp(-a * 50 + -aa * 0.5);

    /*alpha = pow(alpha, 0.5);
    alpha = round(alpha * (32.0 / density)) / (32.0 / density);
    alpha = pow(alpha, 2);*/


    /*float value = texture(density_tex, rel_p / axes).r;

    if(v) {  
        float masking = 1.0 - exp(-(p1 - p0) / height / density * 0.15);
        alpha *= masking;
    }*/

    //cloud_accum = sqrt(cloud_accum);
    //sky_accum = sqrt(sky_accum);

    /*(float m = max(sky_color.x, max(sky_color.y, sky_color.z));

    if(m != 0) {
        //sky_color /= m;
        
        sky_color *= alpha;
    } else sky_color = vec3(0);*/

    incoming_light += cloud_accum + sky_accum;
    //if(incoming_light.x > 1 || incoming_light.y > 1 || incoming_light.z > 1) incoming_light /= max(incoming_light.x, max(incoming_light.y, incoming_light.z));

    frag_color = vec4(incoming_light, clamp(a, 0.0, 1.0));
}