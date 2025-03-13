#version 460 core

layout(binding = 0) uniform sampler2D accum_tex;
layout(binding = 1) uniform sampler2D reveal_tex;

in vec2 tex_coord;

out vec4 frag_color;

void main() {
    ivec2 coords = ivec2(gl_FragCoord.xy);
    
    vec4 accumulation = texelFetch(accum_tex, coords, 0);
    float revealage = texelFetch(reveal_tex, coords, 0).r;

    vec3 average_color = accumulation.rgb / max(accumulation.a, 0.000001);

    //if(revealage == 0.0) revealage = 1.0;

    frag_color = vec4(average_color, 1.0 - revealage);
}