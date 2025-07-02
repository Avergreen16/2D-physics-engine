#pragma once

#include "wrapper.hpp"
#include "ecs.hpp"

struct Collider {
    std::vector<vec2> vertices;
    vec2 radius = vec2(0.0f);

    bool colliding = false;

    float mass;
    float inertia;

    bool allow_gravity = true;
    bool allow_rotation = true;
    bool is_static = false;

    vec2 velocity = vec2(0.0f);
    float angular_velocity = 0.0f;
};

struct Collision_data {
    uint32_t a;
    uint32_t b;

    vec2 pa;
    vec2 pb;

    vec2 normal;

    float prev_lambdaN = 0.0f;
    float prev_lambdaT = 0.0f;
};

struct Collision_constraint {
    Collision_data* d;

    vec2 pa;
    vec2 pb;

    float lambdaN = 0.0f;
    float lambdaT = 0.0f;

    std::vector<float> get_velocities();

    void get_points();

    float get_value();
};

struct Position_constraint {
    uint32_t a = 0xFFFFFFFF;
    uint32_t b = 0xFFFFFFFF;

    vec2 pa;
    vec2 pb;

    vec2 ppa;
    vec2 ppb;

    vec2 dir;

    float lambda = 0.0f;

    void get_points();

    float get_value();
};

struct Rotation_constraint {
    uint32_t a = 0xFFFFFFFF;
    uint32_t b = 0xFFFFFFFF;

    vec2 da;
    vec2 db;

    float lambda = 0.0f;

    float get_value();
};

struct Physics_system : System {
    float physics_step = 0.02f;
    float physics_time = 0.0f;
    uint32_t max_frames = 8;
    uint32_t temporal_iterations = 1;

    std::unordered_map<uint64_t, std::vector<Collision_data>> collision_table;

    std::vector<Position_constraint> position_constraints;
    std::vector<Rotation_constraint> rotation_constraints;

    vec2 gravity_aspect = vec2(1.0f, 1.0f);

    Physics_system();

    static std::optional<Collision_data> collision(Collider& ca, Transform& ta, Collider& cb, Transform& tb);
    
    static bool collision_point(Collider& ca, vec2 point);

    static void transform_vertices(Transform& t, Collider& c, std::vector<vec2>& vertices, vec2 origin);

    static vec2 support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction, mat2 orientation = identity<mat2>());

    void insert_collision(uint64_t a, Collision_data c);

    void solve_constraints(std::vector<Collision_constraint>& constraints);

    static vec2 calculate_inertia(Collider& c);

    static vec4 calculate_bounding_box(Collider& c, Transform& t);

    static std::vector<uint64_t> sweep_and_prune(std::unordered_map<uint32_t, vec4>& input);
    
    static vec2 calculate_point_velocity(Collider& c, vec2 point);

    static float calculate_inverse_mass(Collider& c, Transform& t, vec2 impulse_dir, vec2 point);

    static void apply_impulse(Collider& c, vec2 impulse, vec2 point);

    void physics_loop();

    void call();
};

vec2 get_gravity(vec2 pos);

struct Visualizer_v {
    vec2 pos;
    vec4 color;
};

void get_normal(vec2 a, vec2 b, vec2 r, vec2& normal, vec2& center);

struct Visualizer {
    std::vector<Visualizer_v> points;
    std::vector<Visualizer_v> points_b;
    std::vector<std::vector<Visualizer_v>> lines;
    std::vector<std::vector<Visualizer_v>> triangles;
    uint32_t a = 0xFFFFFFFF;
    uint32_t b = 0xFFFFFFFF;
    int steps = 0;

    void step_collisions();
};

extern Visualizer visualizer;

struct Profiler {
    std::vector<double> times;
    std::vector<std::string> names;
    uint32_t current_pos = 0;
    double prev_time;
    uint32_t iterations = 0;

    void restart() {
        times.clear();
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

extern Profiler profiler;