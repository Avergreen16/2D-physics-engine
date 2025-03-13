#version 460 core

out vec4 frag_color;

//layout(binding = 1) uniform sampler2D depth_tex;

layout(triangles) in;
layout(triangle_strip, max_vertices = 19) out;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;

layout(location = 0) flat in mat4 inv_proj_i[];
layout(location = 4) flat in mat4 inv_view_i[];
layout(location = 8) flat in mat4 inv_model_i[];
layout(location = 0) flat out mat4 inv_proj;
layout(location = 4) flat out mat4 inv_view;
layout(location = 8) flat out mat4 inv_model;

layout(location = 12) out vec3 normal;

vec3 sun_dir = vec3(0, 0, 1);

void main() {
    inv_proj = inv_proj_i[0];
    inv_view = inv_view_i[0];
    inv_model = inv_model_i[0];

    normal = normalize(cross(gl_in[0].gl_Position.xyz - gl_in[2].gl_Position.xyz, gl_in[1].gl_Position.xyz - gl_in[2].gl_Position.xyz));
    
    
    gl_Position = proj * (view * model * gl_in[0].gl_Position);
    EmitVertex();
    gl_Position = proj * (view * model * gl_in[1].gl_Position);
    EmitVertex();
    gl_Position = proj * (view * model * gl_in[2].gl_Position);
    EmitVertex();

    EndPrimitive();
}