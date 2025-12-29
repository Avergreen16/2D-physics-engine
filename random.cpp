#include "random.hpp"

#include <bit>

xsimd::batch_bool<int> bfloat_to_bint(xsimd::batch_bool<float> f) {
    xsimd::batch<float> i1 = xsimd::bitwise_cast<xsimd::batch<float>>(f);
    xsimd::batch<int> i2 = xsimd::bitwise_cast<int>(i1);

    return i2 != 0;
}

xsimd::batch_bool<float> bint_to_bfloat(xsimd::batch_bool<int> f) {
    xsimd::batch<int> i1 = xsimd::bitwise_cast<xsimd::batch<int>>(f);
    xsimd::batch<float> i2 = xsimd::bitwise_cast<float>(i1);

    return i2 != 0.0f;
}

simd_vec2 select(xsimd::batch_bool<float>& batch_bool, simd_vec2 a, simd_vec2 b) {
    simd_vec2 ret;
    ret.x = xsimd::select(batch_bool, a.x, b.x);
    ret.y = xsimd::select(batch_bool, a.y, b.y);

    return ret;
}

uint32_t hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t hash(glm::uvec2 v) { 
    return hash(v.x ^ hash(v.y));
}

uint32_t hash(glm::uvec3 v) { 
    return hash(v.x ^ hash(v.y)) ^ hash(v.z); 
}

uint32_t hash(glm::uvec4 v) {
    return hash(v.x ^ hash(v.y)) ^ hash(v.z) ^ hash(v.w); 
}

float to_float(uint32_t m) {
    const uint32_t ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint32_t ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float f = std::bit_cast<float, uint32_t>(m);       // Range [1:2]
    return f * 2.0f - 3.0f;                // Range [-1:1]
}

float to_float_10(uint32_t m) {
    const uint32_t ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint32_t ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = std::bit_cast<float, uint32_t>(m);       // Range [1:2]
    return f - 1.0f;                // Range [0, 1]
}

uint64_t hash(uint64_t x) {
    x += 1;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccd;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53;
    x ^= x >> 33;

    return x;
}

uint64_t hash(vec<2, uint64_t> v) {
    return hash(v.x ^ hash(v.y));
}

uint64_t hash(vec<3, uint64_t> v) {
    return hash(v.x ^ hash(v.y)) ^ hash(v.z);
}

uint64_t hash(vec<4, uint64_t> v) {
    return hash(v.x ^ hash(v.y)) ^ hash(v.z) ^ hash(v.w); 
}

Random32::Random32(uint32_t init_seed) {
    seed = init_seed;
    value = seed;
}

float Random32::operator()() {
    value = hash(value);

    return to_float(value);
}

uint32_t Random32::next() {
    value = hash(value);

    return value;
}

vec3 Random32::unit_vector() {
    vec3 r;

    bool c = true;
    while(c) {
        r = {operator()(), operator()(), operator()()};
        
        float len = length(r);
        if(!(len > 1 || len == 0)) c = false; 
    }
    
    r = normalize(r);

    return r;
}


float Random32::operator()(uvec3 i) {
    uint32_t hash_value = hash(uvec4(i, seed));

    return to_float(hash_value);
}

uint32_t Random32::hash_i(uvec3 i) {
    uint32_t hash_value = hash(uvec4(i, seed));

    return hash_value;
}

vec3 Random32::unit_vector(uvec3 i) {
    uint32_t current_value = seed;

    glm::vec3 unit_vector;
    float u_length = 10;
    while(u_length > 1.0 || u_length == 0.0) {
        current_value = hash(current_value);
        float x = to_float(hash(i) ^ current_value);
        current_value = hash(current_value);
        float y = to_float(hash(i) ^ current_value);
        current_value = hash(current_value);
        float z = to_float(hash(i) ^ current_value);

        unit_vector = glm::vec3(x, y, z);
        u_length = glm::length(unit_vector);
    }

    return normalize(unit_vector);
}

vec3 Random32::cube_vector(uvec3 i) {
    uint32_t current_value = seed;

    glm::vec3 vector;
    current_value = hash(current_value);
    float x = to_float(hash(i) ^ current_value);
    current_value = hash(current_value);
    float y = to_float(hash(i) ^ current_value);
    current_value = hash(current_value);
    float z = to_float(hash(i) ^ current_value);

    vector = {x, y, z};

    return vector;
}

Random::Random(uint64_t init_seed) {
    seed = init_seed;
    value = seed;
}

float Random::operator()() {
    value = hash(value);

    return to_float(value);
}

uint64_t Random::next() {
    value = hash(value);

    return value;
}


vec3 Random::unit_vector() {
    vec3 r;

    bool c = true;
    while(c) {
        r = {operator()(), operator()(), operator()()};
        
        float len = length(r);
        if(!(len > 1 || len == 0)) c = false; 
    }
    
    r = normalize(r);

    return r;
}

float Random::operator()(glm::vec<3, uint64_t> i) {
    uint32_t hash_value = hash(uvec4(i, seed));

    return to_float(hash_value);
}

uint64_t Random::hash_i(glm::vec<3, uint64_t> i) {
    uint64_t hash_value = hash(uvec4(i, seed));

    return hash_value;
}

vec3 Random::unit_vector(glm::vec<3, uint64_t> i) {
    uint64_t current_value = seed;

    glm::vec3 unit_vector;
    float u_length = 10;
    while(u_length > 1.0 || u_length == 0.0) {
        current_value = hash(current_value);
        float x = to_float(hash(i) ^ current_value);
        current_value = hash(current_value);
        float y = to_float(hash(i) ^ current_value);
        current_value = hash(current_value);
        float z = to_float(hash(i) ^ current_value);

        unit_vector = glm::vec3(x, y, z);
        u_length = glm::length(unit_vector);
    }

    return normalize(unit_vector);
}

vec3 Random::cube_vector(glm::vec<3, uint64_t> i) {
    uint64_t current_value = seed;

    glm::vec3 vector;
    current_value = hash(current_value);
    float x = to_float(hash(i) ^ current_value);
    current_value = hash(current_value);
    float y = to_float(hash(i) ^ current_value);
    current_value = hash(current_value);
    float z = to_float(hash(i) ^ current_value);

    vector = {x, y, z};

    return vector;
}