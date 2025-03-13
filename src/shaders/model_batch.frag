#version 460 core

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 tex_coord;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 pos;

layout(location = 2) uniform vec3 light_dir;
layout(location = 3) uniform vec3 apos;
layout(location = 4) uniform float darkness_value;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_normal;
layout(location = 2) out vec4 slope;

void main() {
    float d = max(darkness_value, 0.1);
    float light = max(dot(light_dir, normal), 0.0) * (1 - d) + d;
    vec4 color = texture(tex, tex_coord);

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

    //if(frag_color.w == 0.0) discard;
    if(frag_color.w < 128.0 / 255) discard;
    else frag_color.w = 1.0;
}