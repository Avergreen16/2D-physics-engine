#version 460 core

layout(location = 6) uniform vec3 outer_axes;
layout(location = 7) uniform uvec3 points_count;
layout(location = 8) uniform vec3 points_origin;
layout(location = 9) uniform vec3 points_range;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint hash(uvec3 v) { 
    return hash(v.x ^ hash(v.y)) ^ hash(v.z); 
}

float to_float(uint m) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat(m);       // Range [1:2]
    return f * 2.0 - 3.0;                // Range [-1:1]
}

vec3 points_sep = points_range / vec3(points_count);

flat out uint vari;

uint variations = 5;

void main() {
    uint z = gl_VertexID / (points_count.y * points_count.x);
    uint y = (gl_VertexID / points_count.y) % points_count.x;
    uint x = gl_VertexID % points_count.x;

    uvec3 id = uvec3(x, y, z);
    vec3 point_pos = points_origin + points_sep * (vec3(id) + 0.5);
    uint h = hash(uvec3((point_pos + outer_axes) / outer_axes * 8623));
    uint hh = hash(h);
    vec3 jitter_pos = vec3(to_float(h), to_float(hh), to_float(hash(hh))) * 0.5 * points_sep;
    vari = uint(floor((to_float(h) * 0.5 + 0.5) * variations));

    point_pos += jitter_pos;
    gl_Position = vec4(point_pos, 1.0);
}