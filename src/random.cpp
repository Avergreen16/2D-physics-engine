#include "random.hpp"

#include <bit>

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
}

float Noise_gen::ridged_perlin_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed) {
    uint32_t h = hash(seed);

    float sum = 0.0;
    float maximum = 0.0;
    float power = 1;

    position = position / period;

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

        power *= 0.5f;

        position *= 2.0f;
    }

    sum /= maximum;

    return sum;
}

std::vector<float> Noise_gen::perlin_noise(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance) {
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

    ret.resize(size.x * size.y * size.z);
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