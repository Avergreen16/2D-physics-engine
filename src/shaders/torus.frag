#version 460 core

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec3 planet_pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 pos;

layout(location = 3) uniform vec3 light_dir;
layout(location = 4) uniform vec3 apos;
layout(location = 5) uniform float darkness_value;
layout(location = 6) uniform vec3 radii;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_normal;
layout(location = 2) out vec4 slope;

float m_pi = 3.14159265358979;

float mod_f(float f, float n) {
    return f - floor(f / n) * n;
}

uint num_segments = 8;

float outer_radius = radii.x;

float center = outer_radius - radii.y;

void main() {
    float d = max(darkness_value, 0.1);
    float light = max(dot(light_dir, normal), 0.0) * (1 - d) + d;
    //vec4 color = texture(tex, tex_coord);
    ivec2 tex_size = textureSize(tex, 0);

    vec4 color = vec4(0.5, 0.5, 0.5, 1.0);
    // texturing
    vec2 xy = planet_pos.xy;
    xy = normalize(xy);
    float angle_i = atan(xy.y, xy.x);
    vec3 dir = vec3(cos(angle_i), sin(angle_i), 0);
    vec3 ring_pos = planet_pos - dir * center;
    vec2 ring_pos_2 = vec2(dot(dir, ring_pos), ring_pos.z);
    vec2 norm_rp2 = normalize(ring_pos_2 / vec2(radii.xy));
    float angle_j = atan(norm_rp2.y, norm_rp2.x);
    float x = mod_f(angle_i / (m_pi * 2), 1);
    float y = mod_f(angle_j / (m_pi * 2), 1);

    vec2 coords = vec2(x, y) * vec2(tex_size);

    float y_angle_a = floor(coords.y);
    y_angle_a /= tex_size.y;
    y_angle_a *= m_pi * 2;
    float radius_pixel_a = center + cos(y_angle_a) * radii.y;
    radius_pixel_a /= outer_radius;
    radius_pixel_a *= tex_size.x;
    radius_pixel_a = floor(radius_pixel_a / (num_segments * 2)) * (num_segments * 2);
    radius_pixel_a /= tex_size.x;
    
    float y_angle_b = floor(coords.y) + 1;
    y_angle_b /= tex_size.y;
    y_angle_b *= m_pi * 2;
    float radius_pixel_b = center + cos(y_angle_b) * radii.y;
    radius_pixel_b /= outer_radius;
    radius_pixel_b *= tex_size.x;
    radius_pixel_b = floor(radius_pixel_b / (num_segments * 2)) * (num_segments * 2);
    radius_pixel_b /= tex_size.x;

    float y_r = fract(coords.y);

    float radius_pixel = mix(radius_pixel_a, radius_pixel_b, y_r);


    uint segment = uint(floor(x * num_segments));

    float offset = float(segment) / num_segments;
    x -= offset;
    x *= num_segments;

    x -= 0.5;

    float f = radius_pixel;

    x *= f;

    x += 0.5;

    x /= num_segments;
    x += offset;



    color = texture(tex, vec2(x, y));




    frag_color = vec4(color.xyz, color.w);

    vec3 normal_2 = normalize(cross(dFdx(pos), dFdy(pos)));
    float cosine = dot(normal_2, vec3(0, 0, -1));
    if(gl_FrontFacing) {
        if(cosine < 0) cosine = -cosine;
    } else {
        if(cosine > 0) cosine = -cosine;
    }

    float min_shadow = 0.01;
    float max_shadow = 0.75;
    
    float d2 = dot(normalize(apos), light_dir);
    float wrap = 0.35;
    float d2_wrap;
    
    if(apos == vec3(0.0) || light_dir == vec3(0.0)) d2_wrap = 1.0;
    else {
        d2_wrap = clamp((d2 + wrap) / (1.0 + wrap), 0.0, 1.0);
        d2_wrap = d2_wrap * (max_shadow - min_shadow) + min_shadow;
    }

    slope = vec4(cosine * 0.5 + 0.5, d2_wrap, 0, 1);

    vec3 output_normal = normal * 0.5 + 0.5;

    if(darkness_value == 1.0) frag_normal = vec4(0, 0, 0, 1.0);
    else frag_normal = vec4(output_normal, 1.0);

    if(frag_color.w <= 2.0/255) discard;
    frag_color.w = 1.0;
    //if(frag_color.w < 128.0 / 255) discard;
    //else frag_color.w = 1.0;
}