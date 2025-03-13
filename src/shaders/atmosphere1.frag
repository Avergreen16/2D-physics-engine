#version 460 core

layout(binding = 0) uniform sampler2D depth_tex;
layout(binding = 1) uniform sampler2D color_tex;

layout(location = 5) uniform vec3 player_pos;
layout(location = 6) uniform vec2 screen_size;
layout(location = 7) uniform vec3 sun_dir;

layout(location = 8) uniform vec3 color;
layout(location = 9) uniform vec3 sunset_color;
layout(location = 10) uniform vec3 bloom_color;
layout(location = 11) uniform float density;
layout(location = 12) uniform mat3 rot_mat;

in vec3 frag_pos;
flat in vec3 r;
flat in vec3 r_planet;
flat in vec3 center_pos;
flat in mat4 inv_proj;

out vec4 frag_color;

float max_r = max(max(r.x, r.y), r.z);
float max_r_planet = max(max(r_planet.x, r_planet.y), r_planet.z);
float max_chord_length = sqrt(max_r * max_r - max_r_planet * max_r_planet);

void main() {
    vec3 frag_dir = rot_mat * normalize(frag_pos - player_pos);
    vec3 rot_sun_dir = rot_mat * sun_dir;
    vec3 axes = r;
    vec3 dir = frag_dir;
    vec3 r_origin = rot_mat * (player_pos - center_pos);

    vec3 a = -r_origin;
    float t = dot(frag_dir, a);
    vec3 p = r_origin + t * frag_dir;

    vec3 normal = normalize(p);
    
    float max_p = max(max(abs(p.x), abs(p.y)), abs(p.z));
    float dist_from_surface = max_p - axes.x;

    float atmo_height = r.x - r_planet.x;
    float alpha = 0;
    if(dist_from_surface < atmo_height) alpha = 1;

    //float alpha = 1.0 - dist_from_surface / atmo_height * density;

    float dot_sun = dot(rot_sun_dir, normal) * 0.65 + 0.35;

    /*
    vec3 global_up = normalize(player_pos - center_pos);


    float t = dot(center_pos - player_pos, dir);
    vec3 p = player_pos + dir * t;
    float y;
    float d = length(center_pos - p);
    if(t > 0) y = d;
    else {
        y = length(center_pos - player_pos);
        t = 0;
    }

    float x = sqrt(r * r - y * y);

    float t1 = max(t - x, 0.0);
    float t2 = t + x;

    float diff = t2 - t1;

    float alpha = 0;

    alpha = diff / max_chord_length;*/

    //alpha *= density;

    /*vec3 global_up = normalize(player_pos - center_pos);

    vec3 dir = normalize(frag_pos - player_pos);

    float t = dot(center_pos - player_pos, dir);
    vec3 p = player_pos + dir * t;
    float y;
    float d = length(center_pos - p);
    if(t > 0) y = d;
    else {
        y = length(center_pos - player_pos);
        t = 0;
    }

    vec2 uv = gl_FragCoord.xy / screen_size;
    float depth = 1.0;//texture(depth_tex, uv).x;
    vec3 ndc = vec3(uv, depth) * 2.0 - 1.0;
    vec4 view_pos = inv_proj * vec4(ndc, 1.0);
    vec3 pers_div = view_pos.xyz / view_pos.w;
    float max_depth = length(pers_div.xyz);

    float atmo_depth = length(frag_pos - player_pos);

    float alpha = 0;

    float x = sqrt(r * r - y * y);

    float t1 = max(t - x, 0.0);
    float t2 = min(t + x, max_depth);

    float diff = t2 - t1;
    
    
    if(max_depth > atmo_depth) alpha = clamp(1.0 - (y - r_planet) / thickness, 0.0, 1.0);
    else {
        alpha = diff / max_chord_length;
    }

    alpha *= density;

    vec3 up = normalize((player_pos + dir * min(t, t2) - center_pos));
    float dot_sun = dot(sun_dir, up) * 0.7 + 0.3;
    alpha = smoothstep(0.0, 1.0, mix(0, alpha, dot_sun));


    float sun_bloom = 0.0;

    if(max_depth > atmo_depth) {
        float specular = pow((max(0.0, dot(dir, sun_dir)) + 0.5) / 1.5, 128);
        float fresnel = 1.0 - clamp(dot(-dir, normalize(center_pos - p)), 0.0, 1.0);
        fresnel *= fresnel;

        sun_bloom = (y - r_planet) / (thickness * 3.0);
        sun_bloom = 1.0 - clamp(sun_bloom, 0.0, 1.0);
        sun_bloom = pow(sun_bloom, 3);
        sun_bloom *= specular * fresnel;   
    }
    sun_bloom *= density;

    float blend = 0.0;
    if(alpha != 0.0) blend = dot(dir, sun_dir) * (1.0 - dot(dir, up));*/

    //frag_color = vec4((texture(color_tex, uv).xyz * (1 - alpha) + mix(color, sunset_color, blend) * alpha) * (1 - sun_bloom) + bloom_color * sun_bloom, 1.0);
    //frag_color = vec4(mix(mix(color, sunset_color, blend), bloom_color, sun_bloom), min((alpha + sun_bloom) * density, density));

    frag_color = vec4(color, min(alpha, density));
}