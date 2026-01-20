#include "physics.hpp"
#include "render.hpp"
#include "input.hpp"
#include "core.hpp"

/*
KEY:
white -> static
grey -> kinematic (no effects)

kinematic effects:
yellow -> colliding
cyan -> held by cursor

*/

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

vec2 Physics_system::support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction) {
    float max_dot = -FLT_MAX;
    vec2 return_vertex;

    for(vec2 v : vertices) {
        float dot_v = dot(direction, v);

        if(dot_v > max_dot) {
            max_dot = dot_v;
            return_vertex = v;
        }
    }
    
    float s = sqrt(radius.x * radius.x * direction.x * direction.x + radius.y * radius.y * direction.y * direction.y);
    if(s > 0.0f) {
        vec2 ellipsoid = vec2(radius.x * radius.x * direction.x, radius.y * radius.y * direction.y) / s;
        return_vertex += ellipsoid;
    }

    return return_vertex;
}

vec2 Physics_system::support_func(std::vector<vec2>& vertices, vec2 radius, vec2 direction, mat2 matrix) {
    float max_dot = -FLT_MAX;
    vec2 return_vertex;

    for(vec2 v : vertices) {
        float dot_v = dot(direction, v);

        if(dot_v > max_dot) {
            max_dot = dot_v;
            return_vertex = v;
        }
    }
    
    //direction = transpose(matrix) * direction;
    
    /*
    float s = sqrt(radius.x * radius.x * direction.x * direction.x + radius.y * radius.y * direction.y * direction.y);
    if(s > 0.0f) {
        vec2 ellipsoid = vec2(radius.x * radius.x * direction.x, radius.y * radius.y * direction.y) / s;
        ellipsoid = matrix * ellipsoid;

        return_vertex += ellipsoid;
    }
    */
    return_vertex += direction * radius.x;

    return return_vertex;
}

struct Simplex_vertex {
    vec2 m;
    vec2 a;
    vec2 b;
};

glm::vec2 segment_project(glm::vec2 a, glm::vec2 b, glm::vec2 c, vec2& p) {
    vec2 line_axis = b - a;
    float dist_c = 1.0f / length(line_axis);
    line_axis *= dist_c;

    vec2 normal = vec2(line_axis.y, -line_axis.x);

    vec2 cc = -a;
    cc = cc - normal * dot(normal, cc);
    cc += a;

    vec2 da = a - cc;
    vec2 db = b - cc;

    float dist_a = length(da);
    float dist_b = length(db);
    
    dist_a *= dist_c;
    dist_b *= dist_c;

    if(dist_a > dist_b && dist_a > 1.0f) {
        dist_a = 1.0f;
        dist_b = 0.0f;
    } else if(dist_b > dist_a && dist_b > 1.0f) {
        dist_a = 0.0f;
        dist_b = 1.0f;
    }

    p = cc;

    return {dist_b, dist_a};
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
        float dist = FLT_MAX;
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

    //get_normal(points[0].m, points[1].m, points[2].m, normal, centroid);
    //float d2 = glm::dot(p - centroid, normal);

    if(d0 > 0 && d0 > d1) return 0;
    if(d1 > 0 && d1 > d0) return 1;
    //if(d2 > 0 && d2 > max(d0, d1)) return 2;

    return -1;
}

batch negative = xsimd::broadcast(-1.0f);
batch flt_max = xsimd::broadcast(FLT_MAX);

struct Polygon_return {
    std::vector<Simplex_vertex> vertices;
    vec2 normal;
    vec2 weights = vec2(-1.0f);
};

struct Polygon_edge {
    std::vector<uint32_t> vertices;
    vec2 normal;
    float distance;
};

struct Polygon {
    std::vector<Simplex_vertex> vertices;
    std::vector<Polygon_edge> edges;
    vec2 sum = vec2(0.0f);
    
    Polygon_return find_closest_face() {
        Polygon_return ret;

        float min_dist = FLT_MAX;
        int edge_i = -1;
        for(int i = 0; i < edges.size(); ++i) {
            Polygon_edge& edge = edges[i];

            if(edge.distance < min_dist) {
                min_dist = edge.distance;
                edge_i = i;
            }
        }

        if(edge_i != -1) {
            Polygon_edge& edge = edges[edge_i];
            uint32_t a = edge.vertices[0];
            uint32_t b = edge.vertices[1];

            Simplex_vertex va = vertices[a];
            Simplex_vertex vb = vertices[b];

            vec2 center;

            vec2 w = segment_project(va.m, vb.m, vec2(0.0f), center);

            if(w.x != -1) {
                ret.vertices = {va, vb};
                vec2 c;
                get_normal(va.m, vb.m, sum / float(vertices.size()), ret.normal, c);
                //ret.normal = edge.normal;
                ret.weights = w;
            }
        }

        return ret;
    }

    void insert_edge(uint32_t a, uint32_t b) {
        vec2 pa = vertices[a].m;
        vec2 pb = vertices[b].m;

        vec2 normal;
        vec2 center;
        get_normal(pa, pb, sum / float(vertices.size()), normal, center);

        Polygon_edge e;
        e.vertices = {a, b};
        e.normal = normal;
        e.distance = abs(dot(normal, -center));

        edges.push_back(e);
    }

    void expand(Simplex_vertex vertex) {
        uint32_t v_n = vertices.size();
        vertices.push_back(vertex);

        vec2 prev_sum = sum;
        sum += vertex.m;
        
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
        p.sum += p.vertices[i].m;
    }
    
    for(int i = 0; i < 3; ++i) {
        uint32_t a = i;
        uint32_t b = (i + 1) % 3;

        p.insert_edge(a, b);
    }

    return p;
}

std::vector<Collision_data> Physics_system::collision(Collision_input& input, bool profiler) {
    std::vector<Collision_data> data;

    std::vector<vec2> a_vertices;
    std::vector<vec2> b_vertices;

    float limit = 0.01;

    transform_vertices(*input.ta, *input.ca, a_vertices, input.ta->position);
    transform_vertices(*input.tb, *input.cb, b_vertices, input.ta->position);

    Simplex simplex;

    vec2 direction = glm::normalize(b_vertices[0] - a_vertices[0]);
    vec2 dd = direction;
    
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

                if(glm::dot(difference, direction) < limit) {
                    //std::cout << "ERROR: difference " << direction.x << " " << direction.y << " " << difference.x << " " << difference.y << " " << size << "\n";
                    return {};
                }
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
                    if(iterations > 100) {
                        std::cout << "ERROR: ITERATION\n";
                        return {};
                    }
                    Polygon_return r = p.find_closest_face();

                    if(r.vertices.size() == 0) {
                        std::cout << "ERROR: ZERO\n";
                        std::cout << p.edges.size() << "\n";
                        return {};
                    }

                    direction = r.normal;
                    
                    vec2 point_a = support_func(a_vertices, input.ca->radius, direction, input.ta->orientation);
                    vec2 point_b = support_func(b_vertices, input.cb->radius, -direction, input.tb->orientation);

                    vec2 point_m = point_a - point_b;

                    float dist = dot(point_m, r.normal);

                    if(abs(dist - dot(r.vertices[0].m, r.normal)) < limit) {
                        vec2 cp_a = r.vertices[0].a * r.weights.x + r.vertices[1].a * r.weights.y;
                        vec2 cp_b = r.vertices[0].b * r.weights.x + r.vertices[1].b * r.weights.y;
                        
                        vec2 separation_vector = cp_b - cp_a;

                        //if(length(separation_vector) == 0.0f) return {}
                        //vec2 collision_normal = normalize(separation_vector);
                        vec2 collision_normal;

                        vec2 main_dir = input.ta->position - input.tb->position;

                        if(r.vertices[0].a != r.vertices[1].a) {
                            vec2 sv = r.vertices[0].a - r.vertices[1].a;
                            collision_normal = normalize(vec2(sv.y, -sv.x));

                            if(dot(collision_normal, main_dir) < 0.0f) collision_normal = -collision_normal;
                        } else {
                            vec2 sv = r.vertices[0].b - r.vertices[1].b;
                            collision_normal = normalize(vec2(sv.y, -sv.x));
                            
                            if(dot(collision_normal, main_dir) < 0.0f) collision_normal = -collision_normal;
                        }

                        if(isnan(collision_normal.x)) {
                            std::cout << "ERROR: ISNAN\n";
                            return {};
                        }

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

                        if(da0 <= db1 && db0 <= da1) {
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

bool Physics_system::collision_point(Collider& ca, vec2 point) {
    std::vector<vec2> a_vertices;

    float limit = 0.00001;

    for(vec2 v : ca.vertices) {
        a_vertices.push_back(v - point);
    }

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
                float dir_len = glm::length(point_a);
                if(dir_len != 0.0f) direction = -point_a / dir_len;
                else direction = vec2(direction.y, -direction.x);
            } else if(size == 1) {
                vec2 line_direction = glm::normalize(simplex.vertices[0].m - simplex.vertices[1].m);
                vec2 rel_origin_pos = -simplex.vertices[1].m;

                vec2 closest_point = line_direction * glm::dot(rel_origin_pos, line_direction) + simplex.vertices[1].m;
                float dir_len = glm::length(closest_point);
                if(dir_len != 0.0f) direction = -closest_point / dir_len;
                else direction = vec2(direction.y, -direction.x);
            }
        } else {
            int n = simplex_contains(vec2(0, 0), simplex.vertices);
            if(n == -1) return true;
            else {
                simplex.vertices.erase(simplex.vertices.begin() + n);
                
                vec2 line_direction = glm::normalize(simplex.vertices[0].m - simplex.vertices[1].m);
                vec2 rel_origin_pos = -simplex.vertices[1].m;

                vec2 closest_point = line_direction * glm::dot(rel_origin_pos, line_direction) + simplex.vertices[1].m;
                //direction = glm::normalize(-closest_point);
                float dir_len = glm::length(closest_point);
                if(dir_len != 0.0f) direction = -closest_point / dir_len;
                else direction = vec2(direction.y, -direction.x);
            }
        }
    }
}

void Physics_system::insert_collision(Collision_data c) {
    uint64_t a = uint64_t(c.a) | (uint64_t(c.b) << 32);

    if(!collision_table.contains(a)) {
        collision_table.emplace(a, std::vector<Collision_data>());
    }
        
    std::vector<Collision_data>& v = collision_table[a];

    for(int i = v.size() - 1; i >= 0; --i) {
        Collision_data& d = v[i];
        vec2 diff_a = d.pa - c.pa;
        vec2 diff_b = d.pb - c.pb;

        if(length(diff_a) < 0.05f || length(diff_b) < 0.05f) return;
    }

    v.push_back(c);
}

bool compare_x(Sap_point& a, Sap_point& b) {
    float aa = (a.is_start) ? a.start.x : a.end.x;
    float bb = (b.is_start) ? b.start.x : b.end.x;
    return aa < bb;
}

template<typename Iter, typename Compare>
void insertion_sort(Iter begin, Iter end, Compare comp) {
    for(Iter i = begin + 1; i < end; ++i) {
        auto k = *i;
        Iter j = i;
        while(j > begin && comp(k, *(j - 1))) {
            *j = *(j - 1);
            --j;
        }
        *j = k;
    }
}

struct spacial_data {
    uint32_t i;
    vec4* bb;
};

std::vector<uint64_t> Physics_system::broad_phase(std::vector<input_data>& input) {
    const std::size_t max_i = 4;
    const float ratio = 4.0f;
    const float start_size = 4.0f;

    std::unordered_set<uint64_t> set;

    std::array<std::unordered_map<ivec2, std::vector<spacial_data>, Hash_coord>, max_i> spacial;

    auto insert_into = [&](Collider& c, Transform& t, uint32_t entity) {
        vec2 size = c.bounding_box.zw() - c.bounding_box.xy();

        float max_size = max(size.x, size.y);

        std::unordered_map<ivec2, std::vector<spacial_data>, Hash_coord>* spacial_scale;

        float bucket_size = start_size;
        for(int i = 0; i < max_i; ++i) {
            if(max_size <= bucket_size || i == max_i - 1) {
                spacial_scale = &spacial[i];
                break;
            }
            bucket_size *= ratio;
        }

        vec2 min = c.bounding_box.xy() / bucket_size;
        vec2 max = c.bounding_box.zw() / bucket_size;

        ivec2 mmin = ivec2(floor(min));
        ivec2 mmax = ivec2(floor(max));
        
        spacial_data sd;
        sd.i = entity;
        sd.bb = &c.bounding_box;

        for(int y = mmin.y; y <= mmax.y; ++y) {
            for(int x = mmin.x; x <= mmax.x; ++x) {
                ivec2 i = {x, y};
                auto& bucket = spacial_scale->operator[](i);

                bucket.push_back(sd);
            }       
        }
    };

    for(auto& ii : input) {
        insert_into(*ii.collider, *ii.transform, ii.id);
    }
    profiler.step("broad phase: insert");

    auto collision_bb = [](vec4& a, vec4& b) {
        return a.x < b.z && b.x < a.z && a.y < b.w && b.y < a.w;
    };
    
    float bucket_size = start_size;
    for(int k = 0; k < max_i; ++k) {
        auto& s = spacial[k];
        for(auto& [key, v] : s) {
            for(int i = 0; i < v.size(); ++i) {
                auto vi = v[i];

                // loop through all shapes in same level
                for(int j = i + 1; j < v.size(); ++j) {
                    auto vj = v[j];

                    if(collision_bb(*vi.bb, *vj.bb)) {
                        uint64_t kk;
                        if(vi.i < vj.i) kk = (uint64_t)vi.i | (uint64_t(vj.i) << 32);
                        else kk = (uint64_t)vj.i | (uint64_t(vi.i) << 32);

                        set.emplace(kk);
                    }
                }

                // loop through all shapes in levels above

                //if(!vi.bb->flag) {
                vec2 key2 = key;
                for(int m = k + 1; m < max_i; ++m) {
                    key2 /= ratio;
                    ivec2 k2 = floor(key2);
                    if(spacial[m].contains(k2)) {
                        auto& v2 = spacial[m].at(k2);

                        for(int j = 0; j < v2.size(); ++j) {
                            auto vj = v2[j];

                            if(collision_bb(*vi.bb, *vj.bb)) {
                                uint64_t kk;
                                if(vi.i < vj.i) kk = (uint64_t)vi.i | (uint64_t(vj.i) << 32);
                                else kk = (uint64_t)vj.i | (uint64_t(vi.i) << 32);

                                set.emplace(kk);
                            }
                        }
                    }
                }
            }
        }
        bucket_size *= ratio;
    }
    profiler.step("broad phase: sample");

    //for(int i = 0; i < max_i; ++i) std::cout << spacial[i].size() << " ";
    //std::cout << "\n";

    return std::vector<uint64_t>(set.begin(), set.end());
}

void Physics_system::physics_loop() {
    profiler.reset();

    Render_system& render_system = ecs.get_system<Render_system>();
    render_system.marker_points.clear();
    render_system.normals.clear();

    std::vector<input_data> input;
    for(uint32_t a : collectors[0].entities) {
        Collider& ac = ecs.get_component<Collider>(a);
        Transform& at = ecs.get_component<Transform>(a);

        Mesh& m = ecs.get_component<Mesh>(a);
        m.color = vec3(0.35f);

        vec4 bounding_box = calculate_bounding_box(ac, at);
        ac.bounding_box = bounding_box;

        input_data ii;
        ii.collider = &ac;
        ii.transform = &at;
        ii.id = a;

        input.push_back(ii);
        
        ac.colliding = false;
    }

    std::vector<uint64_t> collisions = broad_phase(input);
    //profiler.step("broad phase");

    const uint32_t num_threads = 12;
    std::vector<std::thread> threads(num_threads);
    std::vector<std::vector<Collision_data>> cdata(num_threads);
    std::vector<std::vector<uint64_t>> threads_collisions(num_threads);
    std::vector<Profiler> thread_profilers(num_threads);

    uint32_t num_collisions = 0;
    float num_per_thread = float(collisions.size()) / num_threads;
    for(uint64_t i : collisions) {
        float f = float(num_collisions) / num_per_thread;
        uint32_t fi = min(uint32_t(f), num_threads - 1);

        threads_collisions[fi].push_back(i);

        ++num_collisions;
    }
    profiler.step("load possible collisions");

    auto thread_GJK = [&](uint32_t t) {

        std::vector<uint64_t> cache;
        uint32_t ii = 0;
        for(uint64_t i : threads_collisions[t]) {
            if(t == 0) profiler2.reset();

            ++ii;
            cache.push_back(i);

            if(cache.size() == N || ii >= threads_collisions[t].size() - 1) {
                std::vector<Collision_input> inputs;
                for(int j = 0; j < N; ++j) {
                    if(j < cache.size()) {
                        uint32_t a = cache[j] & NULL_ENTITY;
                        uint32_t b = cache[j] >> 32;
                        
                        Collider& ca = ecs.get_component<Collider>(a);
                        Transform& ta = ecs.get_component<Transform>(a);

                        Collider& cb = ecs.get_component<Collider>(b);
                        Transform& tb = ecs.get_component<Transform>(b);

                        if(ca.non_colliding.contains(b) || cb.non_colliding.contains(a)) continue;
                        
                        Collision_input ci;
                        ci.a = a;
                        ci.b = b;
                        ci.ca = &ca;
                        ci.ta = &ta;
                        ci.cb = &cb;
                        ci.tb = &tb;

                        inputs.push_back(ci);
                    }
                }
                if(t == 0) profiler2.step("load inputs");

                for(int j = 0; j < inputs.size(); ++j) {
                    std::vector<Collision_data> cv = collision(inputs[j], t == 0);

                    if(cv.size()) {
                        for(Collision_data& c : cv) {
                            Collision_input& ci = inputs[j];

                            Mesh& am = ecs.get_component<Mesh>(ci.a);
                            Mesh& bm = ecs.get_component<Mesh>(ci.b);

                            am.color = vec3(1.0f, 1.0f, 0.35f);
                            bm.color = vec3(1.0f, 1.0f, 0.35f);
                            
                            /*
                            if(c.a == 1) {
                                am.color.b += 1.0f;
                                bm.color.b += 1.0f;
                            } else {
                                am.color.g += 1.0f;
                                bm.color.g += 1.0f;
                            }
                            */

                            ci.ca->colliding = true;
                            ci.cb->colliding = true;

                            bool insert = true;

                            if(ci.cb->is_static) {
                                if(ci.ca->is_static) insert = false;
                                else {
                                    c.pa = transpose(ci.ta->orientation) * c.pa;
                                    c.pb = ci.ta->position + c.pb;
                                    c.a = ci.a;
                                    c.b = NULL_ENTITY;
                                }
                            } else if(ci.ca->is_static) {
                                c.a = ci.b;
                                c.b = NULL_ENTITY;
                                
                                vec2 temp = c.pa;
                                c.pa = transpose(ci.tb->orientation) * (c.pb + (ci.ta->position - ci.tb->position));
                                c.pb = ci.ta->position + temp;
                                c.normal = -c.normal;
                            } else {
                                c.pa = transpose(ci.ta->orientation) * (c.pa);
                                c.pb = transpose(ci.tb->orientation) * (c.pb + (ci.ta->position - ci.tb->position));
                                c.a = ci.a;
                                c.b = ci.b;
                            }

                            if(insert) cdata[t].push_back(c);
                        }
                    }
                }
                if(t == 0) profiler2.step("inserting data");
               
                cache.clear();
            }
        }
    };

    for(int i = 0; i < num_threads; ++i) {
        threads[i] = std::thread(thread_GJK, i);
    }
    
    for(int i = 0; i < num_threads; ++i) {
        threads[i].join();
    }
    profiler.step("narrow phase");
    
    for(auto& c : cdata) {
        for(Collision_data& collision_data : c) {
            insert_collision(collision_data);
        }
    }
    profiler.step("insert collision constraints");
    
    std::vector<Collision_constraint> collision_constraints(collision_table.size());

    uint32_t i = 0;
    for(auto& [k, d] : collision_table) {
        Collision_constraint cc;
        uint32_t a = k & 0xFFFFFFFF;
        uint32_t b = k >> 32;
        cc.a = a;
        cc.b = b;
        
        for(int i = 0; i < d.size(); ++i) {
            col_constraint col;

            Collision_data& c = d[i];
            
            col.d = &c;

            cc.constraints.push_back(col);
        }
        collision_constraints[i] = cc;
        ++i;
    }
    profiler.step("load constraint buffer");

    for(int i = 0; i < temporal_iterations; ++i) {
        //position_solve(collision_constraints);
        //profiler.step("solve positions");
        velocity_solve(collision_constraints);
        profiler.step("solve velocity");

        for(uint32_t a : collectors[0].entities) {
            Collider& ca = ecs.get_component<Collider>(a);
            Transform& ta = ecs.get_component<Transform>(a);

            if(!ca.is_static) {
                ta.position += ca.velocity * (physics_step / temporal_iterations);

                if(ca.allow_rotation) {
                    mat2 rotation = rotate(ca.angular_velocity * (physics_step / temporal_iterations), vec3(0, 0, 1));
                    ta.orientation = rotation * ta.orientation;
                }

                if(ca.allow_gravity) {
                    vec2 g = get_gravity(ta.position) * -20.0f;

                    ca.velocity += g * (physics_step / temporal_iterations);
                }
            }
        }
        profiler.step("add velocities");
    }

    for(Collision_constraint& c : collision_constraints) {
        for(col_constraint& cc : c.constraints) {
            vec2 distance = cc.pa - cc.pb;

            float dot_normal = dot(distance, cc.d->normal);
            float v = length(distance - cc.d->normal * dot_normal);
            
            if(dot_normal > 0.03 || v > 0.03) {
                uint64_t key = uint64_t(c.a) | (uint64_t(c.b) << 32);
                
                auto& d = collision_table[key];
                d.erase(d.begin() + (uint64_t(cc.d) - uint64_t(d.data())) / sizeof(Collision_data));
                if(d.size() == 0) collision_table.erase(key);
            }
        }
    }
    profiler.step("prune collisions");

    Input_system& input_system = ecs.get_system<Input_system>();

    vec2 collision_threshold = vec2(0.5f, 2);

    for(auto a : constraints) {
        if(a.a != NULL_ENTITY) {
            Collider& collider = ecs.get_component<Collider>(a.a);
            if(!collider.is_static) {
                Mesh& mesh = ecs.get_component<Mesh>(a.a);
                mesh.color = vec3(0.35f, 1.0f, 1.0f); 
            }
        }
        if(a.b != NULL_ENTITY) {
            Collider& collider = ecs.get_component<Collider>(a.b);
            if(!collider.is_static) {
                Mesh& mesh = ecs.get_component<Mesh>(a.b);
                mesh.color = vec3(0.35f, 1.0f, 1.0f); 
            }
        }
    }

    for(uint32_t a : collectors[0].entities) {
        Mesh& am = ecs.get_component<Mesh>(a);
        Collider& ca = ecs.get_component<Collider>(a);

        if(ca.flag2) {
            am.color = vec3(1.0f, 0.35f, 0.35f);
        }
        if(ca.is_static) {
            am.color = vec3(1.0f, 1.0f, 1.0f);
        }
        
        ca.flag2 = false;
        /*
        vec3 color = am.color;

        vec2 m = max(vec2(0.0f), color.gb() - collision_threshold) / collision_threshold;
        float mm = max(m.x, m.y);
        
        color.g = min(1.0f, color.g / collision_threshold.x);
        color.b = color.b / collision_threshold.y;
        if(am.color.g != 0.0f) color.b = 0.0f;
        if(color.b > 1.0f) {
            color.r = min(1.0f, color.b - 1.0f);
        }

        float t = 0.3f;
        t = max(0.0f, t - mm * t);

        color = color * (1.0f - t) + t;

        am.color = {color.g, color.b, color.r};
        */
    }

    if(input_system.debug_mode) {
        for(Collision_constraint& c : collision_constraints) {
            c.get_points();
            for(col_constraint& cc : c.constraints) {
                render_system.marker_points.push_back(cc.pa);
                render_system.marker_points.push_back(cc.pb);
                render_system.normals.push_back(cc.normal);
                render_system.normals.push_back(-cc.normal);
            }
        }
    }
}

void Physics_system::call() {
    Input_system& input_system = ecs.get_system<Input_system>();

    if(!input_system.debug_physics) {
        physics_time += core.delta_time;

        uint32_t frames = 0;

        while(physics_time >= physics_step) {
            physics_loop();
            physics_time -= physics_step;
            ++frames;

            if(frames >= max_frames) {
                physics_time = 0;
                break;
            }
        }
    }
}


void Physics_system::apply_impulse(Collider* c, vec2 impulse, vec2 point) {
    c->velocity += impulse / c->mass;
    c->angular_velocity += cross(vec3(point, 0.0f), vec3(impulse, 0.0f)).z / c->inertia;
}

void Physics_system::apply_position(Collider* c, Transform* t, vec2 delta, vec2 point) {
    vec2 d = delta / c->mass;
    t->position += d;
    float delta_rotation = cross(vec3(point, 0.0f), vec3(delta, 0.0f)).z / c->inertia;
    t->orientation = mat2(rotate(delta_rotation, vec3(0.0f, 0.0f, 1.0f))) * t->orientation;

    c->flag = true;
    c->flag2 = true;
}

void Constraint_distance::get_points() {
    pos_a = ta->orientation * pa + ta->position;

    if(b == NULL_ENTITY) {
        pos_b = pb;
    } else {
        pos_b = tb->orientation * pb + tb->position;
    }
}

void Constraint_distance::get_values() {
    vec2 difference = pos_a - pos_b;
    float len = length(difference);

    jacobian = difference / len;
    if(len == 0) jacobian = vec2(0.0f, 1.0f);

    //lambda = 0.0f;

    // inertia
    inertia = Physics_system::calculate_inverse_mass(ca, ta, jacobian, pos_a - ta->position);
    if(b != NULL_ENTITY) inertia += Physics_system::calculate_inverse_mass(cb, tb, -jacobian, pos_b - tb->position);

    // baumgarte

    baumgarte = -len;
}

void Physics_system::position_solve(std::vector<Collision_constraint>& collisions) {
    int iterations = 6;
    float spring = 0.2f;
    float softness = 0.3f;
    float spring_constraint = 0.5f;
    float softness_constraint = 0.03f;
    float fraction = 0.6f;

    for(Collision_constraint& data : collisions) {
        data.ca = &ecs.get_component<Collider>(data.a);
        data.ca->flag = true;
        data.ta = &ecs.get_component<Transform>(data.a);
        if(data.b != NULL_ENTITY) {
            data.cb = &ecs.get_component<Collider>(data.b);
            data.cb->flag = true;
            data.tb = &ecs.get_component<Transform>(data.b);
        }

        data.get_points();
        data.get_value();
    }

    for(Constraint& data : constraints) {
        data.ca = &ecs.get_component<Collider>(data.a);
        data.ta = &ecs.get_component<Transform>(data.a);
        if(data.b != NULL_ENTITY) {
            data.cb = &ecs.get_component<Collider>(data.b);
            data.tb = &ecs.get_component<Transform>(data.b);
        }

        data.get_points();
        data.get_values();
    }

    for(int i = 0; i < iterations; ++i) {
        for(Collision_constraint& data : collisions) {
            for(col_constraint& cc : data.constraints) {
                data.refresh(cc);
                //std::cout << "y" << cc.baumgarte << "\n";

                if(-cc.baumgarte > slop) {
                    float inertia = cc.inertiaN;
                    //cc.baumgarte += slop;

                    if(cc.d->b == NULL_ENTITY) {
                        float d = -cc.baumgarte * fraction;

                        float L = d; 
                        L /= inertia;
                        L -= softness * cc.lambdaN;
                        
                        vec2 limits = vec2(0.0f, FLT_MAX);

                        float new_lambda = cc.lambdaN + L;
                        new_lambda = clamp(new_lambda, limits.x, limits.y);
                        L = new_lambda - cc.lambdaN;
                        cc.lambdaN = new_lambda;

                        vec2 delta = cc.d->normal * L;

                        apply_position(data.ca, data.ta, delta, cc.pa - data.ta->position);
                    } else {
                        float d = -cc.baumgarte * fraction;

                        float L = d; 
                        L /= inertia;
                        L -= softness * cc.lambdaN;
                        
                        vec2 limits = vec2(0.0f, FLT_MAX);

                        float new_lambda = cc.lambdaN + L;
                        new_lambda = clamp(new_lambda, limits.x, limits.y);
                        L = new_lambda - cc.lambdaN;
                        cc.lambdaN = new_lambda;

                        vec2 delta = cc.d->normal * L;

                        apply_position(data.ca, data.ta, delta, cc.pa - data.ta->position);
                        apply_position(data.cb, data.tb, -delta, cc.pb - data.tb->position);   
                    }
                }
            }
        }

        for(Constraint& data : constraints) {
            for(pos_constraint& c : data.pos) {
                data.refresh(c);

                bool apply = false;
                for(float f : c.baumgarte) if(abs(f) > slop) apply = true;

                if(apply) {
                    for(int i = 0; i < c.inertia.size(); ++i) {
                        float inertia = c.inertia[i];

                        if(data.b == NULL_ENTITY) {
                            float d = -c.baumgarte[i] * fraction;

                            float L = d; 
                            L /= inertia;
                            L -= softness * c.lambda[i];
                            
                            //vec2 limits = vec2(0.0f, FLT_MAX);

                            float new_lambda = c.lambda[i] + L;
                            //new_lambda = clamp(new_lambda, limits.x, limits.y);
                            L = new_lambda - c.lambda[i];
                            c.lambda[i] = new_lambda;

                            vec2 delta = c.vs[i] * L;

                            apply_position(data.ca, data.ta, delta, c.pa - data.ta->position);
                        } else {
                            float d = -c.baumgarte[i] * fraction;

                            float L = d; 
                            L /= inertia;
                            L -= softness * c.lambda[i];
                            
                            //vec2 limits = vec2(0.0f, FLT_MAX);

                            float new_lambda = c.lambda[i] + L;
                            //new_lambda = clamp(new_lambda, limits.x, limits.y);
                            L = new_lambda - c.lambda[i];
                            c.lambda[i] = new_lambda;

                            vec2 delta = c.vs[i] * L;

                            apply_position(data.ca, data.ta, delta, c.pa - data.ta->position);
                            apply_position(data.cb, data.tb, -delta, c.pb - data.tb->position);   
                        }
                    }
                }
            }
            
            /*
            for(rot_constraint& c : data.rot) {
                float angular_delta = -c.baumgarte;
                float inertia = c.inertia;

                float bg = angular_delta * spring_constraint * factor_constraint;

                float angular_velocity = data.ca->angular_velocity - data.cb->angular_velocity;

                float L = -angular_velocity + bg;
                L /= inertia;
                L -= softness_constraint * c.lambda;
                float new_lambda = c.lambda + L;

                new_lambda = clamp(new_lambda, -max_grab / c.inertia, max_grab / c.inertia);
                L = new_lambda - c.lambda;
                c.lambda = new_lambda;

                data.ca->angular_velocity += L / data.ca->inertia;
                data.cb->angular_velocity -= L / data.cb->inertia;
            }*/
        }
    }
}

void Physics_system::velocity_solve(std::vector<Collision_constraint>& collisions) {
    int iterations = 6;

    float spring = 0.45f;
    float softness = 0.025f;
    float spring_constraint = 0.75f;
    float softness_constraint = 0.0f;

    float factor = 1.0f / (physics_step);
    float factor_constraint = 1.0f / (physics_step);

    for(Constraint& data : constraints) {
        data.ca = &ecs.get_component<Collider>(data.a);
        data.ta = &ecs.get_component<Transform>(data.a);
        if(data.b != NULL_ENTITY) {
            data.cb = &ecs.get_component<Collider>(data.b);
            data.tb = &ecs.get_component<Transform>(data.b);
        }

        data.get_points();
        data.get_values();
        

        for(pos_constraint& c : data.pos) {
            uint32_t i = 0;
            for(vec2 v : c.vs) {
                vec2 impulse = v * c.lambda[i];

                if(data.b == NULL_ENTITY) {
                    apply_impulse(data.ca, impulse, c.pa - data.ta->position);
                } else {
                    apply_impulse(data.ca, impulse, c.pa - data.ta->position);
                    apply_impulse(data.cb, -impulse, c.pb - data.tb->position);
                }

                ++i;
            }
        }
        
        for(rot_constraint& c : data.rot) {
            float twirl = c.lambda;

            data.ca->angular_velocity += twirl / data.ca->inertia;
            data.cb->angular_velocity -= twirl / data.cb->inertia;
        }
    }

    for(Constraint_distance& data : constraints_distance) {
        data.ca = &ecs.get_component<Collider>(data.a);
        data.ta = &ecs.get_component<Transform>(data.a);
        if(data.b != NULL_ENTITY) {
            data.cb = &ecs.get_component<Collider>(data.b);
            data.tb = &ecs.get_component<Transform>(data.b);
        }

        data.get_points();
        data.get_values();
        
        vec2 impulse = data.jacobian * data.lambda;
        apply_impulse(data.ca, impulse, data.pos_a - data.ta->position);

        if(data.b != NULL_ENTITY) {
            apply_impulse(data.cb, -impulse, data.pos_b - data.tb->position);
        }
    }

    for(Collision_constraint& data : collisions) {
        data.ca = &ecs.get_component<Collider>(data.a);
        data.ta = &ecs.get_component<Transform>(data.a);
        if(data.b != NULL_ENTITY) {
            data.cb = &ecs.get_component<Collider>(data.b);
            data.tb = &ecs.get_component<Transform>(data.b);
        }

        data.get_points();
        data.get_value();

        float baumgarte_max = 1.0f;

        for(col_constraint& c : data.constraints) {
            c.baumgarte = max(c.baumgarte, -baumgarte_max);

            c.lambdaN = c.d->prev_lambdaN;
            c.lambdaT = c.d->prev_lambdaT;

            if(data.b == NULL_ENTITY) {
                vec2 impulse = c.normal * c.lambdaN;

                apply_impulse(data.ca, impulse, c.pa - data.ta->position);

                vec2 friction_impulse = c.tangent * c.lambdaT;

                apply_impulse(data.ca, friction_impulse, c.pa - data.ta->position);
            } else {                
                vec2 impulse = c.normal * c.lambdaN;

                apply_impulse(data.ca, impulse, c.pa - data.ta->position);
                apply_impulse(data.cb, -impulse, c.pb - data.tb->position);

                vec2 friction_impulse = c.tangent * c.lambdaT;

                apply_impulse(data.ca, friction_impulse, c.pa - data.ta->position);
                apply_impulse(data.cb, -friction_impulse, c.pb - data.tb->position);
            }
        }
    }

    for(int i = 0; i < iterations; ++i) {
        for(Constraint& data : constraints) {
            float max_grab = FLT_MAX;
            if(data.is_grab) max_grab = 0x1000;

            for(pos_constraint& c : data.pos) {
                uint32_t i = 0;
                for(vec2 v : c.vs) {
                    float bg = c.baumgarte[i] * spring_constraint * factor_constraint * data.weight;

                    float inertia = c.inertia[i];

                    vec2 velocity = calculate_point_velocity(data.ca, c.pa - data.ta->position);

                    vec2 diff = c.pa - c.pb;
                    float len = length(diff);
                    vec2 vv = diff / len;

                    if(data.b == NULL_ENTITY) {
                        vec2 vel = velocity;
                        if(c.tolerance != 0.0f) vel = vv * dot(vel, vv);

                        float L = -dot(vel, v) + bg;
                        L /= inertia;
                        L -= softness_constraint * c.lambda[i];

                        if(abs(bg) > 0.01f) {
                            if(c.tolerance != 0.0f) {
                                Mesh& m = ecs.get_component<Mesh>(data.a);
                                m.color = vec3(0.35f, 0.8f, 0.35f);
                            }

                            float new_lambda = c.lambda[i] + L;

                            new_lambda = clamp(new_lambda, -max_grab / inertia, max_grab / inertia);
                            L = new_lambda - c.lambda[i];
                            c.lambda[i] = new_lambda;

                            vec2 impulse = v * L;

                            apply_impulse(data.ca, impulse, c.pa - data.ta->position);
                        }
                    } else {
                        velocity -= calculate_point_velocity(data.cb, c.pb - data.tb->position);
                        
                        vec2 vel = velocity;
                        if(c.tolerance != 0.0f) vel = vv * dot(vel, vv);

                        float L = -dot(vel, v) + bg;
                        L /= inertia;
                        L -= (softness_constraint / data.weight) * c.lambda[i];
                        
                        if(abs(bg) > 0.01f) {
                            if(c.tolerance != 0.0f) {
                                Mesh& ma = ecs.get_component<Mesh>(data.a);
                                Mesh& mb = ecs.get_component<Mesh>(data.b);
                                ma.color = vec3(0.35f, 0.8f, 0.35f);
                                mb.color = vec3(0.35f, 0.8f, 0.35f);
                            }

                            float new_lambda = c.lambda[i] + L;

                            new_lambda = clamp(new_lambda, -max_grab / inertia, max_grab / inertia);
                            L = new_lambda - c.lambda[i];
                            c.lambda[i] = new_lambda;

                            vec2 impulse = v * L;

                            apply_impulse(data.ca, impulse, c.pa - data.ta->position);
                            apply_impulse(data.cb, -impulse, c.pb - data.tb->position);
                        }
                    }

                    ++i;
                }
            }
            
            for(rot_constraint& c : data.rot) {
                float angular_delta = -c.baumgarte;
                float inertia = c.inertia;

                float bg = angular_delta * spring_constraint * factor_constraint;

                float angular_velocity = data.ca->angular_velocity - data.cb->angular_velocity;

                float L = -angular_velocity + bg;
                L /= inertia;
                L -= softness_constraint * c.lambda;
                float new_lambda = c.lambda + L;

                new_lambda = clamp(new_lambda, -max_grab / c.inertia, max_grab / c.inertia);
                L = new_lambda - c.lambda;
                c.lambda = new_lambda;

                data.ca->angular_velocity += L / data.ca->inertia;
                data.cb->angular_velocity -= L / data.cb->inertia;
            }
        }
        
        for(Collision_constraint& data : collisions) {
            for(col_constraint& cc : data.constraints) {
                float inertia = cc.inertiaN;

                vec2 velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position);

                float diff = (cc.baumgarte) * spring * factor;

                if(cc.d->b == NULL_ENTITY) {
                    float v = dot(velocity, cc.d->normal);

                    float L = -v - diff; 
                    L /= inertia;
                    L -= softness * cc.lambdaN;
                    
                    vec2 limits = vec2(0.0f, FLT_MAX);

                    float new_lambda = cc.lambdaN + L;
                    new_lambda = clamp(new_lambda, limits.x, limits.y);
                    L = new_lambda - cc.lambdaN;
                    cc.lambdaN = new_lambda;

                    vec2 impulse = cc.d->normal * L;

                    apply_impulse(data.ca, impulse, cc.pa - data.ta->position);


                    // friction

                    float normal_magnitude = length(impulse);

                    velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position);

                    vec2 tangent_vector = vec2(cc.d->normal.y, -cc.d->normal.x);
                    float tangent_velocity = dot(velocity, tangent_vector);

                    
                    float inverse_mass = cc.inertiaT;

                    float mu = 0.9f;

                    float max_friction = abs(mu * cc.lambdaN);

                    float new_lambdaT = cc.lambdaT - tangent_velocity / inverse_mass;
                    new_lambdaT = clamp(new_lambdaT, -max_friction, max_friction);
                    L = new_lambdaT - cc.lambdaT;
                    cc.lambdaT = new_lambdaT;

                    float Pt = L;

                    vec2 friction_impulse = tangent_vector * Pt;

                    apply_impulse(data.ca, friction_impulse, cc.pa - data.ta->position);
                } else {
                    velocity -= calculate_point_velocity(data.cb, cc.pb - data.tb->position);

                    float v = dot(velocity, cc.normal);

                    float L = -v - diff;
                    L /= inertia;
                    L -= softness * cc.lambdaN;
                    
                    vec2 limits = vec2(0.0f, FLT_MAX);

                    float new_lambda = cc.lambdaN + L;
                    new_lambda = clamp(new_lambda, limits.x, limits.y);
                    L = new_lambda - cc.lambdaN;
                    cc.lambdaN = new_lambda;

                    vec2 impulse = cc.normal * L;
                    
                    apply_impulse(data.ca, impulse, cc.pa - data.ta->position);
                    apply_impulse(data.cb, -impulse, cc.pb - data.tb->position);

                    // friction

                    float normal_magnitude = length(impulse);

                    velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position) - calculate_point_velocity(data.cb, cc.pb - data.tb->position);
                    float tangent_velocity = dot(velocity, cc.tangent);
                    
                    inertia = cc.inertiaT;
                    
                    float mu = 0.9f;

                    float max_friction = abs(mu * cc.lambdaN);

                    float new_lambdaT = cc.lambdaT - tangent_velocity / inertia;
                    new_lambdaT = clamp(new_lambdaT, -max_friction, max_friction);
                    L = new_lambdaT - cc.lambdaT;
                    cc.lambdaT = new_lambdaT;

                    float Pt = L;

                    vec2 friction_impulse = cc.tangent * Pt;

                    apply_impulse(data.ca, friction_impulse, cc.pa - data.ta->position);
                    apply_impulse(data.cb, -friction_impulse, cc.pb - data.tb->position);
                }
            }
        }
    }
    
    for(Collision_constraint& c : collisions) {
        for(col_constraint& cc : c.constraints) {
            cc.d->prev_lambdaN = cc.lambdaN;
            cc.d->prev_lambdaT = cc.lambdaT;
        }
    }

    for(Featherstone_constraint& c : constraints_featherstone) {
        c.solve();
    }
}

vec2 angular_to_linear(vec2 pos, float angular_velocity) {
    return vec2(pos.y, -pos.x) * angular_velocity;
}

void Featherstone_constraint::solve() {
    std::vector<vec3> sums(entities.size());
    
    for(int i = 0; i < entities.size() - 1; ++i) {
        if(i == 0) {
            uint32_t a = entities[i];
            uint32_t b = entities[i + 1];
            pos_constraint& constraint = constraints[0];
            
            Collider& ca = ecs.get_component<Collider>(a);
            Transform& ta = ecs.get_component<Transform>(a);
            Collider& cb = ecs.get_component<Collider>(b);
            Transform& tb = ecs.get_component<Transform>(b);

            // compute relative positions
            constraint.pa = ta.orientation * constraint.a;
            constraint.pb = tb.orientation * constraint.b;
            constraint.pos_a = constraint.pa + ta.position;
            constraint.pos_b = constraint.pb + tb.position;

            vec2 rel_point_pos = constraint.pa;

            vec3 c = vec3(1, 0, 0);

            vec3 vel = vec3(ca.velocity, ca.angular_velocity);

            vec3 sum = vec3(0.0f);

            mat3 X_matrix = {
                {1, 0, rel_point_pos.y},
                {0, 1, -rel_point_pos.x},
                {0, 0, 1}
            };

            sum += (X_matrix * vel);

            sums[0] = sum;
        } else {
            uint32_t a = entities[i];
            uint32_t b = entities[i + 1];
            pos_constraint& constraint = constraints[i];

            Collider& ca = ecs.get_component<Collider>(a);
            Transform& ta = ecs.get_component<Transform>(a);
            Collider& cb = ecs.get_component<Collider>(b);
            Transform& tb = ecs.get_component<Transform>(b);

            // compute relative positions
            constraint.pa = ta.orientation * constraint.a;
            constraint.pb = tb.orientation * constraint.b;
            constraint.pos_a = constraint.pa + ta.position;
            constraint.pos_b = constraint.pb + tb.position;

            // forward pass
            // a is the parent, b is the child

            mat3 inertia_a = identity<mat3>();
            inertia_a[0][0] = ca.mass;
            inertia_a[1][1] = ca.mass;
            inertia_a[2][2] = ca.inertia;
            
            mat3 inertia_b = identity<mat3>();
            inertia_b[0][0] = cb.mass;
            inertia_b[1][1] = cb.mass;
            inertia_b[2][2] = cb.inertia;

            vec3 c = vec3(1, 0, 0);

            mat2 rot_a_to_b = transpose(ta.orientation) * tb.orientation;
            mat2 rot_b_to_a = transpose(rot_a_to_b);
            vec2 pos_a_to_b = tb.position - ta.position;
            vec2 pos_b_to_a = -pos_a_to_b;

            vec2 rel_point_pos_a = constraint.pos_b - ta.position;
            vec2 rel_point_pos_b = constraint.pb;

            // find the velocity of the hinge in the frame of the child (in a matrix)
            //float angular_velocity = cb.angular_velocity;
            //vec2 linear_velocity = cb.velocity + cross(vec3(pos_b_to_a, 0), vec3(0, 0, angular_velocity)).xy();

            vec3 s = vec3(1, 0, 0);

            vec3 child_vel = vec3(cb.velocity, cb.angular_velocity);

            vec3 sum = vec3(0.0f);

            mat3 X_matrix_child = {
                {1, 0, rel_point_pos_b.y},
                {0, 1, -rel_point_pos_b.x},
                {0, 0, 1}
            };

            sum += (X_matrix_child * child_vel);

            // parent propagation
            pos_constraint& prev_constraint = constraints[i - 1];

            vec2 prev_point_pos = prev_constraint.pos_b;
            vec2 offset = constraint.pos_b - prev_point_pos;

            mat3 X_matrix_parent = {
                {1, 0, offset.y},
                {0, 1, -offset.x},
                {0, 0, 1}
            };

            sum += X_matrix_parent * sums[i - 1];

            sums[i] = sum;

            sum = transpose(X_matrix_child) * sum;

            ca.velocity = sum.xy();
            ca.angular_velocity = sum.z;
        }
    }
}

vec2 Physics_system::calculate_inertia(Collider& c) {
    ivec2 num_points = ivec2(16);

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
    if(c.allow_rotation) {
        inertia /= number;

        float dist = length(center);

        inertia -= dist * dist;

        inertia *= c.mass;

        c.inertia = inertia;
    }
    for(vec2& v : c.vertices) v -= center;

    return center;
}

vec2 Physics_system::calculate_point_velocity(Collider* c, vec2 point) {
    vec2 velocity = c->velocity;
    float angular_velocity = c->angular_velocity;
    vec2 linear_velocity = vec2(cross(vec3(0.0f, 0.0f, angular_velocity), vec3(point, 0.0f)));

    velocity += linear_velocity;

    return velocity;
}

float Physics_system::calculate_inverse_mass(Collider* c, Transform* t, vec2 impulse_dir, vec2 point) {
    float inverse_mass = 1.0f / c->mass;

    if(c->allow_rotation) {
        float torque_per_unit = length(cross(vec3(point, 0.0f), vec3(impulse_dir, 0.0f)));

        float angular_velocity = torque_per_unit / c->inertia;

        vec2 linear_velocity = vec2(cross(vec3(0.0f, 0.0f, angular_velocity), vec3(point, 0.0f)));

        float angular_inertia = dot(linear_velocity, impulse_dir);

        inverse_mass += abs(angular_inertia);
    }

    return inverse_mass;
}

vec4 Physics_system::calculate_bounding_box(Collider& c, Transform& t) {
    vec2 minimum = vec2(FLT_MAX);
    vec2 maximum = vec2(-FLT_MAX);

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

void Collision_constraint::get_points() {
    for(col_constraint& c : constraints) {
        vec2 point_a = ta->orientation * c.d->pa + ta->position;

        c.pa = point_a;

        if(c.d->b == NULL_ENTITY) {
            c.pb = c.d->pb;
        } else {
            vec2 point_b = tb->orientation * c.d->pb + tb->position;

            c.pb = point_b;
        }
    }
}

void Collision_constraint::get_value() {
    for(col_constraint& c : constraints) {
        vec2 diff = c.pa - c.pb;
        float dd = dot(diff, c.d->normal);
        c.baumgarte = dd;
        
        c.tangent = vec2(c.d->normal.y, -c.d->normal.x);
        c.normal = c.d->normal;

        c.inertiaNa = Physics_system::calculate_inverse_mass(ca, ta, c.normal, c.pa - ta->position);

        c.inertiaN = c.inertiaNa;
        c.inertiaT = Physics_system::calculate_inverse_mass(ca, ta, c.tangent, c.pa - ta->position);
        
        if(b != NULL_ENTITY) {
            c.inertiaNb = Physics_system::calculate_inverse_mass(cb, tb, c.normal, c.pb - tb->position);
            c.inertiaN += c.inertiaNb;

            c.inertiaT += Physics_system::calculate_inverse_mass(cb, tb, c.tangent, c.pb - tb->position);
        }
    }
}

void Collision_constraint::refresh(col_constraint& c) {
    if(ca->flag) {
        vec2 point_a = ta->orientation * c.d->pa + ta->position;
        c.pa = point_a;

        c.inertiaNa = Physics_system::calculate_inverse_mass(ca, ta, c.normal, c.pa - ta->position);
    }

    if(c.d->b == NULL_ENTITY) {
        c.pb = c.d->pb;
    } else {
        if(cb->flag) {
            vec2 point_b = tb->orientation * c.d->pb + tb->position;
            c.pb = point_b;
            c.inertiaNb = Physics_system::calculate_inverse_mass(cb, tb, c.normal, c.pb - tb->position);
        }
    }

    if((c.d->b == NULL_ENTITY) ? ca->flag : (ca->flag || cb->flag)) {
        vec2 diff = c.pa - c.pb;
        float dd = dot(diff, c.d->normal);
        c.baumgarte = dd;

        c.inertiaN = c.inertiaNa + c.inertiaNb;

        ca->flag = false;
        if(c.d->b != NULL_ENTITY) cb->flag = false;
    }
}

void Constraint::refresh(pos_constraint& c) {
    if(ca->flag) {
        vec2 point_a = ta->orientation * c.a + ta->position;
        c.pa = point_a;

        for(int i = 0; i < c.vs.size(); ++i) c.inertia_a[i] = Physics_system::calculate_inverse_mass(ca, ta, c.vs[i], c.pa - ta->position);
    }

    if(b != NULL_ENTITY) {
        if(cb->flag) {
            vec2 point_b = tb->orientation * c.b + tb->position;
            c.pb = point_b;

            for(int i = 0; i < c.vs.size(); ++i) c.inertia_b[i] = Physics_system::calculate_inverse_mass(cb, tb, c.vs[i], c.pb - tb->position);
        }
    }

    if((b == NULL_ENTITY) ? ca->flag : (ca->flag || cb->flag)) {
        for(int i = 0; i < c.vs.size(); ++i) {
            vec2 diff = c.pa - c.pb;
            float dd = dot(diff, c.vs[i]);
            c.baumgarte[i] = dd;

            c.inertia[i] = c.inertia_a[i] + c.inertia_b[i];

            ca->flag = false;
            if(b != NULL_ENTITY) cb->flag = false;
        }
    }
}


void Constraint::get_points() {
    for(pos_constraint& pc : pos) {
        pc.pa = ta->orientation * pc.a + ta->position;

        if(b != NULL_ENTITY) {
            pc.pb = tb->orientation * pc.b + tb->position;
        } else {
            pc.pb = pc.b;
        }
    }
}

void Constraint::get_values() {
    for(pos_constraint& pc : pos) {
        int prev_i = pc.lambda.size();

        pc.baumgarte.resize(pc.vs.size());
        pc.lambda.resize(pc.vs.size());
        pc.inertia.resize(pc.vs.size());

        pc.inertia_a.resize(pc.vs.size());
        pc.inertia_b.resize(pc.vs.size());

        vec2 diff = pc.pa - pc.pb;
        uint32_t i = 0;
        vec2 target_pos = vec2(0.0f);
        
        std::fill(pc.lambda.begin(), pc.lambda.end(), 0.0f);
        
        if(pc.tolerance != 0.0f) {
            float len = length(diff);

            if(len < pc.tolerance) {
                target_pos = diff;
                
                std::fill(pc.lambda.begin(), pc.lambda.end(), 0.0f);
            } else {
                target_pos = diff / len * pc.tolerance;
            }
        }

        for(vec2 v : pc.vs) {
            float v_diff = dot(diff - target_pos, v);
            pc.baumgarte[i] = -(abs(v_diff)) * sign(v_diff);

            if(i >= prev_i) pc.lambda[i] = 0.0f;
            pc.inertia[i] = Physics_system::calculate_inverse_mass(ca, ta, v, pc.pa - ta->position);

            if(b != NULL_ENTITY) pc.inertia[i] += Physics_system::calculate_inverse_mass(cb, tb, v, pc.pb - tb->position);

            ++i;
        }
    }

    for(rot_constraint& rc : rot) {
        vec2 dir_a = ta->orientation * rc.a;
        vec2 dir_b;

        if(b == NULL_ENTITY) dir_b = rc.b;
        else {
            dir_b = tb->orientation * rc.b;
        }

        float angle = acos(clamp(dot(dir_a, dir_b), -1.0f, 1.0f));

        if(cross(vec3(dir_a, 0), vec3(dir_b, 0)).z > 0) {
            angle = -angle;
        }

        rc.baumgarte = angle;
        //rc.lambda = 0.0f;
        rc.inertia = 1.0f / ca->inertia + (1.0f / ca->inertia);
    }
}

vec2 get_gravity(vec2 pos) {
    Physics_system& ps = ecs.get_system<Physics_system>();

    //vec2 rel_pos = pos;
    //return normalize(vec2(rel_pos.x / (ps.gravity_aspect.x * ps.gravity_aspect.x), rel_pos.y / (ps.gravity_aspect.y * ps.gravity_aspect.y)));
    return vec2(0.0f, 1.0f);
}


void Profiler::restart() {
    times.clear();
    names.clear();
    num_times.clear();
    prev_time = get_time();

    iterations = 0;
}

void Profiler::reset() {
    prev_time = get_time();

    ++iterations;
}

void Profiler::step(std::string name) {
    double current_time = get_time();
    double diff = current_time - prev_time;
    prev_time = current_time;

    int i = -1;

    for(int j = 0; j < names.size(); ++j) {
        if(names[j] == name) {
            i = j;
            break;
        }
    }

    if(i == -1) {
        times.push_back(diff);
        names.push_back(name);
        num_times.push_back(1);
    } else {
        times[i] += diff;
        ++num_times[i];
    }
}

void Profiler::output() {
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

Profiler profiler;
Profiler profiler2;

float slop = 0.0625f;