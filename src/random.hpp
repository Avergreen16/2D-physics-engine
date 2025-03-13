#pragma once;
#include "wrapper.hpp"

#include <array>
#include <vector>
#include <experimental/simd>

namespace stdx = std::experimental;

uint32_t hash(uint32_t x);

uint32_t hash(glm::uvec2 v);

uint32_t hash(glm::uvec3 v);

uint32_t hash(glm::uvec4 v);

float to_float(uint32_t m);


uint64_t hash(uint64_t x);

uint64_t hash(vec<2, uint64_t> v);

uint64_t hash(vec<3, uint64_t> v);

uint64_t hash(vec<4, uint64_t> v);

struct Random {
    uint64_t seed;
    uint64_t value;
    
    Random(uint64_t init_seed);

    Random(const Random& r) = default;
    Random& operator=(const Random& r) = default;
    Random(Random&& r) = default;
    Random& operator=(Random&& r) = default;

    float operator()();
    uint64_t next();
    vec3 unit_vector();

    float operator()(glm::vec<3, uint64_t> i);
    uint64_t hash_i(glm::vec<3, uint64_t> i);
    vec3 unit_vector(glm::vec<3, uint64_t> i);
    vec3 cube_vector(glm::vec<3, uint64_t> i);
};

struct Random32 {
    uint32_t seed;
    uint32_t value;

    Random32(uint32_t init_seed);

    Random32(const Random32& r) = default;
    Random32& operator=(const Random32& r) = default;
    Random32(Random32&& r) = default;
    Random32& operator=(Random32&& r) = default;

    float operator()();
    uint32_t next();
    vec3 unit_vector();

    float operator()(uvec3 i);
    uint32_t hash_i(uvec3 i);
    vec3 unit_vector(uvec3 i);
    vec3 cube_vector(uvec3 i);
};

std::array<vec3, 256> gen_voronoi_vectors();

struct Noise_gen {
    static std::array<int, 256> hash_table;
    static std::array<vec3, 16> perlin_vectors;
    static std::array<vec3, 256> voronoi_vectors;

    static uint8_t hash_with_table(uvec3 i);

    static float voronoi_noise(glm::vec3 position, float period, uint32_t seed);

    static float perlin_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed, float persistance = 0.5f);

    static float ridged_perlin_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed);

    static std::vector<float> perlin_noise(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance = 0.5f);
    
    static std::vector<float> ridged_perlin_noise(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance = 0.5f);
    
    static std::vector<float> perlin_noise_normalized(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance = 0.5f);
    
    static std::vector<float> ridged_perlin_noise_normalized(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance = 0.5f);
    
    void generate_noise(glm::ivec4 index, float* ptr);
};