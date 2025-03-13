// tessellation evaluation shader
#version 460 core

layout (quads) in;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;
//layout(location = 3) uniform vec3 player_pos;
layout(location = 3) uniform vec3 axes;

layout(location = 0) flat out mat4 inv_proj;
layout(location = 4) flat out mat4 inv_view;
layout(location = 8) flat out mat4 inv_model;

void main()
{
    // get patch coordinate
    float u = gl_TessCoord[0];
    float v = gl_TessCoord[1];

    // ----------------------------------------------------------------------
    // retrieve control point position coordinates
    vec3 p[] = {
        gl_in[0].gl_Position.xyz,
        gl_in[1].gl_Position.xyz,
        gl_in[2].gl_Position.xyz,
        gl_in[3].gl_Position.xyz
    };

    // compute patch surface normal

    // bilinearly interpolate position coordinate across patch
    vec3 accum = vec3(0.0);
    accum = mix(mix(p[0], p[1], u), mix(p[2], p[3], u), v);
    accum = normalize(accum) * axes;

    inv_proj = inverse(proj);
    inv_view = inverse(view);
    inv_model = inverse(model);

    gl_Position = vec4(accum, 1.0);
}