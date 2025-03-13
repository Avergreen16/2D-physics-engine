#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec2 tex_coord[];

layout(location = 0) out vec2 o_tex_coord;
layout(location = 1) out vec3 o_normal;
layout(location = 2) out vec3 o_pos;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 4) uniform mat4 model_1;

void main() {
    o_normal = normalize(cross(gl_in[0].gl_Position.xyz - gl_in[2].gl_Position.xyz, gl_in[1].gl_Position.xyz - gl_in[2].gl_Position.xyz));

    for(int i = 0; i < 3; ++i) {
        vec4 v = view * model_1 * gl_in[i].gl_Position;

        o_tex_coord = tex_coord[i];

        o_pos = v.xyz;

        gl_Position = proj * v;

        EmitVertex();
    }

    EndPrimitive();
}