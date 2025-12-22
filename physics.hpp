#pragma once

#include "wrapper.hpp"
#include "ecs.hpp"
#include "random.hpp"

struct Collider {
    std::vector<vec2> vertices;
    vec2 radius = vec2(0.0f);

    bool colliding = false;

    float mass;
    float inertia = __FLT_MAX__;

    bool allow_gravity = true;
    bool allow_rotation = true;
    bool is_static = false;

    vec2 velocity = vec2(0.0f);
    float angular_velocity = 0.0f;

    vec4 bounding_box;
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
};

struct pos_constraint {
    vec2 a;
    vec2 b;

    vec2 pa;
    vec2 pb;

    vec2 pos_a;
    vec2 pos_b;

    std::vector<vec2> vs;

    std::vector<float> baumgarte;
    std::vector<float> inertia;
    std::vector<float> lambda;
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
    float lambda;
};

struct Constraint {
    uint32_t a = 0xFFFFFFFF;
    uint32_t b = 0xFFFFFFFF;
    
    Collider* ca;
    Transform* ta;
    Collider* cb;
    Transform* tb;

    std::vector<pos_constraint> pos;
    std::vector<rot_constraint> rot;

    void get_points();
    void get_values();
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

    vec2 gravity_aspect = vec2(1.0f, 1.0f);

    std::unordered_set<uint32_t> inserted_sap;
    std::vector<Sap_point> sap_points;

    Physics_system();

    static std::vector<std::vector<Collision_data>> collision(std::vector<Collision_input>& input, bool profiler);
    //static std::vector<std::optional<Collision_data>> collision(std::vector<Collision_input>);
    
    static bool collision_point(Collider& ca, vec2 point);

    static void transform_vertices(Transform& t, Collider& c, std::vector<vec2>& vertices, vec2 origin);

    static vec2 support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction);
    static simd_vec2 support_func(std::vector<simd_vec2>& vertices, batch_int& num_vertices, simd_mat2& matrices, simd_vec2& radius, simd_vec2 direction);

    void insert_collision(Collision_data c);

    void velocity_solve(std::vector<Collision_constraint>& constraints);

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


std::vector<Collision_data> Physics_system::collision(Collision_input& input, bool profiler) {
    std::vector<Collision_data> data;

    std::vector<vec2> a_vertices;
    std::vector<vec2> b_vertices;

    float limit = 0.01;

    transform_vertices(*input.ta, *input.ca, a_vertices, input.ta->position);
    transform_vertices(*input.tb, *input.cb, b_vertices, input.ta->position);

    ////std::cout << "collision started\n";

    Simplex simplex;

    vec2 direction = glm::normalize(b_vertices[0] - a_vertices[0]);
    
    vec2 offset = vec2(direction.y, -direction.x);

    if(glm::dot(offset, direction) > 0.99) {
        offset = vec2(direction.x, -direction.y);
    }

    direction = glm::normalize(direction + offset * 0.1f);

    int iterations = 0;

    bool loop = true;

    while(loop) {
        ++iterations;
        if(iterations > 100) return {};

        if(isnan(direction.x)) direction = vec2(0, 1);
        
        int size = simplex.vertices.size();
        if(size < 3) {
            vec2 point_a = support_func(a_vertices, input.ca->radius, direction, input.ta->orientation);
            vec2 point_b = support_func(b_vertices, input.cb->radius, -direction, input.tb->orientation);

            vec2 point_m = point_a - point_b;

            for(Simplex_vertex& v : simplex.vertices) {
                vec2 difference = point_m - v.m;

                if(glm::length(difference) < limit) return {};
            }

            if(glm::dot(point_m, direction) < limit) return {};

            simplex.vertices.push_back(Simplex_vertex{point_m, point_a, point_b});

            if(size == 0) {
                direction = -glm::normalize(point_m);
            } else if(size == 1) {
                vec2 line_direction = glm::normalize(simplex.vertices[0].m - simplex.vertices[1].m);
                vec2 rel_origin_pos = -simplex.vertices[1].m;

                vec2 closest_point = line_direction * glm::dot(rel_origin_pos, line_direction) + simplex.vertices[1].m;
                direction = glm::normalize(-closest_point);
            }
        } else {
            int n = simplex_contains(vec2(0, 0), simplex.vertices);
            if(n == -1) {
                Polygon p = from_simplex(simplex);

                iterations = 0;

                while(true) {
                    ++iterations;
                    if(iterations > 100) return {};
                    Polygon_return r = p.find_closest_face();

                    if(r.vertices.size() == 0) return {};

                    direction = r.normal;
                    
                    vec2 point_a = support_func(a_vertices, input.ca->radius, direction, input.ta->orientation);
                    vec2 point_b = support_func(b_vertices, input.cb->radius, -direction, input.tb->orientation);

                    vec2 point_m = point_a - point_b;

                    float dist = dot(point_m, r.normal);

                    if(abs(dist - dot(r.vertices[0].m, r.normal)) < limit) {
                        vec2 cp_a = r.vertices[0].a * r.weights.x + r.vertices[1].a * r.weights.y;
                        vec2 cp_b = r.vertices[0].b * r.weights.x + r.vertices[1].b * r.weights.y;
                        
                        vec2 separation_vector = cp_b - cp_a;

                        vec2 collision_normal = normalize(separation_vector);
                        if(isnan(collision_normal.x)) return {};

                        // clipping

                        vec2 pa = support_func(a_vertices, input.ca->radius, -collision_normal, input.ta->orientation);
                        float da0 = FLT_MAX;
                        float da1 = -FLT_MAX;
                        
                        vec2 pb = support_func(b_vertices, input.cb->radius, collision_normal, input.tb->orientation);
                        float db0 = FLT_MAX;
                        float db1 = -FLT_MAX;

                        vec2 sideways = {collision_normal.y, -collision_normal.x};

                        float margin = 0.01f;

                        for(int i = 0; i < a_vertices.size(); ++i) {
                            vec2& v = a_vertices[i];
                            float d = dot(v, sideways);

                            bool c = dot(v - pa, -collision_normal) > -margin;

                            if(c) {
                                if(d < da0) da0 = d;
                                if(d > da1) da1 = d;
                            }
                        }

                        for(int i = 0; i < b_vertices.size(); ++i) {
                            vec2& v = b_vertices[i];
                            float d = dot(v, sideways);

                            bool c = dot(v - pb, collision_normal) > -margin;

                            if(c) {
                                if(d < db0) db0 = d;
                                if(d > db1) db1 = d;
                            }
                        }

                        float c0 = max(da0, db0);
                        float c1 = min(da1, db1);

                        vec2 a2 = pa + sideways * (c0 - dot(pa, sideways));
                        vec2 a3 = pa + sideways * (c1 - dot(pa, sideways));

                        vec2 b2 = pb + sideways * (c0 - dot(pb, sideways));
                        vec2 b3 = pb + sideways * (c1 - dot(pb, sideways));

                        if(da0 <= db1 && db0 <= da1 && false) {
                            Collision_data collision_data;
                            collision_data.collide = true;
                            collision_data.a = 0;
                            collision_data.b = 0;
                            collision_data.pa = a2;
                            collision_data.pb = b2;
                            collision_data.normal = collision_normal;

                            data.push_back(collision_data);
                            
                            collision_data.pa = a3;
                            collision_data.pb = b3;

                            data.push_back(collision_data);
                        } else {
                            Collision_data collision_data;
                            collision_data.collide = true;
                            collision_data.a = 0;
                            collision_data.b = 0;
                            collision_data.pa = cp_a;
                            collision_data.pb = cp_b;
                            collision_data.normal = collision_normal;

                            data.push_back(collision_data);
                        }

                        return data;
                    } else {
                        p.expand({point_m, point_a, point_b});
                    }
                }

                return {};
            } else {
                simplex.vertices.erase(simplex.vertices.begin() + n);
                
                vec2 line_direction = glm::normalize(simplex.vertices[0].m - simplex.vertices[1].m);
                vec2 rel_origin_pos = -simplex.vertices[1].m;

                vec2 closest_point = line_direction * glm::dot(rel_origin_pos, line_direction) + simplex.vertices[1].m;
                direction = glm::normalize(-closest_point);
            }
        }
    }
}