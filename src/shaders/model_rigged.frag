#version 460 core

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 tex_coord;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 pos;

layout(location = 3) uniform vec3 light_dir;
layout(location = 5) uniform float darkness_value;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_normal;
layout(location = 2) out vec4 slope;

void main() {
    float light = max(dot(light_dir, normal), 0.0) * (1 - darkness_value) + darkness_value;
    vec4 color = texture(tex, tex_coord);

    frag_color = vec4(color.xyz, color.w);

    vec3 normal_2 = normalize(cross(dFdx(pos), dFdy(pos)));
    float cosine = dot(normal_2, vec3(0, 0, 1));
    if(gl_FrontFacing) {
        if(cosine < 0) cosine = -cosine;
    } else {
        if(cosine > 0) cosine = -cosine;
    }

    slope = vec4(cosine * 0.5 + 0.5, darkness_value, 0, 1);

    vec3 output_normal = normal * 0.5 + 0.5;

    frag_normal = vec4(output_normal, 1.0);

    if(frag_color.w == 0.0) discard;
}