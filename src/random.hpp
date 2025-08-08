#pragma once;
#include "wrapper.hpp"

#include <array>
#include <vector>

#include <xsimd/xsimd.hpp>

using batch = xsimd::batch<float, xsimd::default_arch>;
using batch_int = xsimd::batch<int, xsimd::default_arch>;
constexpr std::size_t N = batch::size;

struct simd_vec2 {
    batch x;
    batch y;

    simd_vec2 operator+(const simd_vec2& a) {
        simd_vec2 ret_v;
        ret_v.x = x + a.x;
        ret_v.y = y + a.y;
        return ret_v;
    }

    simd_vec2 operator-(const simd_vec2& a) {
        simd_vec2 ret_v;
        ret_v.x = x - a.x;
        ret_v.y = y - a.y;
        return ret_v;
    }

    simd_vec2 operator*(const simd_vec2& a) {
        simd_vec2 ret_v;
        ret_v.x = x * a.x;
        ret_v.y = y * a.y;
        return ret_v;
    }

    simd_vec2 operator/(const simd_vec2& a) {
        simd_vec2 ret_v;
        ret_v.x = x / a.x;
        ret_v.y = y / a.y;
        return ret_v;
    }

    simd_vec2 operator+=(const simd_vec2& a) {
        x = x + a.x;
        y = y + a.y;

        return *this;
    }

    simd_vec2 operator-=(const simd_vec2& a) {
        x = x - a.x;
        y = y - a.y;

        return *this;
    }

    simd_vec2 operator*=(const simd_vec2& a) {
        x = x * a.x;
        y = y * a.y;

        return *this;
    }

    simd_vec2 operator/=(const simd_vec2& a) {
        x = x / a.x;
        y = y / a.y;

        return *this;
    }

    simd_vec2 operator=(const simd_vec2& a) {
        x = a.x;
        y = a.y;

        return *this;
    }

    simd_vec2 operator+(const vec2& a) {
        simd_vec2 ret_v;
        ret_v.x = x + a.x;
        ret_v.y = y + a.y;
        return ret_v;
    }

    simd_vec2 operator-(const vec2& a) {
        simd_vec2 ret_v;
        ret_v.x = x - a.x;
        ret_v.y = y - a.y;
        return ret_v;
    }

    simd_vec2 operator*(const vec2& a) {
        simd_vec2 ret_v;
        ret_v.x = x * a.x;
        ret_v.y = y * a.y;
        return ret_v;
    }

    simd_vec2 operator/(const vec2& a) {
        simd_vec2 ret_v;
        ret_v.x = x / a.x;
        ret_v.y = y / a.y;
        return ret_v;
    }

    simd_vec2 operator+=(const vec2& a) {
        x = x + a.x;
        y = y + a.y;

        return *this;
    }

    simd_vec2 operator-=(const vec2& a) {
        x = x - a.x;
        y = y - a.y;

        return *this;
    }

    simd_vec2 operator*=(const vec2& a) {
        x = x * a.x;
        y = y * a.y;

        return *this;
    }

    simd_vec2 operator/=(const vec2& a) {
        x = x / a.x;
        y = y / a.y;

        return *this;
    }

    simd_vec2 operator+(const batch& a) {
        simd_vec2 ret_v;
        ret_v.x = x + a;
        ret_v.y = y + a;
        return ret_v;
    }

    simd_vec2 operator-(const batch& a) {
        simd_vec2 ret_v;
        ret_v.x = x - a;
        ret_v.y = y - a;
        return ret_v;
    }

    simd_vec2 operator*(const batch& a) {
        simd_vec2 ret_v;
        ret_v.x = x * a;
        ret_v.y = y * a;
        return ret_v;
    }

    simd_vec2 operator/(const batch& a) {
        simd_vec2 ret_v;
        ret_v.x = x / a;
        ret_v.y = y / a;
        return ret_v;
    }

    simd_vec2 operator+=(const batch& a) {
        x = x + a;
        y = y + a;

        return *this;
    }

    simd_vec2 operator-=(const batch& a) {
        x = x - a;
        y = y - a;

        return *this;
    }

    simd_vec2 operator*=(const batch& a) {
        x = x * a;
        y = y * a;

        return *this;
    }

    simd_vec2 operator/=(const batch& a) {
        x = x / a;
        y = y / a;

        return *this;
    }

    simd_vec2 operator+(const float& a) {
        simd_vec2 ret_v;
        ret_v.x = x + a;
        ret_v.y = y + a;
        return ret_v;
    }

    simd_vec2 operator-(const float& a) {
        simd_vec2 ret_v;
        ret_v.x = x - a;
        ret_v.y = y - a;
        return ret_v;
    }

    simd_vec2 operator*(const float& a) {
        simd_vec2 ret_v;
        ret_v.x = x * a;
        ret_v.y = y * a;
        return ret_v;
    }

    simd_vec2 operator/(const float& a) {
        simd_vec2 ret_v;
        ret_v.x = x / a;
        ret_v.y = y / a;
        return ret_v;
    }

    simd_vec2 operator+=(const float& a) {
        x = x + a;
        y = y + a;

        return *this;
    }

    simd_vec2 operator-=(const float& a) {
        x = x - a;
        y = y - a;

        return *this;
    }

    simd_vec2 operator*=(const float& a) {
        x = x * a;
        y = y * a;

        return *this;
    }

    simd_vec2 operator/=(const float& a) {
        x = x / a;
        y = y / a;

        return *this;
    }
    
    batch dot(const simd_vec2& a) {
        return x * a.x + y * a.y;
    }

    batch dot(const vec2& a) {
        return x * a.x + y * a.y;
    }

    void lambda(std::function<vec2(int)> func) {
        alignas(32) float temp_x[N];
        alignas(32) float temp_y[N];

        for(int j = 0; j < N; ++j) {
            vec2 v = func(j);

            temp_x[j] = v.x;
            temp_y[j] = v.y;
        }

        x = batch::load_aligned(temp_x);
        y = batch::load_aligned(temp_y);
    }

    simd_vec2 operator-() {
        simd_vec2 v;
        v.x = -x;
        v.y = -y;

        return v;
    }

    simd_vec2 floor() {
        simd_vec2 v;

        v.x = xsimd::floor(x);
        v.y = xsimd::floor(y);

        return v;
    }

    void normalize() {
        batch size = x * x + y * y;
        size = xsimd::sqrt(size);
        size = 1.0f / size;

        x *= size;
        y *= size;
    }

    simd_vec2 normalize2() {
        batch size = x * x + y * y;
        size = xsimd::sqrt(size);
        size = 1.0f / size;

        simd_vec2 m;

        m.x = x * size;
        m.y = y * size;

        return m;
    }
    
    batch length() {
        batch size = x * x + y * y;
        size = xsimd::sqrt(size);

        return size;
    }
};

simd_vec2 select(xsimd::batch_bool<float>& batch_bool, simd_vec2 a, simd_vec2 b);

struct simd_ivec2 {
    batch_int x;
    batch_int y;

    simd_ivec2 operator+(const simd_ivec2& a) {
        simd_ivec2 ret_v;
        ret_v.x = x + a.x;
        ret_v.y = y + a.y;
        return ret_v;
    }

    simd_ivec2 operator-(const simd_ivec2& a) {
        simd_ivec2 ret_v;
        ret_v.x = x - a.x;
        ret_v.y = y - a.y;
        return ret_v;
    }

    simd_ivec2 operator*(const simd_ivec2& a) {
        simd_ivec2 ret_v;
        ret_v.x = x * a.x;
        ret_v.y = y * a.y;
        return ret_v;
    }

    simd_ivec2 operator/(const simd_ivec2& a) {
        simd_ivec2 ret_v;
        ret_v.x = x / a.x;
        ret_v.y = y / a.y;
        return ret_v;
    }

    simd_ivec2 operator+=(const simd_ivec2& a) {
        x = x + a.x;
        y = y + a.y;

        return *this;
    }

    simd_ivec2 operator-=(const simd_ivec2& a) {
        x = x - a.x;
        y = y - a.y;

        return *this;
    }

    simd_ivec2 operator*=(const simd_ivec2& a) {
        x = x * a.x;
        y = y * a.y;

        return *this;
    }

    simd_ivec2 operator/=(const simd_ivec2& a) {
        x = x / a.x;
        y = y / a.y;

        return *this;
    }

    simd_ivec2 operator=(const simd_ivec2& a) {
        x = a.x;
        y = a.y;

        return *this;
    }

    simd_ivec2 operator+(const ivec2& a) {
        simd_ivec2 ret_v;
        ret_v.x = x + a.x;
        ret_v.y = y + a.y;
        return ret_v;
    }

    simd_ivec2 operator-(const ivec2& a) {
        simd_ivec2 ret_v;
        ret_v.x = x - a.x;
        ret_v.y = y - a.y;
        return ret_v;
    }

    simd_ivec2 operator*(const ivec2& a) {
        simd_ivec2 ret_v;
        ret_v.x = x * a.x;
        ret_v.y = y * a.y;
        return ret_v;
    }

    simd_ivec2 operator/(const ivec2& a) {
        simd_ivec2 ret_v;
        ret_v.x = x / a.x;
        ret_v.y = y / a.y;
        return ret_v;
    }

    simd_ivec2 operator+=(const ivec2& a) {
        x = x + a.x;
        y = y + a.y;

        return *this;
    }

    simd_ivec2 operator-=(const ivec2& a) {
        x = x - a.x;
        y = y - a.y;

        return *this;
    }

    simd_ivec2 operator*=(const ivec2& a) {
        x = x * a.x;
        y = y * a.y;

        return *this;
    }

    simd_ivec2 operator/=(const ivec2& a) {
        x = x / a.x;
        y = y / a.y;

        return *this;
    }

    simd_ivec2 operator+(const int& a) {
        simd_ivec2 ret_v;
        ret_v.x = x + a;
        ret_v.y = y + a;
        return ret_v;
    }

    simd_ivec2 operator-(const int& a) {
        simd_ivec2 ret_v;
        ret_v.x = x - a;
        ret_v.y = y - a;
        return ret_v;
    }

    simd_ivec2 operator*(const int& a) {
        simd_ivec2 ret_v;
        ret_v.x = x * a;
        ret_v.y = y * a;
        return ret_v;
    }

    simd_ivec2 operator/(const float& a) {
        simd_ivec2 ret_v;
        ret_v.x = x / a;
        ret_v.y = y / a;
        return ret_v;
    }

    simd_ivec2 operator+=(const int& a) {
        x = x + a;
        y = y + a;

        return *this;
    }

    simd_ivec2 operator-=(const int& a) {
        x = x - a;
        y = y - a;

        return *this;
    }

    simd_ivec2 operator*=(const int& a) {
        x = x * a;
        y = y * a;

        return *this;
    }

    simd_ivec2 operator/=(const int& a) {
        x = x / a;
        y = y / a;

        return *this;
    }

    void from_float(simd_vec2 v) {
        x = xsimd::batch_cast<int>(v.x);
        y = xsimd::batch_cast<int>(v.y);
    }
};

xsimd::batch_bool<int> bfloat_to_bint(xsimd::batch_bool<float> f);

xsimd::batch_bool<float> bint_to_bfloat(xsimd::batch_bool<int> f);

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