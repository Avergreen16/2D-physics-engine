#version 460 core

layout(binding = 0) uniform sampler2D tilesheet;
layout(binding = 1) uniform sampler2D planet_tex;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 tex_world;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec2 tex_size;
layout(location = 5) in vec3 apos;
layout(location = 6) in vec3 pos;
layout(location = 7) in float fade;

layout(location = 2) uniform mat4 model;
layout(location = 3) uniform vec3 light_dir;
layout(location = 4) uniform int lod;
layout(location = 8) uniform ivec3 texture_axes;

layout(location = 9) uniform vec3 planet_size;
layout(location = 10) uniform float atmo_size;

float world_to_pixel = 16;

ivec2 texture_size = textureSize(tilesheet, 0);

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_normal;
layout(location = 2) out vec4 slope;

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

float to_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat(m);       // Range [1:2]
    return f - 1.0;                // Range [0:1]
}

vec3 up[] = {
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},
    {-1, 0, 0},
    {0, -1, 0},
    {0, 0, -1}
};

vec3 orientation_0[] = {
    {0, 0, 1},
    {0, 0, 1},
    {-1, 0, 0},
    {0, 0, 1},
    {0, 0, 1},
    {-1, 0, 0}
};

vec3 orientation_1[] = {
    {0, 1, 0},
    {-1, 0, 0},
    {0, 1, 0},
    {0, -1, 0},
    {1, 0, 0},
    {0, -1, 0}
};

ivec4 tex_coords[] = {
    {0, texture_axes.x, texture_axes.y, texture_axes.z},
    {texture_axes.y, texture_axes.x, texture_axes.x, texture_axes.z},
    {0, texture_axes.x + texture_axes.z, texture_axes.y, texture_axes.x},
    {texture_axes.x + texture_axes.y, texture_axes.x, texture_axes.y, texture_axes.z},
    {texture_axes.x + texture_axes.y * 2, texture_axes.x, texture_axes.x, texture_axes.z},
    {0, 0, texture_axes.y, texture_axes.x}
};

float ellipsoid_height(vec3 pos, vec3 radii) {
    return sqrt(1.0f / (pos.x * pos.x / (radii.x * radii.x) + pos.y * pos.y / (radii.y * radii.y) + pos.z * pos.z / (radii.z * radii.z)));
}

void main() {
    float rand = to_float(hash(uvec2(gl_FragCoord.xy)));
    //if(rand > fade) discard;

    vec3 n = normalize(normal);

    vec2 global_tex = tex_world * world_to_pixel / texture_size;
    vec2 x = dFdx(global_tex);
    vec2 y = dFdy(global_tex);

    vec2 tex = (mod(tex_world * world_to_pixel, tex_size) + tex_coord) / texture_size;
    vec4 color = textureGrad(tilesheet, tex, x, y);

    float light = max(0, dot(light_dir, n));

    float shadow_value = 0.1;
    light = light * (1.0 - shadow_value) + shadow_value;

    float dist = length(pos);
    
    if(dist > 512 && color.xyz != vec3(1, 1, 1)) {
        float blend = clamp((dist - 512) / 1024, 0.0, 1.0);
        vec3 n = normalize(apos);
        //n = smart_normalize(n);

        int i = -1;
        float c;
        if(abs(n.x) > abs(n.y)) {
            if(abs(n.x) > abs(n.z)) {
                c = abs(n.x);
                if(n.x > 0) i = 0;
                else i = 3;
            } else {
                c = abs(n.z);
                if(n.z > 0) i = 2;
                else i = 5;
            }
        } else {
            if(abs(n.y) > abs(n.z)) {
                c = abs(n.y);
                if(n.y > 0) i = 1;
                else i = 4;
            } else {
                c = abs(n.z);
                if(n.z > 0) i = 2;
                else i = 5;
            }
        }

        n /= c;

        mat3 remapping = mat3(orientation_1[i], orientation_0[i], up[i]);

        vec3 a = transpose(remapping) * n;

        a = a * 0.5 + 0.5;

        vec2 coordinates = a.xy;

        vec4 tex_region = tex_coords[i];

        vec2 t = tex_region.xy + coordinates * tex_region.zw;

        t /= textureSize(planet_tex, 0);

        vec4 b = texture(planet_tex, t);

        color = mix(color, b, blend);
    }

    vec3 normal_2 = normalize(cross(dFdx(pos), dFdy(pos)));
    float cosine = dot(normal_2, vec3(0, 0, 1));
    if(gl_FrontFacing) {
        if(cosine < 0) cosine = -cosine;
    } else {
        if(cosine > 0) cosine = -cosine;
    }
    
    float light_factor = 0.0f;
    if(atmo_size != 0.0) {
        float length_apos = length(apos);
        vec3 norm_apos = apos / length(apos);
        float eh = ellipsoid_height(norm_apos, planet_size);
        light_factor = length_apos - eh;
        light_factor = clamp(1.0 - light_factor / atmo_size, 0.0, 1.0);
    }

    float min_shadow = 0.01;
    float max_shadow = min_shadow + 0.74 * light_factor;
    

    float d = dot(normalize(mat3(model) * apos), light_dir);
    float wrap = 0.35;
    float d_wrap = clamp((d + wrap) / (1.0 + wrap), 0.0, 1.0);
    d_wrap = d_wrap * (max_shadow - min_shadow) + min_shadow;
    

    slope = vec4(cosine * 0.5 + 0.5, d_wrap, 0, 1);

    frag_color = color;

    vec3 output_normal = n * 0.5 + 0.5;

    frag_normal = vec4(output_normal, 1.0);

    if(frag_color.w < 128.0 / 255) discard;
    else frag_color.w = 1.0;
}