#include "physics.hpp"
#include "render.hpp"
#include "input.hpp"
#include "core.hpp"

Physics_system::Physics_system() {
    Signature s = ecs.update_signature<Collider>();
    ecs.update_signature<Transform>(s);
    collectors.push_back(Collector(s));
}

void Physics_system::transform_vertices(Transform& t, Collider& c, std::vector<vec2>& vertices, vec2 origin) {
    vec2 o = t.position - origin;

    for(vec2 vertex : c.vertices) {
        vertex = t.orientation * vertex + o;
        vertices.push_back(vertex);
    }
}

vec2 Physics_system::support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction, mat2 orientation) {
    float max_dot = -__FLT_MAX__;
    vec2 return_vertex;

    for(vec2 v : vertices) {
        if(radius.x == radius.y) v += radius * direction;
        else {
            vec2 dir = transpose(orientation) * direction;

            vec2 rad_dir = vec2(radius.x * radius.x * dir.x, radius.y * radius.y * dir.y) * (1.0f / sqrt(radius.x * radius.x * dir.x * dir.x + radius.y * radius.y * dir.y * dir.y));
            v += rad_dir;
        }
        float dot_v = dot(v, direction);

        if(dot_v > max_dot) {
            max_dot = dot_v;
            return_vertex = v;
        }
    }

    return return_vertex;
}

struct Simplex_vertex {
    vec2 m;
    vec2 a;
    vec2 b;
};

glm::vec2 segment_project(glm::vec2 a, glm::vec2 b, glm::vec2 c, vec2& p) {
    vec2 line_axis = b - a;
    float dist_c = length(line_axis);
    line_axis /= dist_c;

    vec2 normal = vec2(line_axis.y, -line_axis.x);

    vec2 cc = c - a;
    cc = cc - normal * dot(normal, cc);
    cc += a;

    vec2 da = a - cc;
    vec2 db = b - cc;
    if(dot(da, db) > 0.0f) return vec2(-1);

    float dist_a = length(da);
    float dist_b = length(db);

    p = cc;

    return vec2(dist_b / dist_c, dist_a / dist_c);
}


void get_normal(vec2 a, vec2 b, vec2 r, vec2& normal, vec2& center) {
    vec2 v = a - b;

    normal = normalize(vec2(v.y, -v.x));
    if(dot(normal, r - a) > 0) normal = -normal;
    center = (a + b) * 0.5f;
}

struct Simplex {
    std::vector<Simplex_vertex> vertices;

    uint32_t find_closest_face(glm::vec2& weights, glm::vec2& dir) {
        float dist = __FLT_MAX__;
        uint32_t f = -1;
        weights = vec2(-1);

        for(int i = 0; i < 2; ++i) {
            vec2 p;

            vec2 w = segment_project(vertices[(i <= 0) ? 1 : 0].m, vertices[(i <= 1) ? 2 : 1].m, vec2(0.0f), p);

            if(w.x != -1) {
                float length_p = glm::length(p);

                if(length_p < dist) {
                    dir = vertices[(i <= 0) ? 1 : 0].m - vertices[(i <= 1) ? 2 : 1].m;
                    dir = normalize(vec2(dir.y, -dir.x));

                    dist = length_p;
                    f = i;
                    weights = w;
                }
            }
        }

        return f;
    }
};

int simplex_contains(glm::vec2 p, std::vector<Simplex_vertex>& points) {
    vec2 centroid;
    vec2 normal;

    get_normal(points[1].m, points[2].m, points[0].m, normal, centroid);
    float d0 = glm::dot(p - centroid, normal);

    get_normal(points[0].m, points[2].m, points[1].m, normal, centroid);
    float d1 = glm::dot(p - centroid, normal);

    get_normal(points[0].m, points[1].m, points[2].m, normal, centroid);
    float d2 = glm::dot(p - centroid, normal);

    if(d0 > 0 && d0 > max(d1, d2)) return 0;
    if(d1 > 0 && d1 > max(d0, d2)) return 1;
    if(d2 > 0 && d2 > max(d0, d1)) return 2;

    return -1;
}

struct Polygon_return {
    std::vector<Simplex_vertex> vertices;
    vec2 normal;
    vec2 weights = vec2(-1.0f);
};

struct Polygon_edge {
    std::vector<uint32_t> vertices;
    vec2 normal;
};

struct Polygon {
    std::vector<Simplex_vertex> vertices;
    std::vector<Polygon_edge> edges;

    Polygon_return find_closest_face() {
        Polygon_return ret;

        float min_dist = __FLT_MAX__;
        for(int i = 0; i < edges.size(); ++i) {
            Polygon_edge& edge = edges[i];
            uint32_t a = edge.vertices[0];
            uint32_t b = edge.vertices[1];

            Simplex_vertex va = vertices[a];
            Simplex_vertex vb = vertices[b];

            vec2 center;

            vec2 w = segment_project(va.m, vb.m, vec2(0.0f), center);

            if(w.x != -1) {
                float dist = length(center);
                if(dist < min_dist) {
                    min_dist = dist;
                    
                    ret.vertices = {va, vb};
                    vec2 c;
                    get_normal(va.m, vb.m, vec2(0.0f), ret.normal, c);
                    ret.weights = w;
                }
            }
        }

        return ret;
    }

    void insert_edge(uint32_t a, uint32_t b) {
        vec2 pa = vertices[a].m;
        vec2 pb = vertices[b].m;

        vec2 normal;
        vec2 center;
        get_normal(pa, pb, vec2(0.0f), normal, center);

        Polygon_edge e;
        e.vertices = {a, b};
        e.normal = normal;

        edges.push_back(e);
    }

    void expand(Simplex_vertex vertex) {
        uint32_t v_n = vertices.size();
        vertices.push_back(vertex);

        std::vector<uint32_t> edges_seen;
        std::vector<uint32_t> vertices_seen;
        for(int i = 0; i < edges.size(); ++i) {
            Polygon_edge& e = edges[i];

            vec2 diff = vertex.m - vertices[e.vertices[0]].m;

            if(dot(e.normal, diff) > 0.0f) {
                edges_seen.push_back(i);
                vertices_seen.push_back(e.vertices[0]);
                vertices_seen.push_back(e.vertices[1]);
            }
        }
        
        std::sort(edges_seen.begin(), edges_seen.end());

        int i = 0;
        for(uint32_t edge : edges_seen) {
            edges.erase(edges.begin() + edge - i);
            ++i;
        }

        for(uint32_t vertex : vertices_seen) {
            if(std::count(vertices_seen.begin(), vertices_seen.end(), vertex) == 1) {
                uint32_t a = vertex;
                
                insert_edge(a, v_n);
            }
        }
    }
};

Polygon from_simplex(Simplex& s) {
    Polygon p;
    p.vertices = s.vertices;
    
    for(int i = 0; i < 3; ++i) {
        uint32_t a = i;
        uint32_t b = (i + 1) % 3;

        vec2 pa = p.vertices[a].m;
        vec2 pb = p.vertices[b].m;

        vec2 normal;
        vec2 center;
        get_normal(pa, pb, vec2(0.0f), normal, center);

        Polygon_edge e;
        e.vertices = {a, b};
        e.normal = normal;

        p.edges.push_back(e);
    }

    return p;
}

std::optional<Collision_data> Physics_system::collision(Collider& ca, Transform& ta, Collider& cb, Transform& tb) {
    std::vector<vec2> a_vertices;
    std::vector<vec2> b_vertices;

    float limit = 0.00001;

    transform_vertices(ta, ca, a_vertices, ta.position);
    transform_vertices(tb, cb, b_vertices, ta.position);

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
        
        int size = simplex.vertices.size();
        if(size < 3) {
            vec2 point_a = support_func(a_vertices, ca.radius, direction, ta.orientation);
            vec2 point_b = support_func(b_vertices, cb.radius, -direction, tb.orientation);

            vec2 point_m = point_a - point_b;

            for(Simplex_vertex& v : simplex.vertices) {
                vec2 difference = point_m - v.m;

                if(glm::length(difference) < limit) return {};
            }

            if(glm::dot(point_m, direction) < limit * 2) return {};

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
                    
                    vec2 point_a = support_func(a_vertices, ca.radius, direction, ta.orientation);
                    vec2 point_b = support_func(b_vertices, cb.radius, -direction, tb.orientation);

                    vec2 point_m = point_a - point_b;

                    float dist = dot(point_m, r.normal);

                    if(abs(dist - dot(r.vertices[0].m, r.normal)) < limit) {
                        vec2 cp_a = r.vertices[0].a * r.weights.x + r.vertices[1].a * r.weights.y;
                        vec2 cp_b = r.vertices[0].b * r.weights.x + r.vertices[1].b * r.weights.y;
                        
                        vec2 separation_vector = cp_b - cp_a;

                        vec2 collision_normal = normalize(separation_vector);

                        std::vector<Collision_data> v;

                        return Collision_data(0, 0, cp_a, cp_b, collision_normal);
                    } else {
                        p.expand({point_m, point_a, point_b});
                    }
                }

                return Collision_data();
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


bool Physics_system::collision_point(Collider& ca, vec2 point) {
    std::vector<vec2> a_vertices;

    float limit = 0.00001;

    for(vec2 v : ca.vertices) {
        a_vertices.push_back(v - point);
    }
    ////std::cout << "collision started\n";

    Simplex simplex;

    vec2 direction = glm::normalize(a_vertices[0]);
    
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
        
        int size = simplex.vertices.size();
        if(size < 3) {
            vec2 point_a = support_func(a_vertices, ca.radius, direction);

            for(Simplex_vertex& v : simplex.vertices) {
                vec2 difference = point_a - v.m;

                if(glm::length(difference) < limit) return {};
            }

            if(glm::dot(point_a, direction) < limit * 2) return {};

            simplex.vertices.push_back(Simplex_vertex{point_a, vec2(0.0f), vec2(0.0f)});

            if(size == 0) {
                direction = -glm::normalize(point_a);
            } else if(size == 1) {
                vec2 line_direction = glm::normalize(simplex.vertices[0].m - simplex.vertices[1].m);
                vec2 rel_origin_pos = -simplex.vertices[1].m;

                vec2 closest_point = line_direction * glm::dot(rel_origin_pos, line_direction) + simplex.vertices[1].m;
                direction = glm::normalize(-closest_point);
            }
        } else {
            int n = simplex_contains(vec2(0, 0), simplex.vertices);
            if(n == -1) return true;
            else {
                simplex.vertices.erase(simplex.vertices.begin() + n);
                
                vec2 line_direction = glm::normalize(simplex.vertices[0].m - simplex.vertices[1].m);
                vec2 rel_origin_pos = -simplex.vertices[1].m;

                vec2 closest_point = line_direction * glm::dot(rel_origin_pos, line_direction) + simplex.vertices[1].m;
                direction = glm::normalize(-closest_point);
            }
        }
    }
}

void Physics_system::insert_collision(uint64_t a, Collision_data c) {
    if(!collision_table.contains(a)) {
        collision_table.emplace(a, std::vector<Collision_data>());
    }
        
    std::vector<Collision_data>& v = collision_table[a];

    std::vector<uint32_t> delete_values;

    for(int i = v.size() - 1; i >= 0; --i) {
        Collision_data& d = v[i];
        vec2 diff_a = d.pa - c.pa;
        vec2 diff_b = d.pb - c.pb;

        if(length(diff_a) < 0.05f || length(diff_b) < 0.05f) {
            //delete_values.push_back(i);
            return;
        }
    }

    for(uint32_t d : delete_values) {
        v.erase(v.begin() + d);
    }

    v.push_back(c);
}

struct Sap_point {
    vec2 pos;
    vec2 end;
    uint32_t id;
};

bool compare_x(Sap_point& a, Sap_point& b) {
    return a.pos.x < b.pos.x;
};

bool compare_y(Sap_point& a, Sap_point& b) {
    return a.pos.y < b.pos.y;
};

std::vector<uint64_t> Physics_system::sweep_and_prune(std::unordered_map<uint32_t, vec4>& input) {
    std::vector<Sap_point> points;
    
    for(auto& [i, box_i] : input) {
        vec4 v = input[i];

        Sap_point p;
        p.pos = v.xy();
        p.end = v.zw();
        p.id = i;
        points.push_back(p);
    }


    std::unordered_set<uint64_t> xc;
    std::unordered_set<uint64_t> yc;

    std::unordered_set<uint32_t> open_boxes;

    // x
    std::sort(points.begin(), points.end(), compare_x);
    for(int i = 0; i < points.size(); ++i) {
        Sap_point p_i = points[i];
        for(int j = i + 1; j < points.size(); ++j) {
            Sap_point p_j = points[j];

            if(p_j.pos.x < p_i.end.x) {
                uint32_t a = p_i.id;
                uint32_t b = p_j.id;
                if(a > b) {
                    uint32_t temp = a;
                    a = b;
                    b = temp;
                }

                uint64_t c = (uint64_t(a) << 32) | uint64_t(b);

                xc.emplace(c);
            } else break;
        }
    }

    // y
    std::sort(points.begin(), points.end(), compare_y);
    for(int i = 0; i < points.size(); ++i) {
        Sap_point p_i = points[i];
        for(int j = i + 1; j < points.size(); ++j) {
            Sap_point p_j = points[j];

            if(p_j.pos.y < p_i.end.y) {
                uint32_t a = p_i.id;
                uint32_t b = p_j.id;
                if(a > b) {
                    uint32_t temp = a;
                    a = b;
                    b = temp;
                }

                uint64_t c = (uint64_t(a) << 32) | uint64_t(b);

                yc.emplace(c);
            } else break;
        }
    }

    std::vector<uint64_t> collisions;

    for(uint64_t x : xc) {
        if(yc.contains(x)) {
            collisions.push_back(x);
        }
    }

    return collisions;
}

void Physics_system::physics_loop() {
    for(auto& [k, d] : collision_table) {
        for(int i = 0; i < d.size(); ++i) {
            Collision_data& c = d[i];
            
            Transform& at = ecs.get_component<Transform>(c.a);
            Collider& ac = ecs.get_component<Collider>(c.a);
            
            Collision_constraint cc;
            cc.d = &c;
            cc.get_points();

            vec2 distance = cc.pa - cc.pb;
            
            float dot_normal = dot(distance, c.normal);

            float v = length(distance - c.normal * dot_normal);

            if(dot_normal > 0.01 || v > 0.01) {
                d.erase(d.begin() + i);
                --i;
            }
        }
    }

    std::unordered_map<uint32_t, vec4> input;
    for(uint32_t a : collectors[0].entities) {
        Collider& ac = ecs.get_component<Collider>(a);
        Transform& at = ecs.get_component<Transform>(a);

        vec4 bounding_box = calculate_bounding_box(ac, at);

        input.emplace(a, bounding_box);
    }


    Render_system& rs = ecs.get_system<Render_system>();
    Input_system& is = ecs.get_system<Input_system>();
    rs.marker_points.clear();

    for(uint32_t a : collectors[0].entities) {
        Collider& ca = ecs.get_component<Collider>(a);
        Transform& ta = ecs.get_component<Transform>(a);

        ca.colliding = false;
    }

    std::vector<uint64_t> collisions = sweep_and_prune(input);
    
    for(uint64_t i : collisions) {
        uint32_t a = i & 0xFFFFFFFF;
        uint32_t b = i >> 32;
        
        Collider& ca = ecs.get_component<Collider>(a);
        Transform& ta = ecs.get_component<Transform>(a);

        Collider& cb = ecs.get_component<Collider>(b);
        Transform& tb = ecs.get_component<Transform>(b);

        std::optional<Collision_data> c = collision(ca, ta, cb, tb);

        if(c.has_value()) {
            ca.colliding = true;
            cb.colliding = true;

            bool insert = true;

            if(cb.is_static) {
                if(ca.is_static) insert = false;
                else {
                    c->pa = transpose(ta.orientation) * c->pa;
                    c->pb = ta.position + c->pb;
                    c->a = a;
                    c->b = 0xFFFFFFFF;
                }
            } else if(ca.is_static) {
                c->a = b;
                c->b = 0xFFFFFFFF;
                
                vec2 temp = c->pa;
                c->pa = transpose(tb.orientation) * (c->pb + (ta.position - tb.position));
                c->pb = ta.position + temp;
            } else {
                c->pa = transpose(ta.orientation) * (c->pa);
                c->pb = transpose(tb.orientation) * (c->pb + (ta.position - tb.position));
                c->a = a;
                c->b = b;
            }

            if(insert) insert_collision(i, c.value());
        }
    }

    
    std::vector<Collision_constraint> collision_constraints;

    for(auto& [k, d] : collision_table) {
        for(int i = 0; i < d.size(); ++i) {
            Collision_data& c = d[i];
            
            Transform& at = ecs.get_component<Transform>(c.a);
            Collider& ac = ecs.get_component<Collider>(c.a);
            
            Collision_constraint cc;
            cc.d = &c;

            collision_constraints.push_back(cc);
        }
    }
    float partial_step = physics_step / temporal_iterations;

    for(int i = 0; i < temporal_iterations; ++i) {
        for(uint32_t a : collectors[0].entities) {
            Collider& ca = ecs.get_component<Collider>(a);
            Transform& ta = ecs.get_component<Transform>(a);

            if(ca.allow_gravity && !ca.is_static) {
                vec2 g = get_gravity(ta.position) * -20.0f;

                ca.velocity += g * partial_step;
            }
        }

        solve_constraints(collision_constraints);

        for(uint32_t a : collectors[0].entities) {
            Collider& ca = ecs.get_component<Collider>(a);
            Transform& ta = ecs.get_component<Transform>(a);

            if(!ca.is_static) {
                ta.position += ca.velocity * partial_step;
                mat2 rotation = rotate(ca.angular_velocity * partial_step, vec3(0, 0, 1));
                ta.orientation = rotation * ta.orientation;
            }
        }
    }
}

void Physics_system::call() {
    physics_time += core.delta_time;

    uint32_t frames = 0;

    while(physics_time >= physics_step) {
        //physics_loop();
        physics_time -= physics_step;
        ++frames;

        if(frames >= max_frames) {
            physics_time = 0;
            break;
        }
    }
}


void Physics_system::apply_impulse(Collider& c, vec2 impulse, vec2 point) {
    c.velocity += impulse / c.mass;
    c.angular_velocity += cross(vec3(point, 0.0f), vec3(impulse, 0.0f)).z / c.inertia;
}

void Physics_system::solve_constraints(std::vector<Collision_constraint>& constraints) {
    int iterations = 4;
    float spring = 0.75f;
    float softness = 0.05f;

    for(Position_constraint& data : position_constraints) {
        data.get_points();
        data.lambda = 0.0f;

        /*vec2 impulse = data.dir * data.lambda;

        Collider& ca = ecs.get_component<Collider>(data.a);
        Transform& ta = ecs.get_component<Transform>(data.a);
        apply_impulse(ca, impulse, data.ppa - ta.position);

        if(data.b != 0xFFFFFFFF) {
            Collider& cb = ecs.get_component<Collider>(data.b);
            Transform& tb = ecs.get_component<Transform>(data.b);
            apply_impulse(cb, -impulse, data.ppb - tb.position);
        }*/
    }

    for(Rotation_constraint& data : rotation_constraints) {
        float twirl = data.lambda;
        data.lambda = 0.0f;

        Collider& ca = ecs.get_component<Collider>(data.a);
        Transform& ta = ecs.get_component<Transform>(data.a);

        Collider& cb = ecs.get_component<Collider>(data.b);
        Transform& tb = ecs.get_component<Transform>(data.b);

        //ca.angular_velocity += twirl / ca.inertia;
        //cb.angular_velocity -= twirl / cb.inertia;
    }

    for(Collision_constraint& data : constraints) {
        data.get_points();
        data.lambdaN = data.d->prev_lambdaN;
        data.lambdaT = data.d->prev_lambdaT;

        if(data.d->b == 0xFFFFFFFF) {
            Collider& ca = ecs.get_component<Collider>(data.d->a);
            Transform& ta = ecs.get_component<Transform>(data.d->a);

            vec2 impulse = data.d->normal * data.lambdaN;

            apply_impulse(ca, impulse, data.pa - ta.position);

            vec2 tangent_vector = vec2(data.d->normal.y, -data.d->normal.x);

            vec2 friction_impulse = tangent_vector * data.lambdaT;

            apply_impulse(ca, friction_impulse, data.pa - ta.position);
        } else {
            Collider& ca = ecs.get_component<Collider>(data.d->a);
            Transform& ta = ecs.get_component<Transform>(data.d->a);

            Collider& cb = ecs.get_component<Collider>(data.d->b);
            Transform& tb = ecs.get_component<Transform>(data.d->b);

            vec2 impulse = data.d->normal * data.lambdaN;

            apply_impulse(ca, impulse, data.pa - ta.position);
            apply_impulse(cb, -impulse, data.pb - tb.position);

            // friction
            
            vec2 tangent_vector = vec2(data.d->normal.y, -data.d->normal.x);

            vec2 friction_impulse = tangent_vector * data.lambdaT;

            apply_impulse(ca, friction_impulse, data.pa - ta.position);
            apply_impulse(cb, -friction_impulse, data.pb - tb.position);
        }
    }

    for(int i = 0; i < iterations; ++i) {
        for(Position_constraint& data : position_constraints) {
            Collider& ca = ecs.get_component<Collider>(data.a);
            Transform& ta = ecs.get_component<Transform>(data.a);

            float bg = data.get_value() * spring / physics_step;

            float inverse_mass = calculate_inverse_mass(ca, ta, data.dir, data.ppa - ta.position);

            vec2 velocity = calculate_point_velocity(ca, data.ppa - ta.position);

            if(data.b == 0xFFFFFFFF) {
                float L = -dot(velocity, data.dir) + bg;
                L /= inverse_mass;
                L -= softness * data.lambda;
                float new_lambda = data.lambda + L;
                data.lambda = new_lambda;

                vec2 impulse = data.dir * L;

                apply_impulse(ca, impulse, data.ppa - ta.position);
            } else {
                Collider& cb = ecs.get_component<Collider>(data.b);
                Transform& tb = ecs.get_component<Transform>(data.b);

                inverse_mass += calculate_inverse_mass(cb, tb, data.dir, data.ppb - tb.position);
                
                velocity -= calculate_point_velocity(cb, data.ppb - tb.position);

                float L = -dot(velocity, data.dir) + bg;
                L /= inverse_mass;
                L -= softness * data.lambda;
                float new_lambda = data.lambda + L;
                data.lambda = new_lambda;

                vec2 impulse = data.dir * L;

                apply_impulse(ca, impulse, data.ppa - ta.position);
                apply_impulse(cb, -impulse, data.ppb - tb.position);
            }
        }
        
        for(Rotation_constraint& data : rotation_constraints) {
            Collider& ca = ecs.get_component<Collider>(data.a);
            Transform& ta = ecs.get_component<Transform>(data.a);
            
            Collider& cb = ecs.get_component<Collider>(data.b);
            Transform& tb = ecs.get_component<Transform>(data.b);

            float angular_delta = data.get_value();
            float inverse_mass = 1.0f / ca.inertia + (1.0f / cb.inertia);


            float bg = angular_delta * spring / physics_step;

            float angular_velocity = ca.angular_velocity - cb.angular_velocity;

            float L = -angular_velocity + bg;
            L /= inverse_mass;
            //L -= softness * data.lambda;
            float new_lambda = data.lambda + L;
            data.lambda = new_lambda;

            ca.angular_velocity += L / ca.inertia;
            cb.angular_velocity -= L / cb.inertia;
        }
        
        for(Collision_constraint& data : constraints) {
            Collider& ca = ecs.get_component<Collider>(data.d->a);
            Transform& ta = ecs.get_component<Transform>(data.d->a);

            float total_inertia = 0.0f;

            total_inertia += calculate_inverse_mass(ca, ta, data.d->normal, data.pa - ta.position);
            vec2 velocity = calculate_point_velocity(ca, data.pa - ta.position);

            float diff = data.get_value();
            diff = diff * spring / physics_step;

            if(data.d->b == 0xFFFFFFFF) {
                float v = dot(velocity, data.d->normal);

                float L = -v - diff;
                L /= total_inertia;
                L -= softness * data.lambdaN;
                
                vec2 limits = vec2(0.0f, __FLT_MAX__);

                float new_lambda = data.lambdaN + L;
                new_lambda = clamp(new_lambda, limits.x, limits.y);
                L = new_lambda - data.lambdaN;
                data.lambdaN = new_lambda;

                vec2 impulse = data.d->normal * L;

                apply_impulse(ca, impulse, data.pa - ta.position);


                // friction

                float normal_magnitude = length(impulse);

                velocity = calculate_point_velocity(ca, data.pa - ta.position);
                vec2 tangent_vector = vec2(data.d->normal.y, -data.d->normal.x);
                float tangent_velocity = dot(velocity, tangent_vector);

                
                float inverse_mass = calculate_inverse_mass(ca, ta, tangent_vector, data.pa - ta.position);

                float mu = 0.9f;

                float max_friction = abs(mu * data.lambdaN);

                float new_lambdaT = data.lambdaT - tangent_velocity / inverse_mass;
                new_lambdaT = clamp(new_lambdaT, -max_friction, max_friction);
                L = new_lambdaT - data.lambdaT;
                data.lambdaT = new_lambdaT;

                float Pt = L;

                vec2 friction_impulse = tangent_vector * Pt;

                apply_impulse(ca, friction_impulse, data.pa - ta.position);
            } else {
                Collider& cb = ecs.get_component<Collider>(data.d->b);
                Transform& tb = ecs.get_component<Transform>(data.d->b);

                total_inertia += calculate_inverse_mass(cb, tb, data.d->normal, data.pb - tb.position);

                velocity -= calculate_point_velocity(cb, data.pb - tb.position);

                float v = dot(velocity, data.d->normal);

                float L = -v - diff;
                L /= total_inertia;
                L -= softness * data.lambdaN;
                
                vec2 limits = vec2(0.0f, __FLT_MAX__);

                float new_lambda = data.lambdaN + L;
                new_lambda = clamp(new_lambda, limits.x, limits.y);
                L = new_lambda - data.lambdaN;
                data.lambdaN = new_lambda;

                vec2 impulse = data.d->normal * L;
                
                apply_impulse(ca, impulse, data.pa - ta.position);
                apply_impulse(cb, -impulse, data.pb - tb.position);

                // friction

                float normal_magnitude = length(impulse);

                velocity = calculate_point_velocity(ca, data.pa - ta.position) - calculate_point_velocity(cb, data.pb - tb.position);
                vec2 tangent_vector = vec2(data.d->normal.y, -data.d->normal.x);
                float tangent_velocity = dot(velocity, tangent_vector);
                
                float inverse_mass = calculate_inverse_mass(ca, ta, tangent_vector, data.pa - ta.position) + calculate_inverse_mass(cb, tb, tangent_vector, data.pb - tb.position);
                
                float mu = 0.9f;

                float max_friction = abs(mu * data.lambdaN);

                float new_lambdaT = data.lambdaT - tangent_velocity / inverse_mass;
                new_lambdaT = clamp(new_lambdaT, -max_friction, max_friction);
                L = new_lambdaT - data.lambdaT;
                data.lambdaT = new_lambdaT;

                float Pt = L;

                vec2 friction_impulse = tangent_vector * Pt;

                apply_impulse(ca, friction_impulse, data.pa - ta.position);
                apply_impulse(cb, -friction_impulse, data.pb - tb.position);
            }
        }
    }
    
    for(Collision_constraint& c : constraints) {
        c.d->prev_lambdaN = c.lambdaN;
        c.d->prev_lambdaT = c.lambdaT;
    }
}

vec2 Physics_system::calculate_inertia(Collider& c) {
    ivec2 num_points = ivec2(25);

    Transform temp;
    temp.position = vec2(0.0f);
    temp.orientation = identity<mat2>();

    vec4 range = calculate_bounding_box(c, temp);
    range.z -= range.x;
    range.w -= range.y;

    vec2 size = range.zw() / vec2(num_points);

    vec2 accum = vec2(0.0f);
    float number = 0.0f;

    float inertia = 0.0f;

    float single_inertia = 1.0f / 12 * (size.x * size.x + size.y * size.y);

    for(int y = 0; y < num_points.y; ++y) {
        for(int x = 0; x < num_points.x; ++x) {
            vec2 point = range.xy() + vec2(x + 0.5f, y + 0.5f) / vec2(num_points) * range.zw();

            bool is_inside = collision_point(c, point);

            if(is_inside) {
                ++number;
                accum += point;

                float dist = length(point);
                inertia += single_inertia + (dist * dist);
            }
        }
    }

    vec2 center = accum / number;
    inertia /= number;

    float dist = length(center);

    inertia -= dist * dist;

    inertia *= c.mass;

    //inertia *= 10;

    std::cout<< "b" << inertia << "\n";

    c.inertia = inertia;

    for(vec2& v : c.vertices) v -= center;

    return center;
}

vec2 Physics_system::calculate_point_velocity(Collider& c, vec2 point) {
    vec2 velocity = c.velocity;
    float angular_velocity = c.angular_velocity;
    vec2 linear_velocity = vec2(cross(vec3(0.0f, 0.0f, angular_velocity), vec3(point, 0.0f)));

    velocity += linear_velocity;

    return velocity;
}

float Physics_system::calculate_inverse_mass(Collider& c, Transform& t, vec2 impulse_dir, vec2 point) {
    float inverse_mass = 1.0f / c.mass;

    if(c.allow_rotation) {
        float torque_per_unit = length(cross(vec3(point, 0.0f), vec3(impulse_dir, 0.0f)));

        float angular_velocity = torque_per_unit / c.inertia;

        vec2 linear_velocity = vec2(cross(vec3(0.0f, 0.0f, angular_velocity), vec3(point, 0.0f)));

        float angular_inertia = dot(linear_velocity, impulse_dir);

        inverse_mass += abs(angular_inertia);
    }

    return inverse_mass;
}

vec4 Physics_system::calculate_bounding_box(Collider& c, Transform& t) {
    vec2 minimum = vec2(__FLT_MAX__);
    vec2 maximum = vec2(-__FLT_MAX__);

    float radius = max(c.radius.x, c.radius.y);

    for(vec2 v : c.vertices) {
        vec2 vv = t.orientation * v + t.position;

        minimum.x = glm::min(minimum.x, vv.x - radius);
        minimum.y = glm::min(minimum.y, vv.y - radius);
        maximum.x = glm::max(maximum.x, vv.x + radius);
        maximum.y = glm::max(maximum.y, vv.y + radius);
    }

    vec4 range = vec4(minimum, maximum);

    return range;
}

std::vector<float> Collision_constraint::get_velocities() {
    Collider& ca = ecs.get_component<Collider>(d->a);
    std::vector<float> ret = {
        ca.velocity.x, ca.velocity.y, ca.angular_velocity
    };

    if(d->b != 0xFFFFFFFF) {
        Collider& cb = ecs.get_component<Collider>(d->b);
        ret.push_back(cb.velocity.x);
        ret.push_back(cb.velocity.y);
        ret.push_back(cb.angular_velocity);
    }

    return ret;
}

void Collision_constraint::get_points() {
    Transform& at = ecs.get_component<Transform>(d->a);
    Collider& ac = ecs.get_component<Collider>(d->a);

    vec2 point_a = at.orientation * d->pa + at.position;

    pa = point_a;

    if(d->b == 0xFFFFFFFF) {
        pb = d->pb;
    } else {
        Transform& bt = ecs.get_component<Transform>(d->b);
        Collider& bc = ecs.get_component<Collider>(d->b);

        vec2 point_b = bt.orientation * d->pb + bt.position;

        pb = point_b;
    }
}

float Collision_constraint::get_value() {
    vec2 diff = pa - pb;
    float dd = dot(diff, d->normal);
    return dd;
}

float Rotation_constraint::get_value() {
    Transform& at = ecs.get_component<Transform>(a);
    Collider& ac = ecs.get_component<Collider>(a);

    if(b == 0xFFFFFFFF) {
        vec2 dir_a = at.orientation * da;
        vec2 perp = vec2(-dir_a.y, dir_a.x);

        float angle = acos(clamp(dot(dir_a, db), -1.0f, 1.0f));
        
        vec3 cr = cross(vec3(dir_a, 0), vec3(db, 0));
        if(cr.z < 0) angle = -angle;

        return angle;
    } else {
        Transform& bt = ecs.get_component<Transform>(b);
        Collider& bc = ecs.get_component<Collider>(b);

        vec2 dir_a = at.orientation * da;

        vec2 dir_b = bt.orientation * db;

        float angle = acos(clamp(dot(dir_a, dir_b), -1.0f, 1.0f));
        
        vec3 cr = cross(vec3(dir_a, 0), vec3(dir_b, 0));
        if(cr.z < 0) angle = -angle;

        return angle;
    }
}

void Position_constraint::get_points() {
    Transform& at = ecs.get_component<Transform>(a);
    Collider& ac = ecs.get_component<Collider>(a);

    vec2 point_a = at.orientation * pa + at.position;

    ppa = point_a;

    if(b == 0xFFFFFFFF) {
        ppb = pb;
    } else {
        Transform& bt = ecs.get_component<Transform>(b);
        Collider& bc = ecs.get_component<Collider>(b);

        vec2 point_b = bt.orientation * pb + bt.position;

        ppb = point_b;
    }
}

float Position_constraint::get_value() {
    vec2 diff = ppb - ppa;
    float dd = dot(diff, dir);
    return dd;
}

vec2 get_gravity(vec2 pos) {
    Physics_system& ps = ecs.get_system<Physics_system>();

    vec2 rel_pos = pos;
    return normalize(vec2(rel_pos.x / (ps.gravity_aspect.x * ps.gravity_aspect.x), rel_pos.y / (ps.gravity_aspect.y * ps.gravity_aspect.y)));
}

void Visualizer::step_collisions() {
    Physics_system& ps = ecs.get_system<Physics_system>();

    points.clear();
    points_b.clear();
    lines.clear();
    triangles.clear();

    Collider& ca = ecs.get_component<Collider>(a);
    Transform& ta = ecs.get_component<Transform>(a);
    Collider& cb = ecs.get_component<Collider>(b);
    Transform& tb = ecs.get_component<Transform>(b);


    std::vector<vec2> a_vertices;
    std::vector<vec2> b_vertices;

    float limit = 0.00001;

    ps.transform_vertices(ta, ca, a_vertices, ta.position);
    ps.transform_vertices(tb, cb, b_vertices, ta.position);

    for(int i = 0; i < a_vertices.size(); ++i) {
        vec2 a_v = a_vertices[i];
        for(int j = 0; j < b_vertices.size(); ++j) {
            vec2 b_v = b_vertices[j];

            vec2 diff = a_v - b_v;

            points_b.push_back(Visualizer_v(diff, vec4(1.0f, 0.25f, 0.25f, 0.25f)));
        }   
    }

    ////std::cout << "collision started\n";

    uint32_t step = -1;

    Simplex simplex;

    vec2 direction = glm::normalize(b_vertices[0] - a_vertices[0]);
    
    /*
    vec2 offset = vec2(direction.y, -direction.x);

    if(glm::dot(offset, direction) > 0.99) {
        offset = vec2(direction.x, -direction.y);
    }

    direction = glm::normalize(direction + offset * 0.1f);*/

    int iterations = 0;

    bool loop = true;
    bool distance_check = false;

    while(loop) {
        ++step;
        if(step >= steps) {
            for(int i = 0; i < a_vertices.size(); ++i) {
                vec2 a_v = a_vertices[i];
                for(int j = 0; j < b_vertices.size(); ++j) {
                    vec2 b_v = b_vertices[j];

                    vec2 diff = a_v - b_v;

                    vec4 color = vec4(1.0f);

                    points_b.push_back(Visualizer_v(diff, color));
                }   
            }

            for(Simplex_vertex v : simplex.vertices) {
                vec4 color = vec4(0.25f, 0.25f, 1.0f, 1.0f);

                points.push_back(Visualizer_v(v.m, color));
            }

            if(simplex.vertices.size() == 1) {
                lines.push_back({Visualizer_v(simplex.vertices[0].m, vec4(1.0f, 1.0f, 0.25f, 1.0f)), Visualizer_v(simplex.vertices[0].m + direction * 0.125f, vec4(1.0f, 1.0f, 0.25f, 1.0f))});
            } else if(simplex.vertices.size() == 2) {
                lines.push_back({Visualizer_v(simplex.vertices[0].m, vec4(0.25f, 1.0f, 1.0f, 1.0f)), Visualizer_v(simplex.vertices[1].m, vec4(0.25f, 1.0f, 1.0f, 1.0f))});
                vec2 avg = simplex.vertices[0].m + simplex.vertices[1].m;
                avg /= 2.0f;

                lines.push_back({Visualizer_v(avg, vec4(1.0f, 1.0f, 0.25f, 1.0f)), Visualizer_v(avg + direction * 0.125f, vec4(1.0f, 1.0f, 0.25f, 1.0f))});
            } else if(simplex.vertices.size() == 3) {
                triangles.push_back({Visualizer_v(simplex.vertices[0].m, vec4(0.25f, 1.0f, 1.0f, 0.5f)), Visualizer_v(simplex.vertices[1].m, vec4(0.25f, 1.0f, 1.0f, 0.5f)), Visualizer_v(simplex.vertices[2].m, vec4(0.25f, 1.0f, 1.0f, 0.5f))});
            }
            goto exit_flag;
        }

        ++iterations;
        //if(iterations > 100) return {};
        
        int size = simplex.vertices.size();
        if(size < 3) {
            vec2 point_a = ps.support_func(a_vertices, ca.radius, direction, ta.orientation);
            vec2 point_b = ps.support_func(b_vertices, cb.radius, -direction, tb.orientation);

            vec2 point_m = point_a - point_b;


            ++step;
            if(step >= steps) {
                for(int i = 0; i < a_vertices.size(); ++i) {
                    vec2 a_v = a_vertices[i];
                    for(int j = 0; j < b_vertices.size(); ++j) {
                        vec2 b_v = b_vertices[j];

                        vec2 diff = a_v - b_v;

                        vec4 color = vec4(1.0f);

                        points_b.push_back(Visualizer_v(diff, color));
                    }   
                }

                vec4 color = vec4(0.25f, 0.25f, 1.0f, 1.0f);

                for(Simplex_vertex v : simplex.vertices) {
                    points.push_back(Visualizer_v(v.m, color));
                }

                points.push_back(Visualizer_v(point_a + ta.position, color));
                points.push_back(Visualizer_v(point_b + ta.position, color));
                points.push_back(Visualizer_v(point_m, color));

                if(simplex.vertices.size() == 1) {
                    lines.push_back({Visualizer_v(simplex.vertices[0].m, vec4(1.0f, 1.0f, 0.25f, 1.0f)), Visualizer_v(simplex.vertices[0].m + direction * 0.125f, vec4(1.0f, 1.0f, 0.25f, 1.0f))});
                } else if(simplex.vertices.size() == 2) {
                    lines.push_back({Visualizer_v(simplex.vertices[0].m, vec4(0.25f, 1.0f, 1.0f, 1.0f)), Visualizer_v(simplex.vertices[1].m, vec4(0.25f, 1.0f, 1.0f, 1.0f))});
                    vec2 avg = simplex.vertices[0].m + simplex.vertices[1].m;
                    avg /= 2.0f;

                    lines.push_back({Visualizer_v(avg, vec4(1.0f, 1.0f, 0.25f, 1.0f)), Visualizer_v(avg + direction * 0.125f, vec4(1.0f, 1.0f, 0.25f, 1.0f))});
                } else if(simplex.vertices.size() == 3) {
                    triangles.push_back({Visualizer_v(simplex.vertices[0].m, vec4(0.25f, 1.0f, 1.0f, 0.5f)), Visualizer_v(simplex.vertices[1].m, vec4(0.25f, 1.0f, 1.0f, 0.5f)), Visualizer_v(simplex.vertices[2].m, vec4(0.25f, 1.0f, 1.0f, 0.5f))});
                }
                goto exit_flag;
            }


            for(Simplex_vertex& v : simplex.vertices) {
                vec2 difference = point_m - v.m;

                if(glm::length(difference) < limit) goto exit_distance;
            }

            //if(glm::dot(point_m, direction) < limit) goto exit_distance;

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
                    ++step;
                    if(step >= steps || steps == -1) {
                        for(int i = 0; i < a_vertices.size(); ++i) {
                            vec2 a_v = a_vertices[i];
                            for(int j = 0; j < b_vertices.size(); ++j) {
                                vec2 b_v = b_vertices[j];

                                vec2 diff = a_v - b_v;

                                vec4 color = vec4(1.0f);

                                points_b.push_back(Visualizer_v(diff, color));
                            }   
                        }

                        vec2 avg = vec2(0.0f);

                        for(Simplex_vertex& v : p.vertices) {
                            avg += v.m;
                            
                            vec4 color = vec4(0.25f, 0.25f, 1.0f, 1.0f);

                            points.push_back(Visualizer_v(v.m, color));
                        }
                        avg /= p.vertices.size();

                        for(auto l : p.edges) {
                            vec2 va = p.vertices[l.vertices[0]].m;
                            vec2 vb = p.vertices[l.vertices[1]].m;
                            
                            triangles.push_back({Visualizer_v(va, vec4(1.0f, 1.0f, 0.25f, 0.5f)), Visualizer_v(vb, vec4(1.0f, 1.0f, 0.25f, 0.5f)), Visualizer_v(avg, vec4(1.0f, 1.0f, 0.25f, 0.5f))});
                        }

                        goto exit_flag;
                    }
                    
                    ++iterations;
                    Polygon_return r = p.find_closest_face();

                    if(r.vertices.size() == 0) goto exit_return_false;

                    direction = r.normal;
                    
                    vec2 point_a = ps.support_func(a_vertices, ca.radius, direction, ta.orientation);
                    vec2 point_b = ps.support_func(b_vertices, cb.radius, -direction, tb.orientation);

                    vec2 point_m = point_a - point_b;

                    ++step;
                    if(step >= steps) {
                        for(int i = 0; i < a_vertices.size(); ++i) {
                            vec2 a_v = a_vertices[i];
                            for(int j = 0; j < b_vertices.size(); ++j) {
                                vec2 b_v = b_vertices[j];

                                vec2 diff = a_v - b_v;

                                vec4 color = vec4(1.0f);

                                points_b.push_back(Visualizer_v(diff, color));
                            }   
                        }

                        vec2 avg = vec2(0.0f);

                        for(Simplex_vertex& v : p.vertices) {
                            avg += v.m;
                            
                            vec4 color = vec4(0.25f, 0.25f, 1.0f, 1.0f);

                            points.push_back(Visualizer_v(v.m, color));
                        }
                        avg /= p.vertices.size();

                        for(auto l : p.edges) {
                            vec2 va = p.vertices[l.vertices[0]].m;
                            vec2 vb = p.vertices[l.vertices[1]].m;
                            
                            triangles.push_back({Visualizer_v(va, vec4(1.0f, 1.0f, 0.25f, 0.5f)), Visualizer_v(vb, vec4(1.0f, 1.0f, 0.25f, 0.5f)), Visualizer_v(avg, vec4(1.0f, 1.0f, 0.25f, 0.5f))});
                        }
                            
                        lines.push_back({Visualizer_v(r.vertices[0].m, vec4(0.25f, 1.0f, 1.0f, 1.0f)), Visualizer_v(r.vertices[1].m, vec4(0.25f, 1.0f, 1.0f, 1.0f))});

                        goto exit_flag;
                    }
                    
                    ++step;
                    if(step >= steps) {
                        for(int i = 0; i < a_vertices.size(); ++i) {
                            vec2 a_v = a_vertices[i];
                            for(int j = 0; j < b_vertices.size(); ++j) {
                                vec2 b_v = b_vertices[j];

                                vec2 diff = a_v - b_v;

                                vec4 color = vec4(1.0f);

                                points_b.push_back(Visualizer_v(diff, color));
                            }   
                        }

                        vec2 avg = vec2(0.0f);
                        vec4 color = vec4(0.25f, 0.25f, 1.0f, 1.0f);

                        for(Simplex_vertex& v : p.vertices) {
                            avg += v.m;

                            points.push_back(Visualizer_v(v.m, color));
                        }
                        avg /= p.vertices.size();
                        
                        points.push_back(Visualizer_v(point_a + ta.position, color));
                        points.push_back(Visualizer_v(point_b + ta.position, color));
                        points.push_back(Visualizer_v(point_m, color));

                        for(auto l : p.edges) {
                            vec2 va = p.vertices[l.vertices[0]].m;
                            vec2 vb = p.vertices[l.vertices[1]].m;
                            
                            triangles.push_back({Visualizer_v(va, vec4(1.0f, 1.0f, 0.25f, 0.5f)), Visualizer_v(vb, vec4(1.0f, 1.0f, 0.25f, 0.5f)), Visualizer_v(avg, vec4(1.0f, 1.0f, 0.25f, 0.5f))});
                        }
                        
                        lines.push_back({Visualizer_v(r.vertices[0].m, vec4(0.25f, 1.0f, 1.0f, 1.0f)), Visualizer_v(r.vertices[1].m, vec4(0.25f, 1.0f, 1.0f, 1.0f))});

                        goto exit_flag;
                    }

                    float dist = dot(point_m, r.normal);

                    if(abs(dist - dot(r.vertices[0].m, r.normal)) < limit) {
                        vec2 cp_a = r.vertices[0].a * r.weights.x + r.vertices[1].a * r.weights.y;
                        vec2 cp_b = r.vertices[0].b * r.weights.x + r.vertices[1].b * r.weights.y;
                        
                        vec2 separation_vector = cp_b - cp_a;

                        vec2 collision_normal = normalize(separation_vector);

                        std::vector<Collision_data> v;

                        // return
                        for(int i = 0; i < a_vertices.size(); ++i) {
                            vec2 a_v = a_vertices[i];
                            for(int j = 0; j < b_vertices.size(); ++j) {
                                vec2 b_v = b_vertices[j];

                                vec2 diff = a_v - b_v;

                                vec4 color = vec4(1.0f);

                                points_b.push_back(Visualizer_v(diff, color));
                            }   
                        }

                        points.push_back(Visualizer_v(cp_a - cp_b, vec4(0.25f, 1.0f, 0.25f, 1.0f)));
                        points.push_back(Visualizer_v(cp_a + ta.position, vec4(0.25f, 1.0f, 0.25f, 1.0f)));
                        points.push_back(Visualizer_v(cp_b + ta.position, vec4(0.25f, 1.0f, 0.25f, 1.0f)));

                        vec2 avg = vec2(0.0f);

                        for(Simplex_vertex& v : p.vertices) {
                            avg += v.m;
                            
                            vec4 color = vec4(0.25f, 0.25f, 1.0f, 1.0f);

                            points.push_back(Visualizer_v(v.m, color));
                        }
                        avg /= p.vertices.size();

                        for(auto l : p.edges) {
                            vec2 va = p.vertices[l.vertices[0]].m;
                            vec2 vb = p.vertices[l.vertices[1]].m;
                            
                            triangles.push_back({Visualizer_v(va, vec4(0.25f, 1.0f, 0.25f, 0.5f)), Visualizer_v(vb, vec4(0.25f, 1.0f, 0.25f, 0.5f)), Visualizer_v(avg, vec4(0.25f, 1.0f, 0.25f, 0.5f))});
                        }

                        goto exit_flag;
                    } else {
                        p.expand({point_m, point_a, point_b});
                    }
                }

                //return Collision_data();
            } else {
                simplex.vertices.erase(simplex.vertices.begin() + n);
                
                vec2 line_direction = glm::normalize(simplex.vertices[0].m - simplex.vertices[1].m);
                vec2 rel_origin_pos = -simplex.vertices[1].m;

                vec2 closest_point = line_direction * glm::dot(rel_origin_pos, line_direction) + simplex.vertices[1].m;
                direction = glm::normalize(-closest_point);
            }
        }
    }

    exit_distance: 

    if(simplex.vertices.size() == 1) {
        points.push_back(Visualizer_v(simplex.vertices[0].a + ta.position, vec4(1.0f, 0.25f, 1.0f, 1.0f)));
        points.push_back(Visualizer_v(simplex.vertices[0].b + ta.position, vec4(1.0f, 0.25f, 1.0f, 1.0f)));
        points.push_back(Visualizer_v(simplex.vertices[0].m, vec4(1.0f, 0.25f, 1.0f, 1.0f)));
    } else if(simplex.vertices.size() == 2) {
        vec2 p0 = simplex.vertices[0].m;
        vec2 p1 = simplex.vertices[1].m;

        vec2 line_axis = p1 - p0;
        float dist_c = length(line_axis);

        line_axis /= dist_c;
        vec2 normal = vec2(line_axis.y, -line_axis.x);

        vec2 p2 = -p0;
        p2 = p2 - normal * dot(normal, p2);
        p2 += p0;

        vec2 p3 = p0 - p2;
        vec2 p4 = p1 - p2;

        vec2 weights = vec2(length(p4) / dist_c, length(p3) / dist_c);

        if(weights.x > 1.0f && weights.x > weights.y) weights = vec2(1.0f, 0.0f);
        else if(weights.y > 1.0f && weights.y > weights.x) weights = vec2(0.0f, 1.0f);

        vec2 pm = p0 * weights.x + p1 * weights.y;
        vec2 pa = simplex.vertices[0].a * weights.x + simplex.vertices[1].a * weights.y;
        vec2 pb = simplex.vertices[0].b * weights.x + simplex.vertices[1].b * weights.y;
        
        points.push_back(Visualizer_v(pa + ta.position, vec4(1.0f, 0.25f, 1.0f, 1.0f)));
        points.push_back(Visualizer_v(pb + ta.position, vec4(1.0f, 0.25f, 1.0f, 1.0f)));
        points.push_back(Visualizer_v(pm, vec4(1.0f, 0.25f, 1.0f, 1.0f)));
        points.push_back(Visualizer_v(p0, vec4(1.0f, 1.0f, 0.25f, 1.0f)));
        points.push_back(Visualizer_v(p1, vec4(1.0f, 1.0f, 0.25f, 1.0f)));
    }

    exit_return_false:

    exit_flag:
}

Visualizer visualizer;

Profiler profiler;

