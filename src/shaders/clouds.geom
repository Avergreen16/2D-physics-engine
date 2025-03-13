#version 460 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 translate_rotate;
layout(location = 3) uniform vec3 player_pos;
layout(location = 4) uniform vec3 light_pos;
layout(location = 5) uniform vec3 inner_axes;
layout(location = 6) uniform vec3 outer_axes;
layout(location = 7) uniform uvec3 points_count;
layout(location = 9) uniform vec3 points_range;
layout(binding = 0) uniform sampler3D density_tex;

vec3 light_color = vec3(0.95, 0.95, 1.0);
vec3 dark_color = vec3(0.03, 0.03, 0.06);
float absorbtion = 50;
float light_absorbtion = 10;
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
    return hash(v.x ^ hash(v.y)) ^ hash(v.z); 
}

uint hash(uvec4 v) {
    return hash(v.x ^ hash(v.y)) ^ hash(v.z) ^ hash(v.w); 
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
    return texture(density_tex, pos).g * 2.0 - 1.0;
}

float base = 1.0 / 16;
float main_detail = 8;
float void_detail = 2;
float high_low = 4;

float get_density(vec3 pos) {
    vec3 base_pos = pos * base;

    float high_low_tex = sample_texture(fract(base_pos * high_low));
    float density_falloff = smoothstep(0.0, 1.0, max(0.0, 1.0 - abs(length(pos) - (inner_axes.x + (outer_axes.x - inner_axes.x) * (0.3 + high_low_tex * 0.3))) / ((outer_axes.x - inner_axes.x) * (0.1 + high_low_tex * 0.3))));

    float main_tex = sample_texture(fract(base_pos * main_detail));
    //float detail_tex = sample_texture(fract(base_pos * detail));
    float void_tex = sample_texture(fract(base_pos * void_detail));
    float tex = main_tex - 5 * void_tex;
    tex = clamp(tex * density_falloff, 0.0, 1.0);
    return tex;
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

flat in uint vari[];

out vec2 uv;
out vec4 color;
flat out uint variation;

float cloud_threshold = 0.05;
float particle_size = points_range.x / points_count.x;


float steps = 10;
float light_steps = 10;
float density_multiplier = 0.2; // density of atmosphere
float scattering_strength = 16.0; // how far light can go through the atmosphere 
float light_multiplier = 1.0; // intensity of light
float density_falloff = 3.0;

vec3 wavelengths = vec3(700, 530, 440);
vec3 scattering_coefficients = vec3(pow(400 / wavelengths.r, 4), pow(400 / wavelengths.g, 4), pow(400 / wavelengths.b, 4));

float get_density_atmosphere(vec3 a) {
    float length_a = length(a);
    vec3 norm_a = a / length_a;
    float t = sqrt(1.0 / (pow(norm_a.x / outer_axes.x, 2) + pow(norm_a.y / outer_axes.y, 2) + pow(norm_a.z / outer_axes.z, 2)));
    float dist = t - length_a;
    float fraction = 1.0 - dist / (outer_axes.x - inner_axes.x);
    fraction = clamp(0.0, 1.0, fraction);
    float density = exp(-fraction * density_falloff) * (1 - fraction);

    return density * density_multiplier;
}

float optical_depth(vec3 ray_origin, vec3 ray_direction, float ray_length, float steps) {
    vec3 sample_point = ray_origin;
    float optical_depth = 0.0;

    float step_size = ray_length / steps;
    for(int i = 0; i < steps; ++i) {
        float local_density = get_density_atmosphere(sample_point);
        optical_depth += local_density * step_size;
        sample_point += ray_direction * step_size;
    }

    return optical_depth;
}

void main() {
    vec3 point_pos = gl_in[0].gl_Position.xyz;
    float dist_from_center = length(point_pos);
    if(dist_from_center > inner_axes.x) {
        if(dist_from_center < outer_axes.x) {
            vec3 point = vec3(view * translate_rotate * vec4(point_pos, 1.0));

            vec3 particle_box = points_range / points_count;

            float max_density = 0.0;
            for(int i = 0; i < 8; ++i) {
                int z = i % 2;
                int y = (i - z*4) % 2;
                int x = (i - z*4 - y*2) % 2;

                vec3 sample_point = point_pos - particle_box * 0.25 + particle_box * vec3(x, y, z) * 0.25;
                float s = get_density(sample_point);
                max_density = max(max_density, s);
            }
            float density_at_point = max_density;

            if(density_at_point >= cloud_threshold) {
                //point_pos = round(point_pos * 6) / 6;
                mat4 inv = inverse(translate_rotate);
                mat4 inv_v = inverse(view);

                vec3 rel_player_pos = vec3(inv * vec4(player_pos, 1));
                vec3 rel_light_pos = vec3(inv * vec4(light_pos, 1));
                
                vec3 up = vec3(0.0, 1.0, 0.0);
                vec3 perpendicular = vec3(1.0, 0.0, 0.0);
                
                
                vec3 light_direction = normalize(rel_light_pos - point_pos);

                float atmo_entry;
                float atmo_exit;

                bool intersects_atmosphere = ray_sphere(point_pos, light_direction, outer_axes, atmo_entry, atmo_exit);

                float light_transmittance = 1.0;
                float g = to_float(hash(uvec3(abs(normalize(point_pos) + 1.0) * 7703.0)));
                float light_step_dist = light_step_size * (0.5 + g * 0.5);
                

                if(atmo_exit > 0.0) {
                    float total_density = 0.0;
                    while(light_step_dist < min(atmo_exit, max_light_step_dist)) {
                        light_step_dist += light_step_size;
                        vec3 light_step_pos = point_pos + light_direction * light_step_dist;

                        float density_light = get_density(light_step_pos);

                        total_density += density_light * light_step_size * light_absorbtion;
                    }
                    light_transmittance = beer(total_density);
                }
            
                //float r = phase(dot(direction, light_direction));
                float le = light_transmittance;
                vec3 translation = vec3(translate_rotate[3][0], translate_rotate[3][1], translate_rotate[3][2]);

                float view_ray_od = 0.0;

                float step_size = atmo_exit / steps;
                float light_ray_od = optical_depth(point_pos, light_direction, atmo_exit, light_steps);

                float transmittance = exp(-(light_ray_od));
                
                le *= transmittance;
                vec3 cloud_color = vec3(le);

                //vec3 cloud_color = vec3(le);


                variation = 0;
                color = vec4(cloud_color, density_at_point * particle_size);

                uv = vec2(0.0, 0.0);
                gl_Position = vec4(point - particle_size * perpendicular - particle_size * up, 1.0);
                gl_Position = proj * gl_Position;
                EmitVertex();

                uv = vec2(1.0, 0.0);
                gl_Position = vec4(point + particle_size * perpendicular - particle_size * up, 1.0);
                gl_Position = proj * gl_Position;
                EmitVertex();
                
                uv = vec2(0.0, 1.0);
                gl_Position = vec4(point - particle_size * perpendicular + particle_size * up, 1.0);
                gl_Position = proj * gl_Position;
                EmitVertex();

                uv = vec2(1.0, 1.0);
                gl_Position = vec4(point + particle_size * perpendicular + particle_size * up, 1.0);
                gl_Position = proj * gl_Position;
                EmitVertex();

                EndPrimitive();

                /*for(int i = 0; i < 12; ++i) {
                    gl_Position = proj * view * vec4(point + vertices[indices[i * 3]] * particle_size, 1.0);
                    EmitVertex();

                    gl_Position = proj * view * vec4(point + vertices[indices[i * 3 + 1]] * particle_size, 1.0);
                    EmitVertex();
                    
                    gl_Position = proj * view * vec4(point + vertices[indices[i * 3 + 2]] * particle_size, 1.0);
                    EmitVertex();

                    EndPrimitive();
                }*/
            }
        }
    }
}