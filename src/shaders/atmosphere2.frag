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
    return f * 2.0 - 3.0;                // Range [-1:1]
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
float step_size = 0.1;
float max_step_dist = 1.0;
float light_step_size = 0.1;
float max_light_step_dist = 1.0;

// atmosphere
float scattering_strength = 5.0; // how much light is scattered away
float atmosphere_density_multiplier = 0.2; // density of atmosphere
float atmosphere_density_falloff = 1.0;

vec4 atmosphere_phase_params = vec4(0.4, -0.4, 0.0, 1.0);

vec3 wavelengths = vec3(700, 530, 440);
vec3 scattering_coefficients = vec3(pow(400 / wavelengths.r, 4), pow(400 / wavelengths.g, 4), pow(400 / wavelengths.b, 4)) * scattering_strength;

// clouds
float absorbtion = 50;
float light_absorbtion = 6;
float darkness_threshold = 0.05;
float cloud_density_multiplier = 10.0;

float base_frequency = 1.0;
float detail_frequency = 4;
float void_frequency = 2;

vec4 cloud_phase_params = {0.4, -0.2, 1.5, 1.5};

float sample_texture(vec3 pos) {
    return texture(density_tex, pos).r * 2.0 - 1.0;
}

float get_cloud_density(vec3 a) {
    float length_a = length(a);
    vec3 norm_a = a / length_a;
    float t = sqrt(1.0 / (pow(norm_a.x / (axes.x + height), 2) + pow(norm_a.y / (axes.y + height), 2) + pow(norm_a.z / (axes.z + height), 2)));
    float dist = t - length_a;
    float fraction = 1.0 - dist / height;

    float density_falloff = smoothstep(0.0, 1.0, 1.0 - abs(fraction - 0.3) / 0.1);
    vec3 base_pos = a * base_frequency;

    float main_tex = sample_texture(fract(base_pos));
    float detail_tex = sample_texture(fract(base_pos * detail_frequency));
    float void_tex = sample_texture(fract(base_pos * void_frequency));
    float tex = (main_tex + 0.5 * detail_tex) * void_tex * 5;
    tex = clamp(tex * density_falloff, 0.0, 1.0);
    return tex * cloud_density_multiplier;
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



float hg(float cos_theta, float g) {
    return ((1 - g * g) / pow(1 + g * g - 2 * g * cos_theta, 3.0 / 2));
}

float phase(float cos_theta, vec4 params) {
    float blend = 0.5;
    float hg_blend = hg(cos_theta, params.x) * (1.0 - blend) + hg(cos_theta, params.y) * blend;
    return params.z + hg_blend * params.w;
}

float optical_depth(vec3 ray_origin, vec3 ray_direction, float ray_length, float steps) {
    vec3 sample_point = ray_origin;
    float optical_depth = 0.0;

    float step_size = ray_length / steps;
    for(int i = 0; i < steps; ++i) {
        float local_density = get_atmosphere_density(sample_point);
        optical_depth += local_density * step_size;
        sample_point += ray_direction * step_size;
    }

    return optical_depth;
}

void main() {
    ivec2 s = textureSize(depth_tex, 0);
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

    vec3 total_inscattered_light = vec3(0.0);
    vec3 total_transmittance = vec3(1.0);

    vec3 color = {0, 0, 0};

    float main_transmittance = 1.0;
    float h = to_float(hash(uvec3((direction + 1.0) * 4096.0)));
    float step_dist = entry + step_size * (0.5 + h * 0.5);

    float total_cloud_density = 0.0;
    float total_atmosphere_density = 0.0;
    while(step_dist < exit) {
        vec3 step_pos = rel_player_pos + direction * step_dist;

        float cloud_density = get_cloud_density(step_pos);
        float atmosphere_density = get_atmosphere_density(step_pos);

        total_cloud_density += cloud_density * step_size;
        total_atmosphere_density += atmosphere_density * step_size;
        
        vec3 light_direction = normalize(rel_light_pos - step_pos);

        float atmo_entry;
        float atmo_exit;

        bool intersects_atmosphere = ray_sphere(step_pos, light_direction, axes + height, atmo_entry, atmo_exit);

        if(intersects_atmosphere) {
            float g = to_float(hash(uvec3((normalize(step_pos) + 1.0 + normalize(direction) + 1.0) * 7703.0)));
            float light_step_dist = light_step_size * (0.5 + g * 0.5);

            float light_cloud_density = 0.0;
            float light_atmosphere_density = 0.0;
            while(light_step_dist < atmo_exit) {
                vec3 light_step_pos = step_pos + light_direction * light_step_dist;

                light_cloud_density += get_cloud_density(light_step_pos) * light_step_size;
                light_atmosphere_density += get_atmosphere_density(light_step_pos) * light_step_size;
                light_step_dist += light_step_size;
            }

            float cloud_phase = phase(dot(direction, light_direction), cloud_phase_params);
            float atmosphere_phase = phase(dot(direction, light_direction), atmosphere_phase_params);

            // atmosphere light
            vec3 a_light_transmittance = exp(-(light_atmosphere_density) * scattering_coefficients);
            vec3 a_main_transmittance = exp(-(total_atmosphere_density) * scattering_coefficients);
            float c_light_transmittance = exp(-light_cloud_density);
            float c_main_transmittance = exp(-total_cloud_density);
            //vec3 light_to_cloud = a_light_transmittance * c_light_transmittance * cloud_phase
            vec3 inscattered_light = atmosphere_density * step_size * a_light_transmittance * a_main_transmittance * c_light_transmittance * c_main_transmittance * scattering_coefficients * atmosphere_phase;

            total_transmittance *= exp(-cloud_density * step_size);

            color += inscattered_light;
        }

        /*float cloud_transmittance = exp(-(light_cloud_density + total_cloud_density));
        float transmittance = beer_powder(total_density_integral);
        float le = transmittance * light_transmittance * r;
        le = darkness_threshold + le * (1.0 - darkness_threshold);
        float alpha = clamp(1.0 - beer(density_integral), 0.0, 1.0);
        vec4 cc = vec4(light_color * le, alpha);

        cloud_color += cc.rgb * cc.a * transparency;
        transparency *= (1.0 - cc.a);*/

        step_dist += step_size;
    }

    /*vec3 step_point = rel_player_pos + direction * (entry + 0.01);
    float step_size = (exit - entry) / steps;
    for(int i = 0; i < steps; i++) {
        vec3 light_direction = normalize(rel_light_pos - step_point);
        float light_entry;
        float light_exit;
        bool intersects_light = ray_sphere(step_point, light_direction, axes + height, light_entry, light_exit);
        

        view_ray_od = optical_depth(step_point,  -direction, step_size * i, steps);
        float light_ray_od = optical_depth(step_point, light_direction, light_exit, light_steps);
        float density = get_density(step_point);

        vec3 transmittance = exp(-(view_ray_od + light_ray_od) * scattering_coefficients);

        float scattering_function = phase(dot(direction, light_direction));
        vec3 inscattered_light = density * transmittance * step_size * scattering_coefficients * scattering_function;
        step_point += direction * step_size;


        color += inscattered_light;
    }

    float original_color_transmittance = exp(-view_ray_od);*/
    frag_color = vec4(color_tex_value.xyz * total_transmittance + color, 1.0);
}

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
    return f * 2.0 - 3.0;                // Range [-1:1]
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

float steps = 10;
float light_steps = 10;

// atmosphere
float scattering_strength = 5.0; // how much light is scattered away
float density_multiplier = 1.0; // density of atmosphere
float density_falloff = 1.0;

vec4 phase_params = vec4(0.4, -0.4, 0.0, 1.0);

vec3 wavelengths = vec3(700, 530, 440);
vec3 scattering_coefficients = vec3(pow(400 / wavelengths.r, 4), pow(400 / wavelengths.g, 4), pow(400 / wavelengths.b, 4)) * scattering_strength;

float get_density(vec3 a) {
    float length_a = length(a);
    vec3 norm_a = a / length_a;
    float t = sqrt(1.0 / (pow(norm_a.x / (axes.x + height), 2) + pow(norm_a.y / (axes.y + height), 2) + pow(norm_a.z / (axes.z + height), 2)));
    float dist = t - length_a;
    float fraction = 1.0 - dist / height;
    fraction = clamp(0.0, 1.0, fraction);
    float density = exp(-fraction * density_falloff) * (1 - fraction);

    return density * density_multiplier;
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
    ivec2 s = textureSize(depth_tex, 0);
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

    vec3 color = vec3(0.0);
    float optical_depth_a = 0.0;

    vec3 sample_point_a = rel_player_pos + direction * (entry + 0.01);
    float step_size_a = (exit - entry) / steps;
    for(int i = 0; i < steps; i++) {
        vec3 light_direction = normalize(rel_light_pos - sample_point_a);
        float light_entry;
        float light_exit;
        ray_sphere(sample_point_a, light_direction, axes + height, light_entry, light_exit);
        
        float local_density_a = get_density(sample_point_a);
        optical_depth_a += local_density_a * step_size_a;


        float optical_depth_b = 0.0;
        float step_size_b = light_exit / light_steps;
        vec3 sample_point_b = sample_point_a;
        for(int j = 0; j < light_steps; ++j) {
            float local_density_b = get_density(sample_point_b);
            optical_depth_b += local_density_b * step_size_b;
            sample_point_b += light_direction * step_size_b;
        }

        vec3 transmittance = exp(-(optical_depth_a + optical_depth_b) * scattering_coefficients);

        float scattering_function = phase(dot(direction, light_direction), phase_params);
        vec3 inscattered_light = local_density_a * transmittance * step_size_a * scattering_coefficients;
        sample_point_a += direction * step_size_a;


        color += inscattered_light;
    }

    float original_color_transmittance = exp(-optical_depth_a);
    frag_color = vec4(color_tex_value.xyz * original_color_transmittance + color, 1.0);

    /*vec3 total_inscattered_light = vec3(0.0);
    vec3 total_transmittance = vec3(1.0);

    vec3 color = {0, 0, 0};

    float main_transmittance = 1.0;
    float h = to_float(hash(uvec3((direction + 1.0) * 4096.0)));
    float step_dist = entry + step_size * (0.5 + h * 0.5);

    float total_cloud_density = 0.0;
    float total_atmosphere_density = 0.0;
    while(step_dist < exit) {
        vec3 step_pos = rel_player_pos + direction * step_dist;

        float cloud_density = get_cloud_density(step_pos);
        float atmosphere_density = get_atmosphere_density(step_pos);

        total_cloud_density += cloud_density * step_size;
        total_atmosphere_density += atmosphere_density * step_size;
        
        vec3 light_direction = normalize(rel_light_pos - step_pos);

        float atmo_entry;
        float atmo_exit;

        bool intersects_atmosphere = ray_sphere(step_pos, light_direction, axes + height, atmo_entry, atmo_exit);

        if(intersects_atmosphere) {
            float g = to_float(hash(uvec3((normalize(step_pos) + 1.0 + normalize(direction) + 1.0) * 7703.0)));
            float light_step_dist = light_step_size * (0.5 + g * 0.5);

            float light_cloud_density = 0.0;
            float light_atmosphere_density = 0.0;
            while(light_step_dist < atmo_exit) {
                vec3 light_step_pos = step_pos + light_direction * light_step_dist;

                light_cloud_density += get_cloud_density(light_step_pos) * light_step_size;
                light_atmosphere_density += get_atmosphere_density(light_step_pos) * light_step_size;
                light_step_dist += light_step_size;
            }

            float cloud_phase = phase(dot(direction, light_direction), cloud_phase_params);
            float atmosphere_phase = phase(dot(direction, light_direction), atmosphere_phase_params);

            // atmosphere light
            vec3 a_light_transmittance = exp(-(light_atmosphere_density) * scattering_coefficients);
            vec3 a_main_transmittance = exp(-(total_atmosphere_density) * scattering_coefficients);
            float c_light_transmittance = exp(-light_cloud_density);
            float c_main_transmittance = exp(-total_cloud_density);
            //vec3 light_to_cloud = a_light_transmittance * c_light_transmittance * cloud_phase
            vec3 inscattered_light = atmosphere_density * step_size * a_light_transmittance * a_main_transmittance * c_light_transmittance * c_main_transmittance * scattering_coefficients * atmosphere_phase;

            total_transmittance *= exp(-cloud_density * step_size);

            color += inscattered_light;
        }

        /*float cloud_transmittance = exp(-(light_cloud_density + total_cloud_density));
        float transmittance = beer_powder(total_density_integral);
        float le = transmittance * light_transmittance * r;
        le = darkness_threshold + le * (1.0 - darkness_threshold);
        float alpha = clamp(1.0 - beer(density_integral), 0.0, 1.0);
        vec4 cc = vec4(light_color * le, alpha);

        cloud_color += cc.rgb * cc.a * transparency;
        transparency *= (1.0 - cc.a);*/

        //step_dist += step_size;
    //}*/
}