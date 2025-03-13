#version 460 core

const vec3 positions[16] = {
    vec3(-0.5, 0, 0),
    vec3(0.5, 0, 0),
    vec3(-0.5, 0, 1),
    vec3(0.5, 0, 1),
    
    vec3(0.5, 0, 0),
    vec3(-0.5, 0, 0),
    vec3(0.5, 0, 1),
    vec3(-0.5, 0, 1),

    vec3(0, -0.5, 0),
    vec3(0, 0.5, 0),
    vec3(0, -0.5, 1),
    vec3(0, 0.5, 1),
    
    vec3(0, 0.5, 0),
    vec3(0, -0.5, 0),
    vec3(0, 0.5, 1),
    vec3(0, -0.5, 1)
};

const vec2 texture_positions[16] = {
    vec2(0, 0),
    vec2(1, 0),
    vec2(0, 1),
    vec2(1, 1),
    
    vec2(1, 0),
    vec2(0, 0),
    vec2(1, 1),
    vec2(0, 1),

    vec2(0, 0),
    vec2(1, 0),
    vec2(0, 1),
    vec2(1, 1),
    
    vec2(1, 0),
    vec2(0, 0),
    vec2(1, 1),
    vec2(0, 1)
};

/*const uint variations = 4;

const float probabilities[variations] = {
    0.2,
    0.15,
    0.15,
    0.5,
};

const vec2 texture_origins[variations] = {
    vec2(0, 0),
    vec2(16, 0),
    vec2(48, 0),
    vec2(64, 0)
};

const vec2 texture_sizes[variations] = {
    vec2(16, 16),
    vec2(16, 16),
    vec2(7, 6),
    vec2(32, 32)
};*/

const uint variations = 6;

const float probabilities[variations] = {
    0.48,
    0.38,
    0.09,
    0.02, 
    0.02,
    0.01
};

const vec2 texture_origins[variations] = {
    vec2(0, 0),
    vec2(16, 0),
    vec2(48, 0),
    vec2(96, 0),
    vec2(112, 0),
    vec2(64, 0)
};

const vec2 texture_sizes[variations] = {
    vec2(16, 16),
    vec2(16, 16),
    vec2(7, 6),
    vec2(16, 16),
    vec2(16, 16),
    vec2(32, 32)
};

layout(triangles) in;
layout(triangle_strip, max_vertices = 19) out;

layout(location = 0) uniform mat4 proj;
layout(location = 1) uniform mat4 view;
layout(location = 2) uniform mat4 model;
layout(location = 3) uniform int level;

layout(location = 0) in vec3 normal[];
layout(location = 1) in vec2 tex_world[];
layout(location = 2) in vec2 tex_coord[];
layout(location = 3) in vec2 tex_size[];
layout(location = 4) in vec3 color[];

layout(location = 0) out vec3 o_normal;
layout(location = 1) out vec2 o_tex_world;
layout(location = 2) out vec2 o_tex_coord;
layout(location = 3) out vec2 o_tex_size;
layout(location = 4) out vec3 o_color;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint hash(uvec2 v) { 
    return hash(v.x ^ hash(v.y)); 
}

uint hash(uvec3 v) { 
    return hash(v.x ^ hash(v.y)) ^ hash(v.z); 
}

uint hash(uvec4 v) {
    return hash(v.x ^ hash(v.y)) ^ hash(v.z) ^ hash(v.w); 
}

float to_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat(m);       // Range [1:2]
    return f * 2.0 - 3.0;                // Range [-1:1]
}

mat3 create_z_rot(float angle) {
    vec3 x = vec3(cos(angle), sin(angle), 0);
    vec3 y = vec3(-sin(angle), cos(angle), 0);
    vec3 z = vec3(0, 0, 1);
    return mat3(x, y, z);
}

float pi = radians(180);

void main() {
    o_color = color[0];

    o_normal = normal[0];
    o_tex_world = tex_world[0];
    o_tex_coord = tex_coord[0];
    o_tex_size = tex_size[0];
    gl_Position = proj * view * model * gl_in[0].gl_Position;
    EmitVertex();

    o_normal = normal[1];
    o_tex_world = tex_world[1];
    o_tex_coord = tex_coord[1];
    o_tex_size = tex_size[1];
    gl_Position = proj * view * model * gl_in[1].gl_Position;
    EmitVertex();

    o_normal = normal[2];
    o_tex_world = tex_world[2];
    o_tex_coord = tex_coord[2];
    o_tex_size = tex_size[2];
    gl_Position = proj * view * model * gl_in[2].gl_Position;
    EmitVertex();

    EndPrimitive();

    if(level == 12) {
        bool pass = false;

        vec3 center = (gl_in[0].gl_Position.xyz + gl_in[1].gl_Position.xyz + gl_in[2].gl_Position.xyz) / 3;
        float random_number = to_float(hash(uvec3(center * 409.6))) * 0.5 + 0.5;
        
        if(tex_coord[0].x == 0 && tex_coord[0].y == 16) {
            if(random_number < 0.4) pass = true;
        } else if(tex_coord[0].x == 16 && tex_coord[0].y == 16) {
            if(random_number < 0.0001) pass = true;
        }

        if(pass) {
            vec3 z = normal[0];
            vec3 x;
            vec3 y;
            if(abs(dot(z, vec3(0, 0, 1))) > 0.99) {
                x = normalize(cross(z, vec3(0, 1, 0)));
                y = cross(z, x);
            } else {
                x = normalize(cross(z, vec3(0, 0, 1)));
                y = cross(z, x);
            }
            mat3 grass_model = mat3(x, y, z);

            float angle = to_float(hash(uvec3(center * 128))) * pi;
            mat3 rot_matrix = create_z_rot(angle);

            vec2 a_tex_coord;
            vec2 a_tex_size;
            mat3 full_matrix = grass_model * rot_matrix;
            if(tex_coord[0].x == 16 && tex_coord[0].y == 16) {
                a_tex_coord = vec2(32, 0);
                a_tex_size = vec2(16, 16);
            } else {
                float variation_random = to_float(hash(uvec3((vec3(600.5) - center) * 553.6)));
                variation_random = variation_random * 0.5 + 0.5;
                uint tex_variation = variations - 1;

                for(int i = 0; i < variations; ++i) {
                    if(variation_random > 0.0) {
                        variation_random -= probabilities[i];
                        if(variation_random <= 0.0) {
                            tex_variation = i;
                        }
                    }
                }

                a_tex_coord = texture_origins[tex_variation]; 
                a_tex_size = texture_sizes[tex_variation];
            }

            for(int i = 0; i < 4; ++i) {
                vec3 n = full_matrix * normalize(cross(positions[i * 4] - positions[i * 4 + 2], positions[i - 4 + 1] - positions[i - 4 + 2]));
                o_normal = n;
                o_tex_world = texture_positions[i * 4] * a_tex_size / 16;
                o_tex_coord = a_tex_coord;
                o_tex_size = a_tex_size;
                gl_Position = proj * view * model * vec4(center + full_matrix * (positions[i * 4] * vec3(a_tex_size.x, a_tex_size.x, a_tex_size.y) / 16), 1.0);
                EmitVertex();

                
                o_normal = n;
                o_tex_world = texture_positions[i * 4 + 1] * a_tex_size / 16;
                o_tex_coord = a_tex_coord;
                o_tex_size = a_tex_size;
                gl_Position = proj * view * model * vec4(center + full_matrix * (positions[i * 4 + 1] * vec3(a_tex_size.x, a_tex_size.x, a_tex_size.y) / 16), 1.0);
                EmitVertex();
                
                
                o_normal = n;
                o_tex_world = texture_positions[i * 4 + 2] * a_tex_size / 16;
                o_tex_coord = a_tex_coord;
                o_tex_size = a_tex_size;
                gl_Position = proj * view * model * vec4(center + full_matrix * (positions[i * 4 + 2] * vec3(a_tex_size.x, a_tex_size.x, a_tex_size.y) / 16), 1.0);
                EmitVertex();

                o_normal = n;
                o_tex_world = texture_positions[i * 4 + 3] * a_tex_size / 16;
                o_tex_coord = a_tex_coord;
                o_tex_size = a_tex_size;
                gl_Position = proj * view * model * vec4(center + full_matrix * (positions[i * 4 + 3] * vec3(a_tex_size.x, a_tex_size.x, a_tex_size.y) / 16), 1.0);
                EmitVertex();

                EndPrimitive();
            }
        }
    }
}