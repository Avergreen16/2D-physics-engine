#pragma once;

#include <array>
#include <vector>
#include <experimental/simd>

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS
#include "glm\glm.hpp"
#include "glm\gtx\matrix_transform_2d.hpp"
#include "glm\gtx\transform.hpp"
#include "glm\gtx\quaternion.hpp"
#include "glm\gtx\orthonormalize.hpp"

using namespace glm;
namespace stdx = std::experimental;

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

struct simd_mat2 {
    simd_vec2 x;
    simd_vec2 y;

    simd_vec2 operator*(const simd_vec2& v) {
        batch bx = v.x * x.x + v.y * y.x;
        batch by = v.x * x.y + v.y * y.y;

        return {bx, by};
    }

    simd_mat2 transpose() {
        simd_mat2 ret;
        ret.x = {x.x, y.x};
        ret.y = {x.y, y.y};

        return ret;
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

vec3 hex_color(uint32_t color);
vec3 hsv_color(float hue, float saturation, float value);

batch_int hash_coords(batch_int x, batch_int y, batch_int z);
int hash_coord(ivec3 v);
int hash_coord(ivec2 v);

struct Hash_coord {
    std::size_t operator()(const ivec3& v) const;
    std::size_t operator()(const ivec2& v) const;
};

struct simd_vec3 {
    batch x;
    batch y;
    batch z;

    simd_vec3 operator+(const simd_vec3& a) {
        simd_vec3 ret_v;
        ret_v.x = x + a.x;
        ret_v.y = y + a.y;
        ret_v.z = z + a.z;
        return ret_v;
    }

    simd_vec3 operator-(const simd_vec3& a) {
        simd_vec3 ret_v;
        ret_v.x = x - a.x;
        ret_v.y = y - a.y;
        ret_v.z = z - a.z;
        return ret_v;
    }

    simd_vec3 operator*(const simd_vec3& a) {
        simd_vec3 ret_v;
        ret_v.x = x * a.x;
        ret_v.y = y * a.y;
        ret_v.z = z * a.z;
        return ret_v;
    }

    simd_vec3 operator/(const simd_vec3& a) {
        simd_vec3 ret_v;
        ret_v.x = x / a.x;
        ret_v.y = y / a.y;
        ret_v.z = z / a.z;
        return ret_v;
    }

    simd_vec3 operator+=(const simd_vec3& a) {
        x = x + a.x;
        y = y + a.y;
        z = z + a.z;

        return *this;
    }

    simd_vec3 operator-=(const simd_vec3& a) {
        x = x - a.x;
        y = y - a.y;
        z = z - a.z;

        return *this;
    }

    simd_vec3 operator*=(const simd_vec3& a) {
        x = x * a.x;
        y = y * a.y;
        z = z * a.z;

        return *this;
    }

    simd_vec3 operator/=(const simd_vec3& a) {
        x = x / a.x;
        y = y / a.y;
        z = z / a.z;

        return *this;
    }

    simd_vec3 operator=(const simd_vec3& a) {
        x = a.x;
        y = a.y;
        z = a.z;

        return *this;
    }

    simd_vec3 operator+=(const batch& a) {
        x = x + a;
        y = y + a;
        z = z + a;

        return *this;
    }

    simd_vec3 operator-=(const batch& a) {
        x = x - a;
        y = y - a;
        z = z - a;

        return *this;
    }

    simd_vec3 operator+(const vec3& a) {
        simd_vec3 ret_v;
        ret_v.x = x + a.x;
        ret_v.y = y + a.y;
        ret_v.z = z + a.z;
        return ret_v;
    }

    simd_vec3 operator-(const vec3& a) {
        simd_vec3 ret_v;
        ret_v.x = x - a.x;
        ret_v.y = y - a.y;
        ret_v.z = z - a.z;
        return ret_v;
    }

    simd_vec3 operator*(const vec3& a) {
        simd_vec3 ret_v;
        ret_v.x = x * a.x;
        ret_v.y = y * a.y;
        ret_v.z = z * a.z;
        return ret_v;
    }

    simd_vec3 operator/(const vec3& a) {
        simd_vec3 ret_v;
        ret_v.x = x / a.x;
        ret_v.y = y / a.y;
        ret_v.z = z / a.z;
        return ret_v;
    }

    simd_vec3 operator+=(const vec3& a) {
        x = x + a.x;
        y = y + a.y;
        z = z + a.z;

        return *this;
    }

    simd_vec3 operator-=(const vec3& a) {
        x = x - a.x;
        y = y - a.y;
        z = z - a.z;

        return *this;
    }

    simd_vec3 operator*=(const vec3& a) {
        x = x * a.x;
        y = y * a.y;
        z = z * a.z;

        return *this;
    }

    simd_vec3 operator/=(const vec3& a) {
        x = x / a.x;
        y = y / a.y;
        z = z / a.z;

        return *this;
    }

    simd_vec3 operator+(const float& a) {
        simd_vec3 ret_v;
        ret_v.x = x + a;
        ret_v.y = y + a;
        ret_v.z = z + a;
        return ret_v;
    }

    simd_vec3 operator-(const float& a) {
        simd_vec3 ret_v;
        ret_v.x = x - a;
        ret_v.y = y - a;
        ret_v.z = z - a;
        return ret_v;
    }

    simd_vec3 operator*(const float& a) {
        simd_vec3 ret_v;
        ret_v.x = x * a;
        ret_v.y = y * a;
        ret_v.z = z * a;
        return ret_v;
    }

    simd_vec3 operator/(const float& a) {
        simd_vec3 ret_v;
        ret_v.x = x / a;
        ret_v.y = y / a;
        ret_v.z = z / a;
        return ret_v;
    }

    simd_vec3 operator+=(const float& a) {
        x = x + a;
        y = y + a;
        z = z + a;

        return *this;
    }

    simd_vec3 operator-=(const float& a) {
        x = x - a;
        y = y - a;
        z = z - a;

        return *this;
    }

    simd_vec3 operator*=(const float& a) {
        x = x * a;
        y = y * a;
        z = z * a;

        return *this;
    }

    simd_vec3 operator/=(const float& a) {
        x = x / a;
        y = y / a;
        z = z / a;

        return *this;
    }
    
    batch dot(const simd_vec3& a) {
        return x * a.x + y * a.y + z * a.z;
    }

    void lambda(std::function<vec3(int)> func) {
        alignas(32) float temp_x[N];
        alignas(32) float temp_y[N];
        alignas(32) float temp_z[N];

        for(int j = 0; j < N; ++j) {
            vec3 v = func(j);

            temp_x[j] = v.x;
            temp_y[j] = v.y;
            temp_z[j] = v.z;
        }

        x = batch::load_aligned(temp_x);
        y = batch::load_aligned(temp_y);
        z = batch::load_aligned(temp_z);
    }

    simd_vec3 floor() {
        simd_vec3 v;

        v.x = xsimd::floor(x);
        v.y = xsimd::floor(y);
        v.z = xsimd::floor(z);

        return v;
    }
};

struct simd_ivec3 {
    batch_int x;
    batch_int y;
    batch_int z;

    simd_ivec3 operator+(const simd_ivec3& a) {
        simd_ivec3 ret_v;
        ret_v.x = x + a.x;
        ret_v.y = y + a.y;
        ret_v.z = z + a.z;
        return ret_v;
    }

    simd_ivec3 operator-(const simd_ivec3& a) {
        simd_ivec3 ret_v;
        ret_v.x = x - a.x;
        ret_v.y = y - a.y;
        ret_v.z = z - a.z;
        return ret_v;
    }

    simd_ivec3 operator*(const simd_ivec3& a) {
        simd_ivec3 ret_v;
        ret_v.x = x * a.x;
        ret_v.y = y * a.y;
        ret_v.z = z * a.z;
        return ret_v;
    }

    simd_ivec3 operator/(const simd_ivec3& a) {
        simd_ivec3 ret_v;
        ret_v.x = x / a.x;
        ret_v.y = y / a.y;
        ret_v.z = z / a.z;
        return ret_v;
    }

    simd_ivec3 operator+=(const simd_ivec3& a) {
        x = x + a.x;
        y = y + a.y;
        z = z + a.z;

        return *this;
    }

    simd_ivec3 operator-=(const simd_ivec3& a) {
        x = x - a.x;
        y = y - a.y;
        z = z - a.z;

        return *this;
    }

    simd_ivec3 operator*=(const simd_ivec3& a) {
        x = x * a.x;
        y = y * a.y;
        z = z * a.z;

        return *this;
    }

    simd_ivec3 operator/=(const simd_ivec3& a) {
        x = x / a.x;
        y = y / a.y;
        z = z / a.z;

        return *this;
    }

    simd_ivec3 operator=(const simd_ivec3& a) {
        x = a.x;
        y = a.y;
        z = a.z;

        return *this;
    }

    simd_ivec3 operator+(const ivec3& a) {
        simd_ivec3 ret_v;
        ret_v.x = x + a.x;
        ret_v.y = y + a.y;
        ret_v.z = z + a.z;
        return ret_v;
    }

    simd_ivec3 operator-(const ivec3& a) {
        simd_ivec3 ret_v;
        ret_v.x = x - a.x;
        ret_v.y = y - a.y;
        ret_v.z = z - a.z;
        return ret_v;
    }

    simd_ivec3 operator*(const ivec3& a) {
        simd_ivec3 ret_v;
        ret_v.x = x * a.x;
        ret_v.y = y * a.y;
        ret_v.z = z * a.z;
        return ret_v;
    }

    simd_ivec3 operator/(const ivec3& a) {
        simd_ivec3 ret_v;
        ret_v.x = x / a.x;
        ret_v.y = y / a.y;
        ret_v.z = z / a.z;
        return ret_v;
    }

    simd_ivec3 operator+=(const ivec3& a) {
        x = x + a.x;
        y = y + a.y;
        z = z + a.z;

        return *this;
    }

    simd_ivec3 operator-=(const ivec3& a) {
        x = x - a.x;
        y = y - a.y;
        z = z - a.z;

        return *this;
    }

    simd_ivec3 operator*=(const ivec3& a) {
        x = x * a.x;
        y = y * a.y;
        z = z * a.z;

        return *this;
    }

    simd_ivec3 operator/=(const ivec3& a) {
        x = x / a.x;
        y = y / a.y;
        z = z / a.z;

        return *this;
    }

    simd_ivec3 operator+(const int& a) {
        simd_ivec3 ret_v;
        ret_v.x = x + a;
        ret_v.y = y + a;
        ret_v.z = z + a;
        return ret_v;
    }

    simd_ivec3 operator-(const int& a) {
        simd_ivec3 ret_v;
        ret_v.x = x - a;
        ret_v.y = y - a;
        ret_v.z = z - a;
        return ret_v;
    }

    simd_ivec3 operator*(const int& a) {
        simd_ivec3 ret_v;
        ret_v.x = x * a;
        ret_v.y = y * a;
        ret_v.z = z * a;
        return ret_v;
    }

    simd_ivec3 operator/(const float& a) {
        simd_ivec3 ret_v;
        ret_v.x = x / a;
        ret_v.y = y / a;
        ret_v.z = z / a;
        return ret_v;
    }

    simd_ivec3 operator+=(const int& a) {
        x = x + a;
        y = y + a;
        z = z + a;

        return *this;
    }

    simd_ivec3 operator-=(const int& a) {
        x = x - a;
        y = y - a;
        z = z - a;

        return *this;
    }

    simd_ivec3 operator*=(const int& a) {
        x = x * a;
        y = y * a;
        z = z * a;

        return *this;
    }

    simd_ivec3 operator/=(const int& a) {
        x = x / a;
        y = y / a;
        z = z / a;

        return *this;
    }

    void from_float(simd_vec3 v) {
        x = xsimd::batch_cast<int>(v.x);
        y = xsimd::batch_cast<int>(v.y);
        z = xsimd::batch_cast<int>(v.z);
    }
};

simd_ivec3 max(simd_ivec3 a, simd_ivec3 b);
simd_ivec3 min(simd_ivec3 a, simd_ivec3 b);

batch lerp(batch a, batch b, batch x);

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

    static float ridged_perlin_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed, float persistance = 0.5f);
    
    static float simplex_noise(glm::vec3 position, float period, uint32_t octaves, uint32_t seed, float persistance = 0.5f);

    static void perlin_noise(float* dst, vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, ivec3 cycle = ivec3(0x7FFFFFFF), float persistance = 0.5f);
    static void voronoi_noise(float* dst, vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, ivec3 cycle = ivec3(0x7FFFFFFF), float persistance = 0.5f);

    static void simplex_noise(float* dst, vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance = 0.5f);
    
    static std::vector<float> ridged_perlin_noise(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance = 0.5f);
    
    static std::vector<float> perlin_noise_normalized(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance = 0.5f);
    
    static std::vector<float> ridged_perlin_noise_normalized(vec3 pos, float period, uint32_t octaves, uint32_t seed, ivec3 size, float diff, float persistance = 0.5f);
    
    void generate_noise(glm::ivec4 index, float* ptr);
};

extern std::array<vec3, 16> perlin_vectors;