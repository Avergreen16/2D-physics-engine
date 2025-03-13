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
//layout(location = 5) uniform float height;
//layout(location = 6) uniform float absorbtion;
layout(location = 7) uniform bool is_corona;

//layout(location = 8) uniform vec3 color;
//layout(location = 9) uniform vec3 sunset_color;
//layout(location = 10) uniform vec3 bloom_color;

layout(location = 11) uniform vec3 sun_pos;

in vec3 frag_pos;
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

float get_depth(float d) {
    float a = proj[2][2];
    float b = proj[3][2];
    float depth = (b) / (((d - 0.5) / 0.5) + a);
    if(isinf(depth)) depth = 3.402823E+38;

    return -depth;
}

// a, b, c
// (x/a)^2 + (y/b)^2 + (z/c)^2 = 1
// x = tvx
// y = tvy
// z = tvz
// (tvx/a)^2 + (tvy/b)^2 + (tvz/c)^2 = 1
// tvx^2/a^2 + tvy^2/b^2 + tvz^2/c^2 = 1
// t^2 * vx^2 * 1/a^2 + t^2 * vy^2 * 1/b^2 + t^2 * vz^2 * 1/c^2 = 1
// t^2 * (vx^2/a^2 + vy^2/b^2 + vz^2/c^2) = 1
// t = sqrt(1 / (vx^2/a^2 + vy^2/b^2 + vz^2/c^2));

// step parameters
uint steps = 40;
uint light_steps = 10;

// atmosphere
float scattering_strength = 3.0; // how much light is scattered away
float atmosphere_density_multiplier = 1.0; // density of atmosphere
float atmosphere_density_falloff = 3.0;
float height = 2.0;

vec4 atmosphere_phase_params = vec4(0.4, -0.4, 0.0, 1.0);

vec3 wavelengths = vec3(700, 600, 550);
vec3 scattering_coefficients = vec3(pow(400 / wavelengths.r, 4), pow(400 / wavelengths.g, 4), pow(400 / wavelengths.b, 4)) * scattering_strength;

// clouds
float absorbtion = 30;
float light_absorbtion = 10;

float base_frequency = 1.0 / 16;
float detail_frequency = 5.0;

vec4 cloud_phase_params = {0.4, -0.4, 0.5, 0.5};

vec2 sample_texture(vec3 pos) {
    return texture(density_tex, pos).rg * 2.0 - 1.0;
}

float fractal = 5;

/*float get_density(vec3 pos, float density_falloff) {
    //float n = noise(pos);
    float main_tex = sample_texture(fract(pos));
    float detail_tex = sample_texture(fract(pos * fractal));
    float tex = main_tex + 0.5 * detail_tex;
    return clamp(tex * density_falloff, 0.0, 1.0);
}*/

float get_cloud_density(vec3 a) {
    float length_a = length(a);
    vec3 norm_a = a / length_a;
    float t = sqrt(1.0 / (pow(norm_a.x / (axes.x + height), 2) + pow(norm_a.y / (axes.y + height), 2) + pow(norm_a.z / (axes.z + height), 2)));
    float dist = t - length_a;
    float fraction = 1.0 - dist / height;
    
    vec3 base_pos = a * base_frequency;
    float density_falloff = smoothstep(0.0, 1.0, 1.0 - abs(fraction - ((0.4) / height)) / ((0.2) / height));

    float main_tex = sample_texture(fract(base_pos)).g;
    float detail = sample_texture(fract(base_pos * detail_frequency)).g;
    float tex = main_tex + detail * 0.5;
    tex = clamp(0.0, 1.0, tex * density_falloff);
    return tex;
}

float get_atmosphere_density(vec3 a) {
    float length_a = length(a);
    vec3 norm_a = a / length_a;
    float t = sqrt(1.0 / (pow(norm_a.x / (axes.x + height), 2) + pow(norm_a.y / (axes.y + height), 2) + pow(norm_a.z / (axes.z + height), 2)));
    float dist = t - length_a;
    float fraction = 1.0 - dist / height;
    fraction = clamp(0.0, 1.0, fraction);
    float density = exp(-fraction * atmosphere_density_falloff) * (1 - fraction);

    return density * atmosphere_density_multiplier;
}

float beer(float a) {
    return exp(-a);
}

float powder(float a) {
    return 1.0 - exp(-2 * a);
}

float beer_powder(float a) {
    return beer(a) * powder(a);
}

float hg(float cos_theta, float g) {
    return ((1 - g * g) / pow(1 + g * g - 2 * g * cos_theta, 3.0 / 2));
}

float phase(float cos_theta, vec4 params) {
    float blend = 0.5;
    float hg_blend = hg(cos_theta, params.x) * (1.0 - blend) + hg(cos_theta, params.y) * blend;
    return params.z + hg_blend * params.w;
}

void main() {
    discard;
    ivec2 s = textureSize(depth_tex, 0) / 2;
    vec4 depth_tex_value = texture(depth_tex, gl_FragCoord.xy / vec2(s));
    vec4 color_tex_value = texture(color_tex, gl_FragCoord.xy / vec2(s));
    float depth = get_depth(depth_tex_value.r);
    
    vec3 rel_frag_pos = frag_pos;
    vec3 rel_player_pos = vec3(inv_model * vec4(player_pos, 1.0));
    vec3 rel_light_pos = vec3(inv_model * vec4(sun_pos, 1.0));
    vec3 direction = normalize(rel_frag_pos - rel_player_pos);

    vec3 rot_direction = mat3(view) * mat3(model) * direction;
    float project_directions = rot_direction.z;
    depth /= project_directions;

    float entry;
    float exit;

    bool intersects = ray_sphere(rel_player_pos, direction, axes + height, entry, exit);

    if(!intersects) discard;

    if(depth < exit) {
        exit = depth;
    }

    float total_transmittance = 0.0;

    vec3 color = {0, 0, 0};

    

    float total_cloud_density = 0.0;
    float total_atmosphere_density = 0.0;

    float dist_through = exit - entry;
    float step_size = dist_through / steps;
    if(step_size == 0) discard;

    float h = to_float(hash(uvec3((direction + 1.0) * 4096.0)));
    float step_dist = entry + step_size * h;
    while(step_dist < exit) {
        vec3 step_pos = rel_player_pos + direction * step_dist;

        float cloud_density = get_cloud_density(step_pos);
        float atmosphere_density = get_atmosphere_density(step_pos);
        
        total_cloud_density += cloud_density * step_size * 0.5;
        total_atmosphere_density += atmosphere_density * step_size * 0.5;
        
        vec3 light_direction = normalize(rel_light_pos - step_pos);

        float atmo_entry;
        float atmo_exit;

        ray_sphere(step_pos, light_direction, axes + height, atmo_entry, atmo_exit);

        float light_step_size = atmo_exit / light_steps;

        float g = to_float(hash(uvec3((normalize(step_pos) + 1.0 + normalize(direction) + 1.0) * 7703.0)));
        float light_step_dist = light_step_size * 0.5;

        float light_cloud_density = 0.0;
        float light_atmosphere_density = 0.0;
        while(light_step_dist < atmo_exit) {
            vec3 light_step_pos = step_pos + light_direction * light_step_dist;

            light_cloud_density += get_cloud_density(light_step_pos) * light_step_size;
            light_atmosphere_density += get_atmosphere_density(light_step_pos) * light_step_size;
            light_step_dist += light_step_size;
        }

        float cloud_phase = phase(dot(direction, light_direction), cloud_phase_params);
        float atmosphere_phase = 1.0;//phase(dot(direction, light_direction), atmosphere_phase_params);

        // atmosphere light
        vec3 a_light_transmittance = exp(-light_atmosphere_density * scattering_coefficients);
        vec3 a_main_transmittance = exp(-total_atmosphere_density * scattering_coefficients);
        float c_light_transmittance = exp(-light_cloud_density * light_absorbtion);
        float c_main_transmittance = exp(-total_cloud_density * absorbtion);
        
        vec3 light_to_cloud = a_light_transmittance * c_light_transmittance;
        vec3 atmosphere_light = (atmosphere_density * scattering_coefficients * step_size) * a_light_transmittance * c_light_transmittance * a_main_transmittance * c_main_transmittance * atmosphere_phase;

        vec3 cloud_light = light_to_cloud * c_main_transmittance * a_main_transmittance * (cloud_density * absorbtion * step_size);

        vec3 inscattered_light = atmosphere_light + cloud_light;

        color += inscattered_light;
        
        step_dist += step_size;

        total_cloud_density += cloud_density * step_size * 0.5;
        total_atmosphere_density += atmosphere_density * step_size * 0.5;

        float max_color = max(cloud_light.r, max(cloud_light.g, cloud_light.b));;
    }

    total_transmittance = exp(-(total_cloud_density * absorbtion + total_atmosphere_density));

    frag_color = vec4(color / (1.0 - total_transmittance), 1.0 - total_transmittance);
}