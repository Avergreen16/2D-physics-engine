#version 460 core

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 translate_rotate;
layout(location = 3) uniform vec3 scale;
layout(location = 4) uniform vec3 player_pos;
layout(location = 5) uniform vec3 light_pos;
layout(location = 6) uniform vec3 inner_axes;
layout(location = 7) uniform vec3 outer_axes;
layout(binding = 0) uniform sampler3D density_tex;
layout(binding = 1) uniform sampler2D depth_tex;
layout(binding = 2) uniform sampler2D color_tex;

layout(location = 0) in vec3 frag_pos;
layout(location = 1) in mat4 inv;
layout(location = 5) in mat4 inv_pv;

out vec4 frag_color;

vec3 light_color = vec3(1.0, 1.0, 1.0);
vec3 dark_color = vec3(0.2, 0.2, 0.2);
float absorbtion = 50;
float light_absorbtion = 6;
float darkness_threshold = 0.05;
vec4 phase_params = {0.4, -0.2, 1.5, 1.5};

const int steps_through = 10;
const int steps_light = 5;
float jitter = 0.5;

float step_size = 0.1;
float max_step_dist = 1;
float light_step_size = 0.1;
float max_light_step_dist = 1.0;

float pi = 3.14159265358979;

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

int worley_units = 8;
int worley_octaves = 1;
int perlin_units = 8;
int perlin_octaves = 1;

float worley(vec3 a) {
    vec3 v = fract(a) * worley_units;
    uvec3 b0 = uvec3(floor(v));
    float dist = 100;

    for(int z = -1; z <= 1; ++z) {
        for(int y = -1; y <= 1; ++y) {
            for(int x = -1; x <= 1; ++x) {
                ivec3 b1 = ivec3(b0) + ivec3(x, y, z);
                if(b1.x < 0) b1.x = worley_units - 1;
                if(b1.y < 0) b1.y = worley_units - 1;
                if(b1.z < 0) b1.z = worley_units - 1;
                uvec3 b2 = uvec3(b1);

                vec3 location = vec3(to_float(hash(b2)), to_float(hash(uvec3(b2.y, b2.x, b2.z + 20))), to_float(hash(uvec3(b2.x + 9, b2.z, b2.y)))) * 0.5 + 0.5;

                location += vec3(b2);
                dist = min(dist, length(location - v));
            }
        }
    }

    return 1.0 - dist;
}

float noise(vec3 a) {
    float n = 0.0;
    float accum = 0.0;
    for(int i = 0; i < worley_octaves; ++i) {
        float p = pow(2, i);
        float w = worley(a * p);

        float over_p = 1.0 / p;
        n += w * over_p;
        accum += over_p;
    }
    float worley_noise = n / accum;

    return max(0.0, worley_noise);
}

float hg(float cos_theta, float g) {
    return ((1 - g * g) / pow(1 + g * g - 2 * g * cos_theta, 3.0 / 2));
}

float phase(float cos_theta) {
    float blend = 0.5;
    float hg_blend = hg(cos_theta, phase_params.x) * (1.0 - blend) + hg(cos_theta, phase_params.y) * blend;
    return phase_params.z + hg_blend * phase_params.w;
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

float sample_texture(vec3 pos) {
    return texture(density_tex, pos).r * 2.0 - 1.0;
}

float fractal = 10;

float get_density(vec3 pos) {
    float density_falloff = smoothstep(0.0, 1.0, max(0.0, 1.0 - abs(length((pos) * 32) - 17) * 2));

    float main_tex = sample_texture(fract(pos));
    float detail_tex = sample_texture(fract(pos * fractal));
    float tex = main_tex + 0.5 * detail_tex;
    tex = clamp(tex * density_falloff, 0.0, 1.0);
    return tex;
}

vec3 get_gradient(vec3 pos) {
    vec3 gradient;

    float difference = 1.0 / fractal * 0.01;
    float current_density = get_density(pos);
    gradient.x = get_density(pos + vec3(difference, 0, 0));
    gradient.y = get_density(pos + vec3(0, difference, 0));
    gradient.z = get_density(pos + vec3(0, 0, difference));
    gradient -= current_density;
    gradient = normalize(gradient);
    return gradient;
}


float get_depth(float d) {
    float a = proj[2][2];
    float b = proj[3][2];
    float depth = b / (((d - 0.5) / 0.5) + a);

    return depth;
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

void main() {
    frag_color = vec4(0.0);
    vec3 rel_player_pos = vec3(inv * vec4(player_pos, 1));
    vec3 rel_frag_pos = vec3(inv * vec4(frag_pos, 1));
    vec3 rel_light_pos = vec3(inv * vec4(light_pos, 1));

    vec3 direction = normalize(rel_frag_pos - rel_player_pos);

    ivec2 s = textureSize(depth_tex, 0);
    vec4 color_tex_value = texture(color_tex, gl_FragCoord.xy / vec2(s));
    vec4 depth_tex_value = texture(depth_tex, gl_FragCoord.xy / vec2(s));
    float depth = get_depth(depth_tex_value.r);

    vec4 camera_direction = inv_pv * vec4(0, 0, 1, 1);
    camera_direction / camera_direction.w;
    float project_directions = 1.0 / dot(camera_direction.xyz, direction);
    depth *= project_directions;




    float entry;
    float exit;

    bool intersects = ray_sphere(rel_player_pos, direction, outer_axes, entry, exit);

    if(!intersects) discard;
    if(depth < exit) {
        exit = depth;
    } else {
        float i_n;
        float i_x;
        bool ii = ray_sphere(rel_player_pos, direction, inner_axes, i_n, i_x);
        if(ii) {
            exit = i_x;
        }
    }

    vec3 color = color_tex_value.xyz;
    vec3 cloud_color = {0, 0, 0};
    float transparency = 1.0;
    float total_density_integral = 0.0;

    float h = to_float(hash(uvec3((direction + 1.0) * 4096.0)));
    float step_dist = entry + step_size * (0.5 + h * jitter);
    while(step_dist < exit) {
        step_dist += step_size;
        vec3 step_pos = rel_player_pos + direction * step_dist;

        float density = get_density((step_pos / 16) * 0.5);
        float density_integral = step_size * density * absorbtion;
        
        if(density > 0) {
            vec3 light_direction = normalize(rel_light_pos - step_pos);

            float planet_entry;
            float planet_exit;

            bool intersects_planet = ray_sphere(step_pos, light_direction, inner_axes, planet_entry, planet_exit);

            float light_transmittance = 0.0;
            if(!intersects_planet) {
                float atmo_entry;
                float atmo_exit;

                bool intersects_atmosphere = ray_sphere(step_pos, light_direction, outer_axes, atmo_entry, atmo_exit);

                light_transmittance = 1.0;
                float g = to_float(hash(uvec3((normalize(step_pos) + 1.0 + normalize(direction) + 1.0) * 7703.0)));
                float light_step_dist = light_step_size * (0.5 + g * 0.5);
                

                if(atmo_exit > 0.0) {
                    float total_density = 0.0;
                    while(light_step_dist < min(atmo_exit, max_light_step_dist)) {
                        light_step_dist += light_step_size;
                        vec3 light_step_pos = step_pos + light_direction * light_step_dist;

                        float density_light = get_density((light_step_pos / outer_axes) * 0.5 + 0.5);

                        total_density += density_light * light_step_size * light_absorbtion;
                    }
                    light_transmittance = beer(total_density);
                }
            }
        
            float r = phase(dot(direction, light_direction));
            total_density_integral += density_integral;
            float transmittance = beer_powder(total_density_integral);
            float le = transmittance * light_transmittance * r;
            le = darkness_threshold + le * (1.0 - darkness_threshold);
            float alpha = clamp(1.0 - beer(density_integral), 0.0, 1.0);
            vec4 cc = vec4(light_color * le, alpha);

            cloud_color += cc.rgb * cc.a * transparency;
            transparency *= (1.0 - cc.a);

            if(transparency < 0.05) break;
        }
    }

    frag_color = vec4(color * transparency + cloud_color, 1.0);
}

