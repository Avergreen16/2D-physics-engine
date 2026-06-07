#pragma once;
#include <shared_mutex>
#include <queue>
#include <vector>
#include <functional>
#include <optional>
#include <chrono>
#include <thread>
#include <iostream>
#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS
#include "glm\glm.hpp"
#include "glm\gtx\matrix_transform_2d.hpp"
#include "glm\gtx\transform.hpp"
#include "glm\gtx\quaternion.hpp"

using namespace glm;

const std::string integers = "0123456789\x80\x81\x82\x83\x84\x85";  
const std::string integers_letters = "0123456789ABCDEF";

const double hexond_ratio = 86400.0 / 65536.0;

constexpr int PRIME_X = 73856093;
constexpr int PRIME_Y = 19349663;
constexpr int PRIME_Z = 83492791;

double get_time();
double get_absolute_time();
time_t get_time_t();

struct Hash_coord {
    std::size_t operator()(const ivec2& v) const;
    std::size_t operator()(const ivec3& v) const;
    std::size_t operator()(const ivec4& v) const;
};

struct Octree_cell {
    ivec4 id;
    std::shared_ptr<Octree_cell> parent = nullptr;
    std::vector<std::shared_ptr<Octree_cell>> children;

    bool is_leaf = true;
};

std::vector<std::shared_ptr<Octree_cell>> compute_octree(int power, int min_power, vec3 rel_pos, float split_factor, int max_power = -1);
//void octree_insert_cell(std::vector<std::shared_ptr<Octree_cell>>& octree, ivec4 insert, int power);
std::vector<std::shared_ptr<Octree_cell>> compute_octree_with_neighbors(int power, int min_power, vec3 rel_pos, float split_factor, float split_add = 0.0f, int max_power = -1);
std::vector<ivec4> get_children(ivec4 cell);
std::vector<ivec4> get_parents(ivec4 cell);

std::u32string convert_string(std::string str);

struct Thread_pool {
    std::queue<std::unique_ptr<std::function<void()>>> tasks;
    std::shared_mutex task_mutex;
    std::vector<std::thread> threads;

    bool stop = false;

    Thread_pool(int num_threads);

    ~Thread_pool();

    void add_task(std::function<void()> f);
};

struct Time {
    std::chrono::steady_clock::time_point last_time;

    void overwrite();

    Time();

    double get_elapsed_time(bool overwrite = false);
};

struct Profiler {
    std::vector<double> times;
    std::vector<std::string> names;
    uint32_t current_pos = 0;
    double prev_time;
    uint32_t iterations = 0;

    void restart() {
        times.clear();
        names.clear();
        current_pos = 0;
        prev_time = get_time();

        iterations = 0;
    }

    void reset() {
        current_pos = 0;
        prev_time = get_time();

        ++iterations;
    }

    void step(std::string name = "") {
        double current_time = get_time();
        double diff = current_time - prev_time;
        prev_time = current_time;

        std::string new_name = std::to_string(current_pos);
        if(name.size()) {
            new_name = name;
        }

        if(times.size() <= current_pos) {
            times.push_back(diff);
            names.push_back(name);
        } else {
            times[current_pos] += diff;
            names[current_pos] = name;
        }

        //std::cout << name << "\n";

        ++current_pos;
    }   
    void step_name(std::string name = "") {
        double current_time = get_time();
        double diff = current_time - prev_time;
        prev_time = current_time;

        std::string new_name = std::to_string(current_pos);
        if(name.size()) {
            new_name = name;
        }

        uint32_t i = 0;
        for(std::string& n : names) {
            if(n == new_name) break;
            else ++i;
        }

        if(i == names.size()) {
            times.push_back(diff);
            names.push_back(name);
        } else {
            times[i] += diff;
        }

        ++current_pos;
    }  

    void output() {
        double total = 0;
        for(double t : times) total += t;

        double total_time = 0.0f;

        uint32_t number = 0;
        for(double t : times) {
            total_time += t;

            std::cout << names[number] << " : " << t / iterations << " = " << (t / total) * 100 << "%\n";
            ++number;
        }
        
        std::cout << total_time / iterations << "\n";

        std::cout << "\n";
    }
};

std::string to_base(int32_t num, int base, bool use_i2 = false);
std::string to_base(int64_t num, int base, bool use_i2 = false);
std::string to_base(float num, int base, int max_float, bool use_i2 = false);
int from_base(std::string num, int base);

extern Profiler profiler;