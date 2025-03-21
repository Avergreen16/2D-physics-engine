#pragma once

#include "wrapper.hpp"
#include "ecs.hpp"

struct Collider {
    std::vector<vec2> vertices;
    float radius = 0.0f;

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

struct Physics_system : System {
    float physics_step = 0.02f;
    float physics_time = 0.0f;
    uint32_t max_frames = 8;

    std::unordered_map<uint64_t, std::vector<Collision_data>> collision_table;

    std::vector<Position_constraint> position_constraints;

    vec2 gravity = vec2(0.0f, -10.0f);

    Physics_system();

    static std::optional<Collision_data> collision(Collider& ca, Transform& ta, Collider& cb, Transform& tb);
    
    static bool collision_point(Collider& ca, vec2 point);

    static void transform_vertices(Transform& t, Collider& c, std::vector<vec2>& vertices, vec2 origin);

    static vec2 support_func(std::vector<vec2>& vertices, float radius, vec2 direction);

    void insert_collision(uint64_t a, Collision_data c);

    void solve_constraints(std::vector<Collision_constraint>& constraints);

    static vec2 calculate_inertia(Collider& c);

    static vec4 calculate_bounding_box(Collider& c, Transform& t);
    
    static vec2 calculate_point_velocity(Collider& c, vec2 point);

    static float calculate_inverse_mass(Collider& c, Transform& t, vec2 impulse_dir, vec2 point);

    static void apply_impulse(Collider& c, vec2 impulse, vec2 point);

    void physics_loop();

    void call();
};