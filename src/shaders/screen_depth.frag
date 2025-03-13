#version 460 core

layout(binding = 0) uniform sampler2D color_tex;
layout(binding = 1) uniform sampler2D depth_tex;
layout(binding = 2) uniform sampler2D normal_tex;
layout(binding = 3) uniform sampler2D slope_tex;
layout(binding = 4) uniform sampler2D sun_tex;
layout(binding = 5) uniform sampler2D sun_slope_tex;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;

layout(location = 2) uniform mat4 sun_proj;
layout(location = 3) uniform mat4 sun_view;
layout(location = 4) uniform vec3 rel_sun_pos;
layout(location = 5) uniform vec3 light_pos;

layout(location = 6) uniform vec3 sample_points[64];

in vec2 tex_coord;
in vec3 light_dir;

out vec4 frag_color;

mat4 inverse_proj = inverse(proj);
mat4 inverse_view = inverse(view);
mat4 inverse_sun_proj = inverse(sun_proj);

mat3 rotate_z_to(vec3 b) {
    vec3 z = b;
    vec3 x = normalize(cross(z, vec3(0, 0, 1)));
    vec3 y = normalize(cross(z, x));

    return mat3(x, y, z);
}

float get_depth(float d, mat4 inv_p) {
    //d = (1 - d) * 2 - 1;
    vec4 dv = vec4(0.0, 0.0, d, 1.0);

    dv = inv_p * dv;
    dv /= dv.w;
    float depth = dv.z;

    if(isinf(depth)) depth = 3.402823E+38;

    return depth;
}

vec3 get_world_pos(vec3 clip_space) {
    clip_space.x = clip_space.x * 2 - 1;
    clip_space.y = clip_space.y * 2 - 1;

    vec4 world_pos = inverse_proj * (vec4(clip_space, 1.0));
    world_pos /= world_pos.w;
    world_pos = inverse_view * vec4(world_pos.xyz, 1.0);
    
    if(isinf(world_pos.z)) world_pos.z = 3.402823E+38;

    return world_pos.xyz;
}

float get_slope(float cosine) {
    float sine = sqrt(1 - cosine * cosine);

    float slope = sine / cosine;

    return clamp(slope, -1, 1);
}



int range = 3;

float bias = 0.05;
float slope_bias = 0.02;
float distance_bias = 10;

float sun_pixel_size = 512.0 / 4096 * sqrt(2.0) * 2;

void main() {
    ivec2 size = textureSize(depth_tex, 0);
    ivec2 texel = ivec2(size * tex_coord);

    vec4 color = texelFetch(depth_tex, texel, 0);

    vec3 view_dir = mat3(inverse_view) * vec3(0, 0, 1);

    vec3 normal = texelFetch(normal_tex, texel, 0).xyz * 2.0 - 1.0;
    bool zero_normal = (normal == vec3(-1, -1, -1));
    normal = normalize(normal);

    vec4 l_data = texelFetch(slope_tex, texel, 0);

    float slope = get_slope(l_data.r * 2 - 1);

    float d = color.r;
    vec3 clip_space = vec3((vec2(texel) + 0.5) / size, d);

    vec3 world_pos = get_world_pos(clip_space);

    vec3 sun_dir = normalize(rel_sun_pos - world_pos);
    vec3 light_dir = normalize(light_pos - world_pos);

    vec4 sun_view_space = sun_view * vec4(world_pos - rel_sun_pos, 1.0);
    vec4 sun_clip_space = sun_proj * sun_view_space;
    sun_clip_space /= sun_clip_space.w;
    sun_clip_space = sun_clip_space * 0.5 + 0.5;
    
    float n = 1.0;

    if(!zero_normal && floor(sun_clip_space.xyz) == vec3(0.0) && sun_view_space.z > -511 && sun_view_space.z < 512) {
        float sun_slope = get_slope(texelFetch(sun_slope_tex, ivec2(sun_clip_space.xy * 4096), 0).r * 2 - 1);
        sun_view_space.z = max(sun_view_space.z, -511);

        float offset = bias + slope_bias * slope + sun_pixel_size * sun_slope;

        float sun_depth = texelFetch(sun_tex, ivec2(sun_clip_space.xy * 4096), 0).r;
        float n_sun_depth = get_depth(sun_depth, inverse_sun_proj);

        //offset += distance_bias * pow(2, log2(abs(sun_view_space.z)) - 23);

        if(sun_view_space.z + offset < n_sun_depth) n = l_data.g;
        else n = max(0, dot(normal, light_dir)) * (1 - l_data.g) + l_data.g;
    } else if(!zero_normal) {
        n = max(0, dot(normal, light_dir)) * (1 - l_data.g) + l_data.g;
    }

    float occlusion = 0;

    mat3 r = rotate_z_to(normal);

    float radius = 0.2;
    float bias = 0.03;

    float depth = get_depth(d, inverse_proj);

    // occlusion
    /*if(depth > -500 && d != 0.0) {
        for(int i = 0; i < 64; ++i) {
            vec3 v = sample_points[i];
            v *= radius;

            v = r * v;

            v += world_pos;

            vec4 screen_space = proj * view * vec4(v, 1.0);
            screen_space /= screen_space.w;


            ivec2 texel_2 = ivec2((screen_space.xy * 0.5 + 0.5) * size);

            float depth_2 = texelFetch(depth_tex, texel_2, 0).r;

            if(depth_2 != 0.0) {
                depth_2 = get_depth(depth_2, inverse_proj);

                float range_check = smoothstep(0.0, 1.0, radius / abs(depth_2 - depth));
                occlusion += (depth_2 >= depth + bias ? 1.0 : 0.0) * range_check;
            }
        }

        n = min(n, 1.0 - ((occlusion / 64) * (1.0 - l_data.g)));
    }*/

    /*if(floor(world_pos) == vec3(0, 0, 0)) {
        float shadow = 0.0;

        vec2 texel_size = 1.0 / textureSize(sun_tex, 0);
        for(int x = -range; x <= range; ++x) {
            for(int y = -range; y <= range; ++y) {
                float depth_sun = texture(sun_tex, world_pos.xy + texel_size * (vec2(x, y) + random_points[(x + range) * (y + range)])).r;
                shadow += int((world_pos.z > depth_sun) ? 1.0 : 0.0);
            }
        } 
        shadow /= (range * 2 + 1) * (range * 2 + 1);
        shadow = (shadow - 0.5) * 2;
        n = min(1.0, 1.0 - (shadow * 0.5 * (1 - max(max(abs(world_pos.x * 2 - 1), abs(world_pos.y * 2 - 1)), abs(world_pos.z * 2 - 1)))));
        //
    }*/

    /*
    float n = 1.0;
    if(floor(world_pos.xyz) == vec3(0, 0, 0)) {
        int count = 0;
        float accum = 0;
        for(int x = -range; x <= range; ++x) {
            for(int y = -range; y <= range; ++y) {
                ivec2 current_texel = texel + ivec2(x, y);

                if(current_texel.x >= 0 && current_texel.x < size.x && current_texel.y >= 0 && current_texel.y < size.y) {
                    ++count;

                    vec4 color = texelFetch(depth_tex, current_texel, 0);
                    vec3 clip_space = vec3(vec2(current_texel) / size, color.r);
                    vec3 sun_world_pos = get_sun_pos(clip_space);

                    if(floor(sun_world_pos) == vec3(0, 0, 0)) {
                        float depth_sun = texture(sun_tex, sun_world_pos.xy).r;
                        if(sun_world_pos.z < depth_sun) {
                            ++accum;
                        }
                    }
                }
            }
        }

        if(count != 0) {
            n = (accum / count) * 0.5 + 0.5;
        }
    }*/

    vec4 tex = texture(color_tex, tex_coord);

    frag_color = vec4(tex.xyz * n, tex.w);
}