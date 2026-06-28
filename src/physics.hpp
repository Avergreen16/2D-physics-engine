#pragma once

#include "wrapper.hpp"
#include "ecs.hpp"
#include "random.hpp"

struct Bounding_box {
    vec2 minimum = vec2(FLT_MAX, FLT_MAX);
    vec2 maximum = vec2(-FLT_MAX, -FLT_MAX);
};

struct BVH_node {
    Bounding_box bounding_box;
    std::vector<uint32_t> children;
};

struct Collision_shape {
    Bounding_box bounding_box;

    //

    std::vector<vec2> vertices;
    vec2 radius = vec2(0.0f);

    float mass;
    float inertia;

    //

    vec2 position = vec2(0.0f);
    mat2 orientation = identity<mat2>();
};

struct Collider {
    Bounding_box bounding_box;
    std::vector<BVH_node> BVH;

    //

    std::vector<Collision_shape> shapes;

    float mass;
    float inertia = __FLT_MAX__;

    vec2 velocity = vec2(0.0f);
    float angular_velocity = 0.0f;

    //

    std::set<uint32_t> non_colliding;

    bool allow_gravity = true;
    bool allow_rotation = true;
    bool is_static = false;

    std::vector<uint32_t> colliding_with;
    std::vector<vec2> colliding_normal;

    //

    void create_bounding_box();
    void create_BVH();
};

struct Soft_body_point {
    vec2 position;
    vec2 velocity;
    float mass;
};

struct Soft_body {
    Bounding_box bounding_box;
    std::vector<BVH_node> BVH;

    std::vector<vec2> targets;
    vec2 target_center;
    mat2 target_ori;
    float target_area;
    float current_area;
    float pressure;

    float inflate = 0.125f;

    std::vector<Soft_body_point> points;

    std::set<uint32_t> non_colliding;
    bool allow_gravity = true;

    void create_BVH();
    void create(std::vector<vec2> vs, vec2 pos, float mass);
    void compute_match();
};

//

struct Collision_data {
    bool collide = false;

    uint32_t a;
    uint32_t b;

    vec2 pa;
    vec2 pb;

    vec2 normal;

    float lambdaN = 0.0f;
    float lambdaT = 0.0f;

    float deltaT = 0.0f;
    float deltaN = 0.0f;
    float sumN = 0.0f;
};

struct col_constraint {
    Collision_data* d;

    vec2 pa;
    vec2 pb;

    vec2 normal;
    vec2 tangent;

    float lambdaN = 0.0f;
    float lambdaT = 0.0f;

    float pos_lambdaN = 0.0f;

    float inertiaNa = 0.0f;
    float inertiaNb = 0.0f;

    float inertiaTa = 0.0f;
    float inertiaTb = 0.0f;

    float baumgarteN;
    float baumgarteT;
};

struct Collision_constraint {
    uint32_t a;
    uint32_t b;

    Collider* ca;
    Soft_body* sa;
    Transform* ta;
    Collider* cb;
    Soft_body* sb;
    Transform* tb;

    std::vector<col_constraint> constraints;

    void get_points();
    void get_value();

    void refresh(col_constraint& c);
    void refresh_C(col_constraint& c);
};

//

struct RS_Collision_data {
    bool collide = false;

    uint32_t a;
    uint32_t b;

    vec2 pa;

    uint32_t pb0;
    uint32_t pb1;
    float blend;
    vec2 offset;

    vec2 normal;
};

struct RS_col_constraint {
    RS_Collision_data* d;

    vec2 pa;
    vec2 pb;

    uint32_t ib0;
    uint32_t ib1;
    float blend;
    vec2 offset;

    vec2 normal;
    vec2 tangent;

    float lambdaN = 0.0f;
    float lambdaT = 0.0f;

    float pos_lambdaN = 0.0f;

    float inertiaNa = 0.0f;
    float inertiaNb = 0.0f;

    float inertiaTa = 0.0f;
    float inertiaTb = 0.0f;

    float baumgarteN;
    float baumgarteT;
};

struct RS_Collision_constraint {
    uint32_t a;
    uint32_t b;

    Collider* ca;
    Transform* ta;
    Soft_body* sb;

    std::vector<RS_col_constraint> constraints;

    void get_points();
    void get_value();
};

//

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

struct pos_constraint {
    vec2 a;
    vec2 b;

    vec2 pa;
    vec2 pb;

    std::vector<vec2> vs;
    std::vector<float> inertia_a;
    std::vector<float> inertia_b;
    std::vector<float> baumgarte;
    std::vector<float> lambda;
    std::vector<float> pos_lambda;

    float compliance = 0.0001f;
    float limit = FLT_MAX;

    bool is_hold = false;
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

    std::vector<pos_constraint> pos;
    std::vector<rot_constraint> rot;

    void get_points();
    void get_values();

    void refresh(pos_constraint& c);
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

struct RS_Collision_input {
    uint32_t a;
    uint32_t b;

    Collider* ca;
    Transform* ta;
    Soft_body* sb;
};

struct input_data {
    Bounding_box bounding_box;
    Transform* transform;
    uint32_t id;
};

enum collision_type{COLLISION_TYPE_EDGE, COLLISION_TYPE_VERTEX};
struct Return_tag {
    collision_type type;
    ivec2 va[2];
    ivec2 vb[2];
};

extern Transform null_transform;
extern uint32_t depth;

struct Physics_system : System {
    bool sim_active = true;

    // parameters
    float fps = 60.0f;
    uint32_t velocity_iterations = 2;
    uint32_t position_iterations = 0;
    uint32_t substeps = 4;
    float contact_sep = 0.02f;
    float static_dist = 0.0625f;
    float penetration_threshold = FLT_MAX;
    uint32_t iteration_threshold = 3;
    float softness_duration = 1.0f;
    /*
    float penetration_threshold = 0.01f;
    uint32_t iteration_threshold = 3;
    float softness_duration = 1.0f;
    */

    float physics_step = 1.0f / fps;
    float sub_dt = physics_step / substeps;
    uint32_t max_frames = 1;
    float physics_time = 0.0f;

    std::unordered_map<uint64_t, std::vector<Collision_data>> collision_table;
    std::unordered_map<uint64_t, std::vector<RS_Collision_data>> RS_collision_table;

    std::vector<Constraint> constraints;
    std::vector<Constraint_distance> constraints_distance;
    std::vector<Collision_constraint> collision_constraints;
    std::vector<RS_Collision_constraint> RS_collision_constraints;

    vec2 gravity_aspect = vec2(1.0f, 1.0f);

    std::unordered_set<uint32_t> inserted_sap;
    std::vector<Sap_point> sap_points;

    Physics_system();

    //static std::vector<std::vector<Collision_data>> collision(std::vector<Collision_input>& input);
    static std::vector<Collision_data> collision(Collision_input& input);
    static std::vector<RS_Collision_data> collision(RS_Collision_input& input);
    static std::vector<Collision_data> collision(Transform& ta, Collision_shape& ca, Transform& tb, Collision_shape& cb, Return_tag& tag);

    static Bounding_box transform(Transform& t, Bounding_box& b);
    static bool collision(Transform& ta, Bounding_box& a, Transform& tb, Bounding_box& b);
    static bool collision(Bounding_box& a, Bounding_box& b);

    static std::vector<uint32_t> traverse_BVH(Transform& ta, std::vector<BVH_node>& ca, Transform& tb, Bounding_box& bb);
    static std::vector<uint64_t> traverse_BVH(Transform& ta, std::vector<BVH_node>& ca, Transform& tb, std::vector<BVH_node>& cb);

    //

    static bool collision_point(std::vector<vec2> vs, vec2 radius, vec2 point);

    static vec2 transform_vertices(Transform& t, Collision_shape& c, std::vector<vec2>& vertices, vec2 origin);

    static vec2 support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction);
    static vec2 support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction, mat2 matrix);

    void insert_collision(Collision_data c);
    void insert_collision(RS_Collision_data c);

    void velocity_solve();

    static vec2 calculate_inertia(Collision_shape& c);
    static vec2 calculate_inertia(Collider& c);

    std::vector<uint64_t> broad_phase(std::vector<input_data>& input);

    static vec2 calculate_point_velocity(Collider* c, vec2 point);
    static vec2 calculate_point_velocity(Soft_body* c, ivec2 ids, float blend);

    static float calculate_inverse_mass(Collider* c, Transform* t, vec2 impulse_dir, vec2 point);

    static void apply_impulse(Collider* c, vec2 impulse, vec2 point);
    static void apply_impulse(Soft_body* c, vec2 impulse, ivec2 ids, float blend);

    void integrate();

    void physics_loop();

    void call();

    std::vector<Collision_data> collide(Transform t, std::vector<vec2> vs, float radius);
};

vec2 get_gravity(vec2 pos);

void get_normal(vec2 a, vec2 b, vec2 r, vec2& normal, vec2& center);
