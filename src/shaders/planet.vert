#version 460 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 tex;
layout(location = 2) in vec3 norm;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view; 
layout(location = 2) uniform mat4 model;
layout(location = 3) uniform mat3 rotate;
layout(location = 4) uniform bool shadow;
layout(location = 5) uniform vec3 light_dir;

out vec2 frag_tex;
out float light;

void main() {
    frag_tex = tex;
    
    vec3 rot_norm = rotate * norm;
    if(!shadow) {
        light = 1;
    } else {
        light = min(1.0, max(0.05, dot(rot_norm, light_dir) + 0.3));
    }

    gl_Position = view * model * vec4(rotate * pos, 1.0);
    gl_Position = proj * gl_Position;
}