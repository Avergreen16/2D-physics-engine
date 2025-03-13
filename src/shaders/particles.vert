#version 460 core

struct Particle_data {
    vec3 position;
    vec4 color;
    vec2 size;
    vec2 tex_coord;
    vec2 tex_size;
    vec3 orient_center;
    uint settings;
};

layout(std140, binding = 0) buffer Particle_buffer {
    Particle_data particle_data[];
};

vec2 vertices[6] = {
    vec2(-0.5, -0.5),
    vec2(0.5, -0.5),
    vec2(-0.5, 0.5),
    vec2(-0.5, 0.5),
    vec2(0.5, -0.5),
    vec2(0.5, 0.5)
};

layout(location = 0) uniform mat4 view_mat;
layout(location = 1) uniform mat4 proj_mat;
layout(location = 2) uniform vec3 particle_up;
layout(location = 3) uniform ivec2 screen_size;

out vec4 color;
out vec2 tex_coord;

float far_plane = pow(2, 37) * 1000000;

mat3 inv = inverse(mat3(view_mat));

void main() {
    Particle_data current_data = particle_data[gl_VertexID / 6];
    vec2 position = vertices[gl_VertexID % 6];

    color = current_data.color;
    tex_coord = current_data.tex_coord + current_data.tex_size * (position + vec2(0.5, 0.5));

    vec4 view_pos = view_mat * vec4(current_data.position, 1.0);

    position *= current_data.size;

    vec3 y = vec3(0, 1, 0);
    vec3 x = vec3(1, 0, 0);
    float fade_factor = 1;
    /*vec3 y = vec3(0, 1, 0);
    vec3 dir = vec3(0, 0, 1);
    float fade_factor = 1;
    if((current_data.settings & 0x2) == 0x2) {
        y = particle_up;
    }*/
    if((current_data.settings & 0x8) == 0x8) {
        fade_factor = clamp(-view_pos.z / (max(current_data.size.x, current_data.size.y) * 0.5), 0, 1);
    }

    /*vec3 x = normalize(cross(y, dir));
    y = normalize(y - dir * dot(y, dir));*/

    color.a *= fade_factor;

    /*if(current_data.is_oriented) {
        vec4 proj_center = proj_mat * view_mat * vec4(current_data.orient_center, 1.0);
        vec4 proj_pos = proj_mat * view_pos;

        vec4 new_center = vec4(proj_center.xy / proj_center.w * proj_pos.w, proj_pos.z, proj_pos.w);
        
        mat4 inverse_proj = inverse(proj_mat);
        vec4 view_center = inverse_proj * new_center;

        vec2 diff = view_center.xy - view_pos.xy;
        float hypot = sqrt(diff.x * diff.x + diff.y * diff.y);
        float sine = diff.y / hypot;
        float cosine = diff.x / hypot;
        rot_mat = mat2(cosine, sine, -sine, cosine);
    }*/

    if((current_data.settings & 0x10) == 0x10) {
        gl_Position = proj_mat * view_pos;
        gl_Position += vec4(position.x / (screen_size.x * 0.5) * gl_Position.w, position.y / (screen_size.y * 0.5) * gl_Position.w, 0.0, 0.0);
    } else if((current_data.settings & 0x20) == 0x20) {
        vec3 down = mat3(view_mat) * current_data.orient_center;
        down = normalize(vec3(down.x, down.y, 0.0f));
        vec3 a = vec3(-down.y, down.x, 0.0f);
        vec3 b = -down;

        gl_Position = proj_mat * (view_pos + vec4(-position.x * a - position.y * b, 0.0));
    } else {
        gl_Position = proj_mat * (view_pos + vec4(position.x, position.y, 0.0, 0.0));   
    }
}