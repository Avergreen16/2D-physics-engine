#pragma once

#include "wrapper.hpp"
#include "ecs.hpp"
#include "random.hpp"

struct Collider {
    std::vector<vec2> vertices;
    vec2 radius = vec2(0.0f);
    std::set<uint32_t> non_colliding;

    bool colliding = false;

    float mass;
    float inertia = __FLT_MAX__;

    bool allow_gravity = true;
    bool allow_rotation = true;
    bool is_static = false;

    vec2 velocity = vec2(0.0f);
    float angular_velocity = 0.0f;

    vec4 bounding_box;

    bool flag = false;
    bool flag2 = false;
};

struct Collision_data {
    bool collide = false;

    uint32_t a;
    uint32_t b;

    vec2 pa;
    vec2 pb;

    vec2 normal;

    float prev_lambdaN = 0.0f;
    float prev_lambdaT = 0.0f;
};

struct col_constraint {
    Collision_data* d;

    vec2 pa;
    vec2 pb;
    
    vec2 normal;
    vec2 tangent;

    float lambdaN = 0.0f;
    float lambdaT = 0.0f;

    float inertiaN;
    float inertiaT;

    float inertiaNa = 0.0f;
    float inertiaNb = 0.0f;

    float baumgarte;
};

struct Collision_constraint {
    uint32_t a;
    uint32_t b;
    
    Collider* ca;
    Transform* ta;
    Collider* cb;
    Transform* tb;

    std::vector<col_constraint> constraints;

    void get_points();
    void get_value();

    void refresh(col_constraint& c);
};

struct pos_constraint {
    vec2 a;
    vec2 b;

    vec2 pa;
    vec2 pb;

    vec2 pos_a;
    vec2 pos_b;

    float tolerance = 0.0f;

    vec2 target_dir;

    std::vector<vec2> vs;

    std::vector<float> inertia_a;
    std::vector<float> inertia_b;

    std::vector<float> baumgarte;
    std::vector<float> inertia;
    std::vector<float> lambda;
};

struct Constraint_distance {
    uint32_t a = NULL_ENTITY;
    uint32_t b = NULL_ENTITY;

    vec2 pa;
    vec2 pb;

    vec2 pos_a;
    vec2 pos_b;
    
    Collider* ca;
    Transform* ta;
    Collider* cb;
    Transform* tb;

    vec2 jacobian;
    float lambda;
    float inertia;
    float baumgarte;
    
    void get_points();
    void get_values();
};

struct rot_constraint {
    // vectors to be aligned in the space of their object
    vec2 a;
    vec2 b;

    // vectors in world space
    vec2 va;
    vec2 vb;

    float baumgarte;
    float inertia;
    float lambda = 0.0f;
};

struct Constraint {
    uint32_t a = NULL_ENTITY;
    uint32_t b = NULL_ENTITY;
    
    Collider* ca;
    Transform* ta;
    Collider* cb;
    Transform* tb;

    bool is_grab = false;

    std::vector<pos_constraint> pos;
    std::vector<rot_constraint> rot;

    void get_points();
    void get_values();

    void refresh(pos_constraint& c);

    float weight = 1.0f;
};

struct Featherstone_constraint {
    std::vector<uint32_t> entities;
    std::vector<pos_constraint> constraints;
};

struct Sap_point {
    vec2 start;
    vec2 end;
    uint32_t id;
    bool is_start = true;
};

struct Collision_input {
    uint32_t a;
    uint32_t b;

    Collider* ca;
    Transform* ta;
    Collider* cb;
    Transform* tb;
};

struct input_data {
    Collider* collider;
    Transform* transform;
    uint32_t id;
};

struct Physics_system : System {
    float physics_step = 0.02f;
    float physics_time = 0.0f;
    uint32_t max_frames = 1;
    uint32_t temporal_iterations = 1;

    std::unordered_map<uint64_t, std::vector<Collision_data>> collision_table;

    std::vector<Constraint> constraints;
    std::vector<Constraint_distance> constraints_distance;
    std::vector<Featherstone_constraint> constraints_featherstone;

    vec2 gravity_aspect = vec2(1.0f, 1.0f);

    std::unordered_set<uint32_t> inserted_sap;
    std::vector<Sap_point> sap_points;

    Physics_system();

    static std::vector<std::vector<Collision_data>> collision(std::vector<Collision_input>& input, bool profiler);
    static std::vector<Collision_data> collision(Collision_input& input, bool profiler);
    //static std::vector<std::optional<Collision_data>> collision(std::vector<Collision_input>);
    
    static bool collision_point(Collider& ca, vec2 point);

    static void transform_vertices(Transform& t, Collider& c, std::vector<vec2>& vertices, vec2 origin);

    static vec2 support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction);
    static vec2 support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction, mat2 matrix);

    void insert_collision(Collision_data c);

    void velocity_solve(std::vector<Collision_constraint>& constraints);
    void position_solve(std::vector<Collision_constraint>& constraints);

    static vec2 calculate_inertia(Collider& c);

    static vec4 calculate_bounding_box(Collider& c, Transform& t);

    std::vector<uint64_t> broad_phase(std::vector<input_data>& input);
    
    static vec2 calculate_point_velocity(Collider* c, vec2 point);

    static float calculate_inverse_mass(Collider* c, Transform* t, vec2 impulse_dir, vec2 point);

    static void apply_impulse(Collider* c, vec2 impulse, vec2 point);
    static void apply_position(Collider* c, Transform* t, vec2 impulse, vec2 point);

    void physics_loop();

    void call();
};

vec2 get_gravity(vec2 pos);

void get_normal(vec2 a, vec2 b, vec2 r, vec2& normal, vec2& center);

struct Profiler {
    std::vector<double> times;
    std::vector<std::string> names;
    std::vector<int> num_times;
    double prev_time;
    uint32_t iterations = 0;

    void restart();

    void reset();

    void step(std::string name = "");

    void output();
};

extern Profiler profiler;
extern Profiler profiler2;
extern float slop;