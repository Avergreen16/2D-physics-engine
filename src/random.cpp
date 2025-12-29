#include "random.hpp"

#include <bit>
#include <thread>
#include <iostream>


simd_ivec3 max(simd_ivec3 a, simd_ivec3 b) {
    simd_ivec3 ret;
    ret.x = select(a.x > b.x, a.x, b.x);
    ret.y = select(a.y > b.y, a.y, b.y);
    ret.z = select(a.z > b.z, a.z, b.z);

    return ret;
}

simd_ivec3 min(simd_ivec3 a, simd_ivec3 b) {
    simd_ivec3 ret;
    ret.x = select(a.x < b.x, a.x, b.x);
    ret.y = select(a.y < b.y, a.y, b.y);
    ret.z = select(a.z < b.z, a.z, b.z);

    return ret;
}

simd_ivec3 clamp(simd_ivec3 x, simd_ivec3 vmin, simd_ivec3 vmax) {
    return min(max(vmin, x), vmax);
}

batch_int max(batch_int a, batch_int b) {
    return select(a > b, a, b);
}

batch_int min(batch_int a, batch_int b) {
    return select(a < b, a, b);
}

batch_int clamp(batch_int x, batch_int vmin, batch_int vmax) {
    return min(max(vmin, x), vmax);
}

batch to_float(batch_int x) {
    const uint32_t float_mantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint32_t float_one = 0x3F800000u; // 1.0 in IEEE binary32

    x &= float_mantissa;
    x |= float_one;
    // index between 1 and 2;

    batch pos = xsimd::bit_cast<batch, batch_int>(x);

    pos -= 1.0f;

    return pos;
}

batch length(simd_vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

constexpr int PRIME_X = 73856093;
constexpr int PRIME_Y = 19349663;
constexpr int PRIME_Z = 83492791;
constexpr float PI = 3.14159265358979f;

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

vec3 hex_color(uint32_t color) {
    vec3 col = vec3((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    col /= 0xFF;

    return col;
}

vec3 hsv_color(float hue, float saturation, float value) {
    vec3 color;

    float f = fract(hue);

    if(hue < 1) {
        color = vec3(1.0, f, 0.0);
    } else if(hue < 2) {
        color = vec3(1.0 - f, 1.0, 0.0);
    } else if(hue < 3) {
        color = vec3(0.0, 1.0, f);
    } else if(hue < 4) {
        color = vec3(0.0, 1.0 - f, 1.0);
    } else if(hue < 5) {
        color = vec3(f, 0.0, 1.0);
    } else if(hue < 6) {
        color = vec3(1.0, 0.0, 1.0 - f);
    }

    color = color * saturation + (1.0f - saturation);
    color *= value;

    return color;
}

std::size_t Hash_coord::operator()(const ivec3& v) const {
    int hash = v.x * PRIME_X;
    hash ^= v.y * PRIME_Y;
    hash ^= v.z * PRIME_Z;

    hash ^= (hash >> 13);
    hash = hash * 60493 + 19990303;
    return abs(hash) % 16;
}

std::size_t Hash_coord::operator()(const ivec2& v) const {
    int hash = v.x * PRIME_X;
    hash ^= v.y * PRIME_Y;

    hash ^= (hash >> 13);
    hash = hash * 60493 + 19990303;
    return abs(hash) % 16;
}

vec3 get_vec(int i) {
    return perlin_vectors[i];
}

uint32_t hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

inline batch lerp(batch a, batch b, batch x) {
    return a + x * (b - a);
}

inline batch smoothstep(batch x) {
    batch xx = x * x;
    return xx * 3 - xx * x * 2;
}

inline simd_vec3 smoothstep(simd_vec3 v) {
    simd_vec3 ret;
    ret.x = smoothstep(v.x);
    ret.y = smoothstep(v.y);
    ret.z = smoothstep(v.z);
    return ret;
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

simd_vec3 unit_vector(batch_int seeds) {
    batch z = to_float(seeds);
    batch theta = 2.0f * PI * to_float(seeds ^ 0x1F085EE7);

    batch r = sqrt(1.0f - z * z);

    return simd_vec3(r * cos(theta), r * sin(theta), z);
}

float to_float_10(uint32_t m) {
    const uint32_t ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint32_t ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float f = std::bit_cast<float, uint32_t>(m);       // Range [1:2]
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

batch_int hash_coords(batch_int x, batch_int y, batch_int z, uint32_t seed) {
    batch_int h = x * PRIME_X;
    h ^= y * PRIME_Y;
    h ^= z * PRIME_Z;

    h ^= (h >> 13);
    h = h * 60493 + 19990303;
    h ^= hash(seed);
    return xsimd::abs(h) & 0xF;
}

batch_int hash_coords2(batch_int x, batch_int y, batch_int z, uint32_t seed) {
    batch_int h = x * PRIME_X;
    h ^= y * PRIME_Y;
    h ^= z * PRIME_Z;

    h ^= (h >> 13);
    h = h * 60493 + 19990303;
    h ^= hash(seed);
    return h;
}

int hash_coord(ivec3 v, uint32_t seed) {
    int h = v.x * PRIME_X;
    h ^= v.y * PRIME_Y;
    h ^= v.z * PRIME_Z;

    h ^= (h >> 13);
    h = h * 60493 + 19990303;
    h ^= hash(seed);
    return abs(h) % 16;
}

int hash_coord(ivec2 v, uint32_t seed) {
    int h = v.x * PRIME_X;
    h ^= v.y * PRIME_Y;

    h ^= (h >> 13);
    h = h * 60493 + 19990303;
    h ^= hash(seed);
    return abs(h) % 16;
}


vec2 get_vector_v2(ivec2 v, uint32_t seed) {
    int h = v.x * PRIME_X;
    h ^= v.y * PRIME_Y;

    h ^= (h >> 13);
    h = h * 60493 + 19990303;
    h ^= hash(seed);
    float f = abs(float(h) / 0x7FFFFFFF) * 2.0f * PI;

    return vec2(cos(f), sin(f));
}

vec3 get_vector_v3(ivec3 v, uint32_t seed) {
    int h = 0.0f;

    while(true) {
        h ^= v.x * PRIME_X;
        h ^= v.y * PRIME_Y;
        h ^= v.z * PRIME_Z;

        h ^= (h >> 13);
        h = h * 60493 + 19990303;
        h ^= hash(seed);

        uint32_t hh = std::bit_cast<uint32_t>(h);
        uint32_t hx = hh & 0x3FF;
        uint32_t hy = (hh >> 10) & 0x3FF;
        uint32_t hz = (hh >> 20) & 0x3FF;
        
        vec3 vector = vec3(hx, hy, hz) / float(0x3FF);
        vector = vector * 2.0f - 1.0f;
        float len = length(vector);

        if(len < 1.0f) {
            vector /= len;
            return vector;
        }
    }
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

std::array<vec3, 256> gen_voronoi_vectors() {
    std::array<vec3, 256> ret;

    Random random(8376436);

    for(int i = 0; i < 156; ++i) {
        vec3 v = {random(), random(), random()};

        v = v * 0.5f + 0.5f;

        ret[i] = v;
    }

    return ret;
}

std::array<vec3, 256> Noise_gen::voronoi_vectors = gen_voronoi_vectors();

std::array<int, 256> Noise_gen::hash_table = {
    151, 160, 137,  91,  90,  15, 131,  13, 201,  95,  96,  53, 194, 233,   7, 225,
    140,  36, 103,  30,  69, 142,   8,  99,  37, 240,  21,  10,  23, 190,   6, 148,
    247, 120, 234,  75,   0,  26, 197,  62,  94, 252, 219, 203, 117,  35,  11,  32,
    57, 177,  33,  88, 237, 149,  56,  87, 174,  20, 125, 136, 171, 168,  68, 175,
    74, 165,  71, 134, 139,  48,  27, 166,  77, 146, 158, 231,  83, 111, 229, 122,
    60, 211, 133, 230, 220, 105,  92,  41,  55,  46, 245,  40, 244, 102, 143,  54,
    65,  25,  63, 161,   1, 216,  80,  73, 209,  76, 132, 187, 208,  89,  18, 169,
    200, 196, 135, 130, 116, 188, 159,  86, 164, 100, 109, 198, 173, 186,   3,  64,
    52, 217, 226, 250, 124, 123,   5, 202,  38, 147, 118, 126, 255,  82,  85, 212,
    207, 206,  59, 227,  47,  16,  58,  17, 182, 189,  28,  42, 223, 183, 170, 213,
    119, 248, 152,   2,  44, 154, 163,  70, 221, 153, 101, 155, 167,  43, 172,   9,
    129,  22,  39, 253,  19,  98, 108, 110,  79, 113, 224, 232, 178, 185, 112, 104,
    218, 246,  97, 228, 251,  34, 242, 193, 238, 210, 144,  12, 191, 179, 162, 241,
    81,  51, 145, 235, 249,  14, 239, 107,  49, 192, 214,  31, 181, 199, 106, 157,
    184,  84, 204, 176, 115, 121,  50,  45, 127,   4, 150, 254, 138, 236, 205,  93,
    222, 114,  67,  29,  24,  72, 243, 141, 128, 195,  78,  66, 215,  61, 156, 180
};

std::array<vec3, 16> Noise_gen::perlin_vectors = {
    vec3(1, 1, 0),
    vec3(-1, 1, 0),
    vec3(1, -1, 0),
    vec3(-1, -1, 0), 
    vec3(1, 0, 1),
    vec3(-1, 0, 1),
    vec3(1, 0, -1),
    vec3(-1, 0, -1),
    vec3(0, 1, 1),
    vec3(0, -1, 1),
    vec3(0, 1, -1),
    vec3(0, -1, -1),
    vec3(1, 1, 0),
    vec3(-1, 1, 0),
    vec3(1, -1, 0),
    vec3(-1, -1, 0), 
};

uint8_t Noise_gen::hash_with_table(uvec3 i) {
    return hash_table[uint8_t(hash_table[uint8_t(hash_table[uint8_t(i.x)] + i.y)] + i.z)];
}


float Noise_gen::voronoi_noise(glm::vec3 position, float period, uint32_t seed) {
    Random32 random(seed);

    float dist = __FLT_MAX__;

    glm::vec3 pos = position / period;

    ivec3 cell_pos = glm::floor(pos);

    for(int z = -1; z <= 1; ++z) {
        for(int y = -1; y <= 1; ++y) {
            for(int x = -1; x <= 1; ++x) {
                ivec3 cell_point = cell_pos + ivec3(x, y, z);
                glm::vec3 pt = voronoi_vectors[hash_with_table(cell_point)] * 0.5f + 0.5f;

                float dist_pt = length((pos - vec3(cell_point)) - pt);

                dist = glm::min(dist_pt, dist);
            }
        }
    }

    return clamp(dist, 0.0f, 1.0f);
}

float Noise_gen::perlin_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed, float persistance) {
    uint32_t h = hash(seed);

    float sum = 0.0;
    float maximum = 0.0;
    float power = 1;

    position = position / period;

    for(int i = 0; i < octaves; ++i) {
        glm::ivec3 corner_id = glm::ivec3(floor(position));
        glm::vec3 fractional = glm::fract(position);
        glm::ivec3 vector_origins[8] = {
            corner_id,
            corner_id + glm::ivec3(1, 0, 0),
            corner_id + glm::ivec3(0, 1, 0),
            corner_id + glm::ivec3(1, 1, 0),
            corner_id + glm::ivec3(0, 0, 1),
            corner_id + glm::ivec3(1, 0, 1),
            corner_id + glm::ivec3(0, 1, 1),
            corner_id + glm::ivec3(1, 1, 1)
        };
        
        glm::vec3 vecs[8] = {
            get_vec(hash_coord(vector_origins[0], seed)),
            get_vec(hash_coord(vector_origins[1], seed)),
            get_vec(hash_coord(vector_origins[2], seed)),
            get_vec(hash_coord(vector_origins[3], seed)),
            get_vec(hash_coord(vector_origins[4], seed)),
            get_vec(hash_coord(vector_origins[5], seed)),
            get_vec(hash_coord(vector_origins[6], seed)),
            get_vec(hash_coord(vector_origins[7], seed)),
        };

        float values[8];

        for(int j = 0; j < 8; ++j) {
            vec3 origin = vector_origins[j];
            vec3 vector = vecs[j];

            vec3 offset = position - origin;

            values[j] = dot(offset, vector);
        }

        fractional = glm::vec3(smoothstep(0.0f, 1.0f, fractional.x), smoothstep(0.0f, 1.0f, fractional.y), smoothstep(0.0f, 1.0f, fractional.z));

        float v = mix(
            mix(
            mix(values[0], values[1], fractional.x), 
            mix(values[2], values[3], fractional.x), fractional.y),
            mix(
            mix(values[4], values[5], fractional.x), 
            mix(values[6], values[7], fractional.x), fractional.y), fractional.z);
        
        sum += v * power;
        maximum += power;

        power *= persistance;

        position *= 2.0f;
    }

    sum /= maximum;

    return sum;
}

float Noise_gen::ridged_perlin_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed, float persistance) {
    uint32_t h = hash(seed);

    float sum = 0.0;
    float maximum = 0.0;
    float power = 1;

    position = position / period;

    for(int i = 0; i < octaves; ++i) {
        glm::ivec3 corner_id = glm::ivec3(floor(position));
        glm::vec3 fractional = glm::fract(position);
        glm::ivec3 vector_origins[8] = {
            corner_id,
            corner_id + glm::ivec3(1, 0, 0),
            corner_id + glm::ivec3(0, 1, 0),
            corner_id + glm::ivec3(1, 1, 0),
            corner_id + glm::ivec3(0, 0, 1),
            corner_id + glm::ivec3(1, 0, 1),
            corner_id + glm::ivec3(0, 1, 1),
            corner_id + glm::ivec3(1, 1, 1)
        };
        
        glm::vec3 vecs[8] = {
            perlin_vectors[(hash_with_table(vector_origins[0]) ^ h) & 0xF],
            perlin_vectors[(hash_with_table(vector_origins[1]) ^ h) & 0xF],
            perlin_vectors[(hash_with_table(vector_origins[2]) ^ h) & 0xF],
            perlin_vectors[(hash_with_table(vector_origins[3]) ^ h) & 0xF],
            perlin_vectors[(hash_with_table(vector_origins[4]) ^ h) & 0xF],
            perlin_vectors[(hash_with_table(vector_origins[5]) ^ h) & 0xF],
            perlin_vectors[(hash_with_table(vector_origins[6]) ^ h) & 0xF],
            perlin_vectors[(hash_with_table(vector_origins[7]) ^ h) & 0xF],
        };

        float values[8];

        for(int j = 0; j < 8; ++j) {
            vec3 origin = vector_origins[j];
            vec3 vector = vecs[j];

            vec3 offset = position - origin;

            values[j] = dot(offset, vector);
        }

        fractional = glm::vec3(smoothstep(0.0f, 1.0f, fractional.x), smoothstep(0.0f, 1.0f, fractional.y), smoothstep(0.0f, 1.0f, fractional.z));

        float v = mix(
            mix(
            mix(values[0], values[1], fractional.x), 
            mix(values[2], values[3], fractional.x), fractional.y),
            mix(
            mix(values[4], values[5], fractional.x), 
            mix(values[6], values[7], fractional.x), fractional.y), fractional.z);

        v = 1.0f - abs(v);
        v *= v;
        
        sum += v * power;
        maximum += power;

        power *= persistance;

        position *= 2.0f;
    }

    sum /= maximum;

    return sum;
}

float Noise_gen::simplex_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed, float persistance) {
    Random r(seed);

    uint32_t h = hash(seed);

    float sum = 0.0;
    float maximum = 0.0;
    float power = 1;

    position = position / period;

    vec3 pos = position;
    pos += (1.0f / 3.0f) * (pos.x + pos.y + pos.z);

    for(int i = 0; i < octaves; ++i) {
        glm::ivec3 corner_id = glm::ivec3(floor(pos));
        glm::vec3 fractional = glm::fract(pos);

        glm::ivec3 order;
        if(fractional.x > fractional.y) {
            // 0 > 1
            if(fractional.y > fractional.z) {
                // 0 > 1 > 2
                order = {0, 1, 2};
            } else {
                // (0, 2) > 1
                if(fractional.x > fractional.z) {
                    // 0 > 2
                    order = {0, 2, 1};
                } else {
                    order = {2, 0, 1};
                }
            }
        } else {
            // 1 > 0
            if(fractional.x > fractional.z) {
                // 1 > 0 > 2
                order = {1, 0, 2};
            } else {
                // (1, 2) > 0
                if(fractional.y > fractional.z) {
                    // 1 > 2
                    order = {1, 2, 0};
                } else {
                    // 2 > 1
                    order = {2, 1, 0};
                }

            }
        }

        glm::ivec3 vector_origins[4] = {
            corner_id,
            corner_id,
            corner_id,
            corner_id + glm::ivec3(1, 1, 1)
        };

        ivec3 corner = corner_id;
        corner[order.x] += 1;
        vector_origins[1] = corner;
        corner[order.y] += 1;
        vector_origins[2] = corner;
        
        glm::vec3 vecs[4] = {
            get_vector_v3(vector_origins[0], seed),
            get_vector_v3(vector_origins[1], seed),
            get_vector_v3(vector_origins[2], seed),
            get_vector_v3(vector_origins[3], seed),
        };

        float total = 0.0f;

        for(int j = 0; j < 4; ++j) {
            vec3 origin = vector_origins[j];
            vec3 vector = vecs[j];

            vec3 offset = (pos - origin);
            offset -= (1.0f / 6.0f) * (offset.x + offset.y + offset.z);
            float gradient = dot(offset, vector);
            
            float R2 = 0.6f;

            float ti = R2 - dot(offset, offset);
            ti = max(0.0f, ti);
            ti = pow(ti, 4);

            total += ti * gradient * 32.0f;
        }

        sum += total * power;

        maximum += power;

        power *= persistance;

        pos *= 2.0f;
    }

    sum /= maximum;

    return sum;
}

void Noise_gen::simplex_noise(float* dst, vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance) {
    int num_target = size.x * size.y * size.z;

    float dx = 1.0f / size.x;
    float dy = 1.0f / size.y;
    float dz = 1.0f / size.z;
    float dxy = dx * dy;
    
    float frequency = 1.0f / period;

    alignas(32) int iota_v[N];
    for(int i = 0; i < N; ++i) {
        iota_v[i] = i;
    }
    batch_int iota = xsimd::load_aligned(iota_v);

    int num_threads = 1;
    std::vector<std::thread> threads(num_threads);

    int section = ceil(float(num_target) / num_threads);

    float third = 1.0f / 3.0f;
    float sixth = 1.0f / 6.0f;

    for(int i = 0; i < num_threads; ++i) {
        threads[i] = std::thread([&, i]() {
            int num_current = section * i;
            int end_pos = min(num_target, section * (i + 1));

            while(num_current < end_pos) {
                simd_ivec3 position_i;

                // bb is the index
                batch_int bb = iota + float(num_current);

                batch bbf = xsimd::batch_cast<float>(bb);
                simd_vec3 p_i;

                batch zeroes = xsimd::broadcast<float>(0.0f);
                batch ones = xsimd::broadcast<float>(1.0f);

                p_i.x = bbf - floor(bbf * dx) * (float)size.x;
                p_i.y = floor((bbf - p_i.x) * dx) - floor((bbf * dx) * dy) * (float)size.y;
                p_i.z = floor((bbf - p_i.x - p_i.y * (float)size.x) * dxy) - floor(bbf * dxy) * dz * (float)size.z;

                simd_vec3 position = p_i;
                position += third * (position.x + position.y + position.z);

                position *= diff;
                simd_vec3 save_pos = position;

                position += pos;
                position *= frequency;

                batch sum = zeroes;

                float maximum = 0.0;
                float power = 1.0f;

                for(int i = 0; i < octaves; ++i) {
                    simd_vec3 corner = position.floor();
                    simd_vec3 fractional = position - corner;

                    xsimd::batch_bool<float> first_is_x = (fractional.x >= fractional.y) && (fractional.x >= fractional.z);
                    xsimd::batch_bool<float> first_is_y = (fractional.y > fractional.x) && (fractional.y >= fractional.z);
                    xsimd::batch_bool<float> first_is_z = (fractional.z > fractional.x) && (fractional.z > fractional.y);
                    
                    xsimd::batch_bool<float> second_is_x = (fractional.x >= fractional.y) != (fractional.x >= fractional.z);
                    xsimd::batch_bool<float> second_is_y = (fractional.y > fractional.x) != (fractional.y >= fractional.z);
                    xsimd::batch_bool<float> second_is_z = (fractional.z > fractional.x) != (fractional.z > fractional.y);

                    simd_vec3 first(xsimd::select(first_is_x, ones, zeroes), xsimd::select(first_is_y, ones, zeroes), xsimd::select(first_is_z, ones, zeroes));
                    simd_vec3 second(xsimd::select(second_is_x, ones, zeroes), xsimd::select(second_is_y, ones, zeroes), xsimd::select(second_is_z, ones, zeroes));
                    second += first;

                    // get corners
                    simd_vec3 origins[4] = {
                        corner,
                        corner + first,
                        corner + second,
                        corner + vec3(1, 1, 1),
                    };

                    simd_vec3 gradients[4];
                    
                    int ii = 0;
                    for(simd_vec3& v : origins) {
                        batch_int x = xsimd::batch_cast<int>(v.x);
                        batch_int y = xsimd::batch_cast<int>(v.y);
                        batch_int z = xsimd::batch_cast<int>(v.z);
                        
                        batch_int index = hash_coords(x, y, z, seed);

                        batch_int batch_A = ((index & 1) << 1) - 1; // -1 and 1
                        batch_int batch_B = ((index & 2)) - 1; // -1 and 1
                        batch_int batch_C = (((index + 1) & 2)) - 1; // -1 and 1
                        batch_int batch_0 = index & 0; // zero
                        auto batch_1 = index > 3;
                        auto batch_2 = index < 8 || index > 11;
                        auto batch_3 = index < 12;

                        gradients[ii].x = xsimd::batch_cast<float>(xsimd::select(batch_1, batch_A, batch_0));
                        gradients[ii].y = xsimd::batch_cast<float>(xsimd::select(batch_2, batch_B, batch_0));
                        gradients[ii].z = xsimd::batch_cast<float>(xsimd::select(batch_3, batch_C, batch_0));

                        ++ii;
                    }

                    batch total = zeroes;

                    for(int iii = 0; iii < 4; ++iii) {
                        simd_vec3 origin = origins[iii];
                        simd_vec3& gradient = gradients[iii];

                        simd_vec3 offset = position - origin;
                        offset -= sixth * (offset.x + offset.y + offset.z);

                        batch grad = offset.dot(gradient);
            
                        float R2 = 0.6f;

                        batch ti = R2 - (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
                        ti = select(ti > 0.0f, ti, zeroes);
                        ti = ti * ti * ti * ti;

                        total += ti * grad * 32.0f;
                    }
                    
                    sum += total * power;

                    maximum += power;
                    power *= persistance;
                    position *= 2.0f;
                }

                float m = 1.0f / maximum;

                sum *= m;

                alignas(32) float r[N];
                sum.store_aligned(r);

                for(int i = 0; i < N; ++i) {
                    dst[i + num_current] = r[i];
                }

                num_current += N;
            }
        });
    }

    for(auto& thread : threads) {
        thread.join();
    }
}

/*
2D Averie Noise
mat2 matrix = inverse(mat2{
    {1.0f, 0.0f},
    {-0.5, sqrt(3.0f) / 2.0f}
});

mat2 inv = inverse(matrix);

float Noise_gen::averie_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed, float persistance) {
    Random r(seed);

    uint32_t h = hash(seed);

    float sum = 0.0;
    float maximum = 0.0;
    float power = 1;

    position = position / period;

    vec2 pos = position;
    pos = matrix * pos;

    for(int i = 0; i < octaves; ++i) {
        glm::ivec2 corner_id = glm::ivec2(floor(pos));
        glm::vec2 fractional = glm::fract(pos);

        glm::ivec2 order;
        if(fractional.x > fractional.y) {
            order = {0, 1};
        } else {
            order = {1, 0};
        }

        glm::ivec2 vector_origins[3] = {
            corner_id,
            corner_id,
            corner_id + glm::ivec2(1, 1)
        };

        ivec2 corner = corner_id;
        corner[order.x] += 1;
        vector_origins[1] = corner;
        
        glm::vec2 vecs[3] = {
            get_vector_v2(vector_origins[0], seed),
            get_vector_v2(vector_origins[1], seed),
            get_vector_v2(vector_origins[2], seed),
        };

        vec3 ap = vec3(vec2(vector_origins[0]) - pos, 0.0f);
        vec3 bp = vec3(vec2(vector_origins[1]) - pos, 0.0f);
        vec3 cp = vec3(vec2(vector_origins[2]) - pos, 0.0f);
        vec3 ab = vec3(vector_origins[1] - vector_origins[0], 0.0f);
        vec3 ac = vec3(vector_origins[2] - vector_origins[0], 0.0f);
        
        float total_area = abs(cross(ab, ac).z);
        float area_ab = abs(cross(ap, bp).z);
        float area_bc = abs(cross(bp, cp).z);
        float area_ac = abs(cross(ap, cp).z);
        
        float weights[3] {
            area_bc / total_area,
            area_ac / total_area,
            area_ab / total_area
        };

        float total = 0.0f;
        float denom = 0.0f;

        for(int j = 0; j < 3; ++j) {
            vec2 origin = vector_origins[j];
            vec2 vector = vecs[j];

            vec2 offset = pos - origin;

            float weight = weights[j];
            weight *= weight;
            //weight = smoothstep(0.0f, 1.0f, weight);
            total += dot(offset, vector) * weight;
            denom += weight;
        }

        //fractional = glm::vec3(smoothstep(0.0f, 1.0f, fractional.x), smoothstep(0.0f, 1.0f, fractional.y), smoothstep(0.0f, 1.0f, fractional.z));

        sum += (total / denom) * power;

        maximum += power;

        power *= persistance;

        pos *= 2.0f;
    }

    sum /= maximum;

    return sum;
}
*/

std::array<vec3, 16> perlin_vectors = {
    vec3(0, -1, -1),
    vec3(0, -1, 1),
    vec3(0, 1, 1),
    vec3(0, 1, -1), 
    vec3(-1, 0, -1),
    vec3(1, 0, 1),
    vec3(-1, 0, 1),
    vec3(1, 0, -1),
    vec3(-1, 0, -1),
    vec3(1, 0, 1),
    vec3(-1, 0, 1),
    vec3(1, 0, -1), 
    vec3(-1, -1, 0),
    vec3(1, -1, 0),
    vec3(-1, 1, 0),
    vec3(1, 1, 0),
};

void Noise_gen::perlin_noise(float* dst, vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, ivec3 cycle, float persistance) {
    int num_target = size.x * size.y * size.z;

    float dx = 1.0f / size.x;
    float dy = 1.0f / size.y;
    float dz = 1.0f / size.z;
    float dxy = dx * dy;
    
    float frequency = 1.0f / period;

    alignas(32) int iota_v[N];
    for(int i = 0; i < N; ++i) {
        iota_v[i] = i;
    }
    batch_int iota = xsimd::load_aligned(iota_v);

    int num_threads = 1;
    std::vector<std::thread> threads(num_threads);

    int section = ceil(float(num_target) / num_threads);

    batch_int mx = xsimd::broadcast(cycle.x);
    batch_int my = xsimd::broadcast(cycle.y);
    batch_int mz = xsimd::broadcast(cycle.z);

    for(int i = 0; i < num_threads; ++i) {
        threads[i] = std::thread([&, i]() {
            int num_current = section * i;
            int end_pos = min(num_target, section * (i + 1));

            while(num_current < end_pos) {
                simd_ivec3 position_i;

                // bb is the index
                batch_int bb = iota + float(num_current);

                batch bbf = xsimd::batch_cast<float>(bb);
                simd_vec3 p_i;

                p_i.x = bbf - floor(bbf * dx) * (float)size.x;
                p_i.y = floor((bbf - p_i.x) * dx) - floor((bbf * dx) * dy) * (float)size.y;
                p_i.z = floor((bbf - p_i.x - p_i.y * (float)size.x) * dxy) - floor(bbf * dxy) * dz * (float)size.z;

                /*
                simd_vec3 position;
                position.x = xsimd::batch_cast<float>(position_i.x);
                position.y = xsimd::batch_cast<float>(position_i.y);
                position.z = xsimd::batch_cast<float>(position_i.z);
                */
                simd_vec3 position = p_i;

                position *= diff;
                simd_vec3 save_pos = position;

                position += pos;
                position *= frequency;

                alignas(32) float v[N];
                for(int i = 0; i < N; ++i) v[i] = 0;

                batch sum = xsimd::load_aligned(v);

                float maximum = 0.0;
                float power = 1.0f;

                for(int i = 0; i < octaves; ++i) {
                    simd_vec3 corner = position.floor();
                    simd_vec3 fractional = position - corner;
                    simd_vec3 origins[8] = {
                        corner,
                        corner + vec3(1, 0, 0),
                        corner + vec3(0, 1, 0),
                        corner + vec3(1, 1, 0),
                        corner + vec3(0, 0, 1),
                        corner + vec3(1, 0, 1),
                        corner + vec3(0, 1, 1),
                        corner + vec3(1, 1, 1),
                    };

                    batch values[8];
                    simd_vec3 gradient;
                    
                    int ii = 0;
                    for(simd_vec3& v : origins) {
                        batch_int x = xsimd::batch_cast<int>(v.x);
                        batch_int y = xsimd::batch_cast<int>(v.y);
                        batch_int z = xsimd::batch_cast<int>(v.z);

                        x %= mx;
                        y %= my;
                        z %= mz;
                        

                        batch_int index = hash_coords2(x, y, z, seed);
                        gradient = unit_vector(index);
                        
                        values[ii] = (position - v).dot(gradient);
                        ++ii;

                        /*
                        batch_int index = hash_coords(x, y, z, seed);

                        batch_int batch_A = ((index & 1) << 1) - 1; // -1 and 1
                        batch_int batch_B = ((index & 2)) - 1; // -1 and 1
                        batch_int batch_C = (((index + 1) & 2)) - 1; // -1 and 1
                        batch_int batch_0 = index & 0; // zero
                        auto batch_1 = index > 3;
                        auto batch_2 = index < 4 || index > 11;
                        auto batch_3 = index < 12;

                        gradient.x = xsimd::batch_cast<float>(xsimd::select(batch_1, batch_A, batch_0));
                        gradient.y = xsimd::batch_cast<float>(xsimd::select(batch_2, batch_B, batch_0));
                        gradient.z = xsimd::batch_cast<float>(xsimd::select(batch_3, batch_C, batch_0));

                        values[ii] = (position - v).dot(gradient);
                        ++ii;*/
                    }
                    
                    fractional = smoothstep(fractional);

                    batch A = lerp(values[0], values[1], fractional.x);
                    batch B = lerp(values[2], values[3], fractional.x);
                    batch C = lerp(values[4], values[5], fractional.x);
                    batch D = lerp(values[6], values[7], fractional.x);

                    A = lerp(A, B, fractional.y);
                    B = lerp(C, D, fractional.y);
                    
                    A = lerp(A, B, fractional.z);
                    
                    sum += A * power;
                    //sum += save_pos.z;
                    maximum += power;
                    power *= persistance;
                    position *= 2.0f;
                }

                float m = 1.0f / maximum;

                sum *= m;

                alignas(32) float r[N];
                sum.store_aligned(r);
                //memcpy(&ret[i], &sum, N);

                for(int i = 0; i < N; ++i) {
                    dst[i + num_current] = r[i];
                }

                num_current += N;
            }
        });
    }

    for(auto& thread : threads) {
        thread.join();
    }
}

void Noise_gen::voronoi_noise(float* dst, vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, ivec3 cycle, float persistance) {
    int num_target = size.x * size.y * size.z;

    float dx = 1.0f / size.x;
    float dy = 1.0f / size.y;
    float dz = 1.0f / size.z;
    float dxy = dx * dy;
    
    float frequency = 1.0f / period;

    alignas(32) int iota_v[N];
    for(int i = 0; i < N; ++i) {
        iota_v[i] = i;
    }
    batch_int iota = xsimd::load_aligned(iota_v);
    batch sqrt2 = xsimd::broadcast(sqrt(2.0f));

    int num_threads = 1;
    std::vector<std::thread> threads(num_threads);

    int section = ceil(float(num_target) / num_threads);

    batch_int mx = xsimd::broadcast(cycle.x);
    batch_int my = xsimd::broadcast(cycle.y);
    batch_int mz = xsimd::broadcast(cycle.z);

    for(int i = 0; i < num_threads; ++i) {
        threads[i] = std::thread([&, i]() {
            int num_current = section * i;
            int end_pos = min(num_target, section * (i + 1));

            while(num_current < end_pos) {
                simd_ivec3 position_i;

                // bb is the index
                batch_int bb = iota + float(num_current);

                batch bbf = xsimd::batch_cast<float>(bb);
                simd_vec3 p_i;

                p_i.x = bbf - floor(bbf * dx) * (float)size.x;
                p_i.y = floor((bbf - p_i.x) * dx) - floor((bbf * dx) * dy) * (float)size.y;
                p_i.z = floor((bbf - p_i.x - p_i.y * (float)size.x) * dxy) - floor(bbf * dxy) * dz * (float)size.z;

                /*
                simd_vec3 position;
                position.x = xsimd::batch_cast<float>(position_i.x);
                position.y = xsimd::batch_cast<float>(position_i.y);
                position.z = xsimd::batch_cast<float>(position_i.z);
                */
                simd_vec3 position = p_i;

                position *= diff;
                simd_vec3 save_pos = position;

                position += pos;
                position *= frequency;

                alignas(32) float v[N];
                for(int i = 0; i < N; ++i) v[i] = 0;

                batch sum = xsimd::load_aligned(v);

                float maximum = 0.0;
                float power = 1.0f;

                batch min_v = sqrt2;

                for(int i = 0; i < octaves; ++i) {
                    simd_vec3 corner = position.floor();
                    simd_vec3 fractional = position - corner;
                    simd_vec3 origins[27] = {
                        corner + vec3(-1, -1, -1),
                        corner + vec3(0, -1, -1),
                        corner + vec3(1, -1, -1),
                        corner + vec3(-1, 0, -1),
                        corner + vec3(0, 0, -1),
                        corner + vec3(1, 0, -1),
                        corner + vec3(-1, 1, -1),
                        corner + vec3(0, 1, -1),
                        corner + vec3(1, 1, -1),
                        
                        corner + vec3(-1, -1, 0),
                        corner + vec3(0, -1, 0),
                        corner + vec3(1, -1, 0),
                        corner + vec3(-1, 0, 0),
                        corner + vec3(0, 0, 0),
                        corner + vec3(1, 0, 0),
                        corner + vec3(-1, 1, 0),
                        corner + vec3(0, 1, 0),
                        corner + vec3(1, 1, 0),
                        
                        corner + vec3(-1, -1, 1),
                        corner + vec3(0, -1, 1),
                        corner + vec3(1, -1, 1),
                        corner + vec3(-1, 0, 1),
                        corner + vec3(0, 0, 1),
                        corner + vec3(1, 0, 1),
                        corner + vec3(-1, 1, 1),
                        corner + vec3(0, 1, 1),
                        corner + vec3(1, 1, 1),
                    };
                    
                    for(simd_vec3& v : origins) {
                        batch_int x = xsimd::batch_cast<int>(v.x);
                        batch_int y = xsimd::batch_cast<int>(v.y);
                        batch_int z = xsimd::batch_cast<int>(v.z);

                        x %= mx;
                        y %= my;
                        z %= mz;
                        
                        batch_int index_x = hash_coords2(x, y, z, seed);
                        batch_int index_y = index_x ^ PRIME_X;
                        batch_int index_z = index_y ^ PRIME_Y;

                        simd_vec3 voronoi_pos;
                        voronoi_pos.x = to_float(index_x);
                        voronoi_pos.y = to_float(index_y);
                        voronoi_pos.z = to_float(index_z);

                        voronoi_pos += v;

                        voronoi_pos -= position;

                        batch len = length(voronoi_pos);

                        min_v = min(min_v, len);
                    }
                    
                    sum += min_v * power;
                    //sum += save_pos.z;
                    maximum += power;
                    power *= persistance;
                    position *= 2.0f;
                }

                float m = 1.0f / maximum;

                sum *= m;

                alignas(32) float r[N];
                sum.store_aligned(r);
                //memcpy(&ret[i], &sum, N);

                for(int i = 0; i < N; ++i) {
                    dst[i + num_current] = r[i];
                }

                num_current += N;
            }
        });
    }

    for(auto& thread : threads) {
        thread.join();
    }
}


std::vector<float> Noise_gen::ridged_perlin_noise(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance) {
    uint32_t h = hash(seed);

    std::vector<float> ret;
    ret.reserve(size.x * size.y * size.z);

    //stdx::simd_size<float> size_var;
    //int size_v = size_var.value;
    int size_v = 16;

    int num_current = 0;
    int num_target = size.x * size.y * size.z;

    while(num_current < num_target) {
        stdx::fixed_size_simd<float, 16> a([num_current, pos, period, octaves, seed, size, diff, persistance, h](int i) {
            uint32_t i2 = i + num_current;
            ivec3 chunk_pos = {i2 % size.x, i2 / size.x % size.y, i2 / (size.x * size.y)};

            vec3 position = (vec3)chunk_pos * diff;
            position += pos;
            position /= period;

            float sum = 0.0;
            float maximum = 0.0;
            float power = 1;

            //position = position / period;

            for(int i = 0; i < octaves; ++i) {
                glm::ivec3 corner_id = glm::ivec3(floor(position));
                glm::vec3 fractional = glm::fract(position);
                glm::vec3 vector_origins[8] = {
                    corner_id,
                    corner_id + glm::ivec3(1, 0, 0),
                    corner_id + glm::ivec3(0, 1, 0),
                    corner_id + glm::ivec3(1, 1, 0),
                    corner_id + glm::ivec3(0, 0, 1),
                    corner_id + glm::ivec3(1, 0, 1),
                    corner_id + glm::ivec3(0, 1, 1),
                    corner_id + glm::ivec3(1, 1, 1)
                };
                
                glm::vec3 vecs[8] = {
                    perlin_vectors[(hash_with_table(vector_origins[0]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[1]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[2]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[3]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[4]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[5]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[6]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[7]) ^ h) & 0xF],
                };

                float values[8];

                for(int j = 0; j < 8; ++j) {
                    vec3 origin = vector_origins[j];
                    vec3 vector = vecs[j];

                    vec3 offset = position - origin;

                    values[j] = dot(offset, vector);
                }

                fractional = glm::vec3(smoothstep(0.0f, 1.0f, fractional.x), smoothstep(0.0f, 1.0f, fractional.y), smoothstep(0.0f, 1.0f, fractional.z));

                float v = mix(
                    mix(
                    mix(values[0], values[1], fractional.x), 
                    mix(values[2], values[3], fractional.x), fractional.y),
                    mix(
                    mix(values[4], values[5], fractional.x), 
                    mix(values[6], values[7], fractional.x), fractional.y), fractional.z);

                v = abs(v);
                
                sum += v * power;
                maximum += power;

                power *= persistance;

                position *= 2.0f;
            }

            sum /= maximum;

            return sum;
        });

        for(int i = 0; i < size_v; ++i) {
            ret.push_back(a[i]);
        }

        num_current += size_v;
    }

    ret.resize(size.x * size.y * size.z);
    return ret;
}


std::vector<float> Noise_gen::perlin_noise_normalized(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance) {
    uint32_t h = hash(seed);

    std::vector<float> ret;
    ret.reserve(size.x * size.y * size.z);

    //stdx::simd_size<float> size_var;
    //int size_v = size_var.value;
    int size_v = 16;

    int num_current = 0;
    int num_target = size.x * size.y * size.z;


    while(num_current < num_target) {
        stdx::fixed_size_simd<float, 16> a([num_current, pos, period, octaves, seed, size, diff, persistance, h](int i) {
            uint32_t i2 = i + num_current;
            ivec3 chunk_pos = {i2 % size.x, i2 / size.x % size.y, i2 / (size.x * size.y)};

            vec3 position = (vec3)chunk_pos * diff;
            position += pos;
            position = normalize(position);
            position /= period;

            float sum = 0.0;
            float maximum = 0.0;
            float power = 1;

            //position = position / period;

            for(int i = 0; i < octaves; ++i) {
                vec3 norm_pos = normalize(position);

                glm::ivec3 corner_id = glm::ivec3(floor(position));
                glm::vec3 fractional = glm::fract(position);
                glm::vec3 vector_origins[8] = {
                    corner_id,
                    corner_id + glm::ivec3(1, 0, 0),
                    corner_id + glm::ivec3(0, 1, 0),
                    corner_id + glm::ivec3(1, 1, 0),
                    corner_id + glm::ivec3(0, 0, 1),
                    corner_id + glm::ivec3(1, 0, 1),
                    corner_id + glm::ivec3(0, 1, 1),
                    corner_id + glm::ivec3(1, 1, 1)
                };
                
                glm::vec3 vecs[8] = {
                    perlin_vectors[(hash_with_table(vector_origins[0]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[1]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[2]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[3]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[4]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[5]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[6]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[7]) ^ h) & 0xF],
                };

                float values[8];

                for(int j = 0; j < 8; ++j) {
                    vec3 origin = vector_origins[j];
                    vec3 vector = vecs[j];

                    vec3 offset = position - origin;

                    values[j] = dot(offset, vector);
                }

                fractional = glm::vec3(smoothstep(0.0f, 1.0f, fractional.x), smoothstep(0.0f, 1.0f, fractional.y), smoothstep(0.0f, 1.0f, fractional.z));

                float v = mix(
                    mix(
                    mix(values[0], values[1], fractional.x), 
                    mix(values[2], values[3], fractional.x), fractional.y),
                    mix(
                    mix(values[4], values[5], fractional.x), 
                    mix(values[6], values[7], fractional.x), fractional.y), fractional.z);
                
                sum += v * power;
                maximum += power;

                power *= persistance;

                position *= 2.0f;
            }

            sum /= maximum;

            return sum;
        });

        for(int i = 0; i < size_v; ++i) {
            ret.push_back(a[i]);
        }

        num_current += size_v;
    }

    //ret.resize(size.x * size.y * size.z);
    return ret;
}


std::vector<float> Noise_gen::ridged_perlin_noise_normalized(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance) {
    uint32_t h = hash(seed);

    std::vector<float> ret;
    ret.reserve(size.x * size.y * size.z);

    //stdx::simd_size<float> size_var;
    //int size_v = size_var.value;
    int size_v = 16;

    int num_current = 0;
    int num_target = size.x * size.y * size.z;

    while(num_current < num_target) {
        stdx::fixed_size_simd<float, 16> a([num_current, pos, period, octaves, seed, size, diff, persistance, h](int i) {
            uint32_t i2 = i + num_current;
            ivec3 chunk_pos = {i2 % size.x, i2 / size.x % size.y, i2 / (size.x * size.y)};

            vec3 position = (vec3)chunk_pos * diff;
            position += pos;
            position = normalize(position);
            position /= period;

            float sum = 0.0;
            float maximum = 0.0;
            float power = 1;

            //position = position / period;

            for(int i = 0; i < octaves; ++i) {
                glm::ivec3 corner_id = glm::ivec3(floor(position));
                glm::vec3 fractional = glm::fract(position);
                glm::vec3 vector_origins[8] = {
                    corner_id,
                    corner_id + glm::ivec3(1, 0, 0),
                    corner_id + glm::ivec3(0, 1, 0),
                    corner_id + glm::ivec3(1, 1, 0),
                    corner_id + glm::ivec3(0, 0, 1),
                    corner_id + glm::ivec3(1, 0, 1),
                    corner_id + glm::ivec3(0, 1, 1),
                    corner_id + glm::ivec3(1, 1, 1)
                };
                
                glm::vec3 vecs[8] = {
                    perlin_vectors[(hash_with_table(vector_origins[0]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[1]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[2]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[3]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[4]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[5]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[6]) ^ h) & 0xF],
                    perlin_vectors[(hash_with_table(vector_origins[7]) ^ h) & 0xF],
                };

                float values[8];

                for(int j = 0; j < 8; ++j) {
                    vec3 origin = vector_origins[j];
                    vec3 vector = vecs[j];

                    vec3 offset = position - origin;

                    values[j] = dot(offset, vector);
                }

                fractional = glm::vec3(smoothstep(0.0f, 1.0f, fractional.x), smoothstep(0.0f, 1.0f, fractional.y), smoothstep(0.0f, 1.0f, fractional.z));

                float v = mix(
                    mix(
                    mix(values[0], values[1], fractional.x), 
                    mix(values[2], values[3], fractional.x), fractional.y),
                    mix(
                    mix(values[4], values[5], fractional.x), 
                    mix(values[6], values[7], fractional.x), fractional.y), fractional.z);

                v = abs(v);
                
                sum += v * power;
                maximum += power;

                power *= persistance;

                position *= 2.0f;
            }

            sum /= maximum;

            return sum;
        });

        for(int i = 0; i < size_v; ++i) {
            ret.push_back(a[i]);
        }

        num_current += size_v;
    }

    ret.resize(size.x * size.y * size.z);
    return ret;
}

/*void Noise_gen::generate_noise(glm::ivec4 index, float* ptr) {
    float s1 = pow(2, core.world->octree_width - index.w);
    float scale = s1 / CHUNK_SIZE;
    glm::vec3 origin = (glm::vec3)index.xyz() * s1 - (float)pow(2, core.world->octree_width - 1);

    glm::vec3 size = glm::vec3(CHUNK_SIZE + 2);
    glm::vec3 range = size * scale;
    //origin -= scale;
    float frequency = 128;
    int octaves = 3;
    uint32_t seed = 56;

    for(int z = 0; z < size.z; ++z) {
        for(int y = 0; y < size.y; ++y) {
            for(int x = 0; x < size.x; ++x) {
                
                glm::vec3 global_loc = origin + range * (glm::vec3(x, y, z) / size);

                float variance = noise(global_loc, 128, 3, 25);

                float elevation = ridged_noise(global_loc, 4096, 3, 26);

                float temperature = 0.0;

                float humidity = noise(global_loc, 512, 4, 27);

                int i = z * size.x * size.y + y * size.x + x;
                i *= 4;

                ptr[i] = variance;
                ptr[i + 1] = elevation;
                ptr[i + 2] = temperature;
                ptr[i + 3] = humidity;
            }
        }
    }

    //noise_gen_mutex.lock();
    /*float s1 = pow(2, core.world->octree_width - index.w);
    float scale = s1 / CHUNK_SIZE;
    glm::vec3 origin = (glm::vec3)index.xyz() * s1 - (float)pow(2, core.world->octree_width - 1);
        
    Texture t;
    t.load(glm::uvec3{CHUNK_SIZE + 2, CHUNK_SIZE + 2, CHUNK_SIZE + 2}, {GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT});

    core.shaders["noise_terrain"].use();

    t.bind_image(0, 0, GL_RGBA16);
    glUniform1ui(0, 56u);
    glUniform1f(1, 128);
    glUniform1ui(2, 3);
    glUniform3f(3, origin.x - scale, origin.y - scale, origin.z - scale);
    glUniform3f(4, (CHUNK_SIZE + 2) * scale, (CHUNK_SIZE + 2) * scale, (CHUNK_SIZE + 2) * scale);

    Shader::dispatch_compute(glm::uvec3{CHUNK_SIZE + 2, CHUNK_SIZE + 2, CHUNK_SIZE + 2});

    //get_image(ptr, (unsigned int)pow(CHUNK_SIZE + 2, 3) * 2 * 4, t);
    return t;
}*/