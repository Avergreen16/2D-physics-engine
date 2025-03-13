#version 460 core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_world;
layout(location = 3) in vec2 tex_coord;
layout(location = 4) in vec2 tex_size;
layout(location = 5) in vec4 wind_data;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;
layout(location = 4) uniform int lod;
layout(location = 5) uniform vec3 apos;
layout(location = 7) uniform float time;

layout(location = 0) out vec3 o_normal;
layout(location = 1) out vec2 o_tex_world;
layout(location = 2) out vec2 o_tex_coord;
layout(location = 3) out vec2 o_tex_size;
layout(location = 5) out vec3 o_apos;
layout(location = 6) out vec3 o_pos;
layout(location = 7) out float o_fade;

void main() {
    vec3 w_pos = pos;
    /*if(wind_data.w != 0) {
        vec3 up = wind_data.xyz;

        vec3 x = cross(up, vec3(0, 0, 1));
        if(abs(length(x)) < 0.01) x = cross(up, vec3(0, 1, 0));
        x = normalize(x);

        vec3 y = normalize(cross(up, x));

        vec3 wind_movement = (x * sin((time + (w_pos.x + w_pos.z) * (2 * 3.14159 / 16)) * 2) + y * cos((time + (w_pos.y + w_pos.x) * (2 * 3.14159 / 16)) * 2)) * (wind_data.w * 0.15);

        w_pos += wind_movement;
    }*/

    o_normal = mat3(model) * normal;
    o_tex_world = tex_world;
    o_tex_coord = tex_coord;
    o_tex_size = tex_size;
    o_apos = apos + w_pos;

    gl_Position = view * model * vec4(w_pos, 1.0);
    o_pos = gl_Position.xyz;

    if(lod != 0) {
        float rad_a = pow(2, lod) * 16;
        float rad_b = pow(2, lod - 1) * 16;
        float len = length(o_pos);

        o_fade = smoothstep(rad_b, rad_a, len);
    } else o_fade = 1;

    gl_Position = proj * gl_Position;
}