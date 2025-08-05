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

vec2 Physics_system::support_func(std::vector<vec2>& vertices, float radius, vec2 direction) {
    float max_dot = -__FLT_MAX__;
    vec2 return_vertex;

    vec2 dir = radius * direction;

    for(vec2 v : vertices) {
        float dot_v = dot(direction, v);

        if(dot_v > max_dot) {
            max_dot = dot_v;
            return_vertex = v;
        }
    }

    return_vertex += dir;

    return return_vertex;
}

simd_vec2 Physics_system::support_func(std::vector<simd_vec2>& vertices, simd_vec2 direction) {
    batch max_dot = xsimd::broadcast(-__FLT_MAX__);
    simd_vec2 vv;

    for(simd_vec2& v : vertices) {
        batch dot_v = v.dot(direction);

        auto m = dot_v > max_dot;

        max_dot = xsimd::select(m, dot_v, max_dot);
        vv.x = xsimd::select(m, v.x, vv.x);
        vv.y = xsimd::select(m, v.y, vv.y);
    }

    return vv;
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

    if(glm::dot(p - centroid, normal) > 0.0f) return 0;

    get_normal(points[0].m, points[2].m, points[1].m, normal, centroid);

    if(glm::dot(p - centroid, normal) > 0.0f) return 1;

    return -1;
}

struct simd_simplex_vertex {
    simd_vec2 m;
    simd_vec2 a;
    simd_vec2 b;
};

batch negative = xsimd::broadcast(-1.0f);
batch flt_max = xsimd::broadcast(__FLT_MAX__);

struct simd_simplex {
    std::array<simd_simplex_vertex, 3> vertices;
    batch_int num_v = xsimd::broadcast(0);

    batch_int find_closest_face(simd_vec2& weights, simd_vec2& dir) {
        batch dist = xsimd::broadcast(__FLT_MAX__);
        batch_int number = xsimd::broadcast(-1);
        weights.x = xsimd::broadcast(-1.0f);
        weights.y = xsimd::broadcast(-1.0f);

        for(int i = 0; i < 2; ++i) {
            batch_int i_batch = xsimd::broadcast(i);



            simd_vec2& va = (i == 0) ? vertices[1].m : vertices[0].m;
            simd_vec2& vb = vertices[2].m;

            simd_vec2 line_axis = vb - va;
            batch inv_dist_c = 1.0f / line_axis.length();
            line_axis *= inv_dist_c;

            simd_vec2 normal;
            normal.x = line_axis.y;
            normal.y = -line_axis.x;

            simd_vec2 cc = -va;
            cc = cc - normal * normal.dot(cc);
            cc += va;

            simd_vec2 da = va - cc;
            simd_vec2 db = vb - cc;

            batch dd = da.dot(db);

            auto mask = dd <= 0.0f;

            batch dist_a = da.length();
            batch dist_b = db.length();

            simd_vec2 ret_value;
            ret_value.x = xsimd::select(mask, dist_b * inv_dist_c, negative);
            ret_value.y = xsimd::select(mask, dist_a * inv_dist_c, negative);

            // end func

            batch len_closest_pt = cc.length();
            len_closest_pt = xsimd::select(mask, len_closest_pt, flt_max);

            auto is_closest = len_closest_pt < dist;
            xsimd::batch_bool<int> ic_int = bfloat_to_bint(is_closest);

            dir = normal;
            dist = xsimd::select(is_closest, len_closest_pt, dist);
            weights.x = xsimd::select(is_closest, ret_value.x, weights.x);
            weights.y = xsimd::select(is_closest, ret_value.y, weights.y);
            number = xsimd::select(ic_int, i_batch, number);
        }

        return number;
    }

    std::vector<Simplex_vertex> get_vertices(int i) {
        std::vector<Simplex_vertex> v0(3);

        for(int j = 0; j < 3; ++j) {
            Simplex_vertex vv;
            alignas(32) float fx[N];
            alignas(32) float fy[N];

            vertices[j].m.x.store_aligned(fx);
            vertices[j].m.y.store_aligned(fy);
            v0[j].m.x = fx[i];
            v0[j].m.y = fy[i];

            vertices[j].a.x.store_aligned(fx);
            vertices[j].a.y.store_aligned(fy);
            v0[j].a.x = fx[i];
            v0[j].a.y = fy[i];
            
            vertices[j].b.x.store_aligned(fx);
            vertices[j].b.y.store_aligned(fy);
            v0[j].b.x = fx[i];
            v0[j].b.y = fy[i];
        }

        return v0;
    }

    void erase_vertices(batch_int num, xsimd::batch_bool<int> mask) {
        auto is_0 = bint_to_bfloat(num == 0 && mask);
        auto is_1 = bint_to_bfloat(num == 1 && mask);
        auto is_2 = bint_to_bfloat(num == 2 && mask);

        // if 0, 0 = 1 and 1 = 2;
        // if 1, 1 = 2;

        auto move_1 = is_0 || is_1;

        vertices[0].m.x = xsimd::select(is_0, vertices[1].m.x, vertices[0].m.x);
        vertices[0].m.y = xsimd::select(is_0, vertices[1].m.y, vertices[0].m.y);
        vertices[0].a.x = xsimd::select(is_0, vertices[1].a.x, vertices[0].a.x);
        vertices[0].a.y = xsimd::select(is_0, vertices[1].a.y, vertices[0].a.y);
        vertices[0].b.x = xsimd::select(is_0, vertices[1].b.x, vertices[0].b.x);
        vertices[0].b.y = xsimd::select(is_0, vertices[1].b.y, vertices[0].b.y);

        vertices[1].m.x = xsimd::select(move_1, vertices[2].m.x, vertices[1].m.x);
        vertices[1].m.y = xsimd::select(move_1, vertices[2].m.y, vertices[1].m.y);
        vertices[1].a.x = xsimd::select(move_1, vertices[2].a.x, vertices[1].a.x);
        vertices[1].a.y = xsimd::select(move_1, vertices[2].a.y, vertices[1].a.y);
        vertices[1].b.x = xsimd::select(move_1, vertices[2].b.x, vertices[1].b.x);
        vertices[1].b.y = xsimd::select(move_1, vertices[2].b.y, vertices[1].b.y);
        
        num_v += xsimd::select(bfloat_to_bint(is_0 || is_1 || is_2), xsimd::broadcast(-1), xsimd::broadcast(0));
    }
};

void get_normal(simd_vec2& a, simd_vec2& b, simd_vec2& c, simd_vec2& normal, simd_vec2& center) {
    simd_vec2 v = a - b;

    normal.x = v.y;
    normal.y = -v.x;

    auto mask = normal.dot(c - a) < 0; // if normal is *not* pointed towards c, flip it

    normal.x = xsimd::select(mask, -normal.x, normal.x);
    normal.y = xsimd::select(mask, -normal.y, normal.y);

    // normal IS pointed towards c

    center = a + b;
    center *= 0.5f;
}

batch_int simplex_contains(simd_simplex& simplex) {
    simd_vec2 centroid;
    simd_vec2 normal;
    
    batch_int ret = xsimd::broadcast(-1);

    get_normal(simplex.vertices[1].m, simplex.vertices[2].m, simplex.vertices[0].m, normal, centroid);
    auto m = normal.dot(-centroid) < 0.0f; // if normal is *not* pointed towards zero, c and zero are on different sides of the line (so return c)
    auto int_m = bfloat_to_bint(m);
    ret = xsimd::select(int_m, xsimd::broadcast(0), ret);

    get_normal(simplex.vertices[0].m, simplex.vertices[2].m, simplex.vertices[1].m, normal, centroid);
    m = normal.dot(-centroid) < 0.0f; 
    int_m = bfloat_to_bint(m);
    ret = xsimd::select(int_m, xsimd::broadcast(1), ret);
    
    get_normal(simplex.vertices[0].m, simplex.vertices[1].m, simplex.vertices[2].m, normal, centroid);
    m = normal.dot(-centroid) < 0.0f;
    int_m = bfloat_to_bint(m);
    ret = xsimd::select(int_m, xsimd::broadcast(2), ret);

    return ret;
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

std::vector<Polygon> from_simplex(simd_simplex& s) {
    std::vector<Polygon> ret(N);

    for(int i = 0; i < N; ++i) {
        Polygon p;
        p.vertices = s.get_vertices(i);
        
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

        ret[i] = p;
    }

    return ret;
}

std::vector<bool> Physics_system::collision(std::vector<Collision_input> input) {
    // get sizes

    batch b0 = xsimd::broadcast(0.0f);
    batch b1 = xsimd::broadcast(1.0f);

    batch_int a_num_verts;
    batch_int b_num_verts;

    alignas(32) int an[N];
    alignas(32) int bn[N];

    int max_a = 0;
    int max_b = 0;

    for(int i = 0; i < N; ++i) {
        int num_a = 0;
        int num_b = 0;
        if(i < input.size()) {
            Collision_input& ci = input[i];

            num_a = ci.ca->vertices.size();
            num_b = ci.cb->vertices.size();
        }

        an[i] = num_a;
        bn[i] = num_b;

        max_a = max(max_a, num_a);
        max_b = max(max_b, num_b);
    }

    a_num_verts.load_aligned(an);
    b_num_verts.load_aligned(bn);

    // transform verts

    std::vector<simd_vec2> a_vertices(max_a);
    std::vector<simd_vec2> b_vertices(max_b);
    
    for(int j = 0; j < max_a; ++j) {
        alignas(32) float vx[N];
        alignas(32) float vy[N];
        for(int i = 0; i < N; ++i) {
            if(i < input.size()) {
                Collision_input& ci = input[i];

                vec2 v = ci.ca->vertices[j];
                v = ci.ta->orientation * v;
                vx[i] = v.x;
                vy[i] = v.y;
            } else {
                vx[i] = INFINITY;
            }
        }

        a_vertices[j].x = xsimd::load_aligned(vx);
        a_vertices[j].y = xsimd::load_aligned(vy);
    }

    for(int j = 0; j < max_b; ++j) {
        alignas(32) float vx[N];
        alignas(32) float vy[N];
        for(int i = 0; i < N; ++i) {
            if(i < input.size()) {
                Collision_input& ci = input[i];

                vec2 v = ci.cb->vertices[j];
                v = ci.tb->orientation * v + (ci.tb->position - ci.ta->position);
                vx[i] = v.x;
                vy[i] = v.y;
            } else {
                vx[i] = INFINITY;
            }
        }

        b_vertices[j].x = xsimd::load_aligned(vx);
        b_vertices[j].y = xsimd::load_aligned(vy);
    }

    // GJK
    
    float limit = 0.01f;
    simd_simplex simplex;

    simd_vec2 direction = b_vertices[0] - a_vertices[0];
    direction.normalize();

    int iterations = 0;
    uint32_t max_iteration = 128;

    xsimd::batch_bool<int> active = a_num_verts != 0;
    xsimd::batch_bool<float> active_total = bint_to_bfloat(active);
    
    while(!none(active)) {
        ++iterations;

        /*auto isnan_bool = isnan(direction.x);

        direction.x = xsimd::select(isnan_bool, b0, direction.x);
        direction.y = xsimd::select(isnan_bool, b1, direction.y);*/
        
        int size = simplex.vertices.size();

        simd_vec2 point_a = support_func(a_vertices, direction);
        simd_vec2 point_b = support_func(b_vertices, -direction);
        
        simd_vec2 point_m = point_a - point_b;

        auto is_3 = simplex.num_v == 3;

        for(int i = 0; i < 2; ++i) {
            auto is_active = i < simplex.num_v && !is_3;

            simd_vec2 difference = point_m - simplex.vertices[i].m;
            batch diff = difference.dot(direction);

            auto return_mask = diff < limit;
            return_mask = return_mask && bint_to_bfloat(is_active);

            active_total = active_total && !return_mask;
        }

        auto return_mask = point_m.dot(direction) > limit;
        active_total = active_total && return_mask;

        batch_int ii = 0;
        for(int i = 0; i < 3; ++i) {
            xsimd::batch_bool<float> mask_n = bint_to_bfloat(simplex.num_v == i);
            
            simplex.vertices[i].m.x = xsimd::select(mask_n, point_m.x, simplex.vertices[i].m.x);
            simplex.vertices[i].a.x = xsimd::select(mask_n, point_a.x, simplex.vertices[i].a.x);
            simplex.vertices[i].b.x = xsimd::select(mask_n, point_b.x, simplex.vertices[i].b.x);
            
            simplex.vertices[i].m.y = xsimd::select(mask_n, point_m.y, simplex.vertices[i].m.y);
            simplex.vertices[i].a.y = xsimd::select(mask_n, point_a.y, simplex.vertices[i].a.y);
            simplex.vertices[i].b.y = xsimd::select(mask_n, point_b.y, simplex.vertices[i].b.y);

            batch b = xsimd::select(mask_n, b1, b0);

            ii += xsimd::to_int(b);
        }
        simplex.num_v += ii;
        
        //if(simplex.num_v.get(0) == 3) std::cout << simplex.vertices[0].m.x.get(0) << " " << simplex.vertices[0].m.y.get(0)  << " " << simplex.vertices[1].m.x.get(0) << " " << simplex.vertices[1].m.y.get(0) << " " << simplex.vertices[2].m.x.get(0) << " " << simplex.vertices[2].m.y.get(0) << "\n"; 

        xsimd::batch_bool<float> mask_1 = bint_to_bfloat(simplex.num_v == 1);
        xsimd::batch_bool<float> mask_2 = bint_to_bfloat(simplex.num_v == 2);

        //std::cout << mask_1.get(0) << " " << mask_2.get(0) << " ";

        simd_vec2 dir_1 = -point_m.normalize2();

        simd_vec2 dir_2 = simplex.vertices[0].m - simplex.vertices[1].m;
        dir_2.normalize();
        batch temp = dir_2.x;
        dir_2.x = dir_2.y;
        dir_2.y = -temp;
        simd_vec2 rel_origin_pos = -simplex.vertices[1].m;
        auto dd = dir_2.dot(rel_origin_pos) < 0;
        dir_2.x = xsimd::select(dd, -dir_2.x, dir_2.x);
        dir_2.y = xsimd::select(dd, -dir_2.y, dir_2.y);

        direction.x = xsimd::select(mask_1, dir_1.x, direction.x);
        direction.y = xsimd::select(mask_1, dir_1.y, direction.y);
        direction.x = xsimd::select(mask_2, dir_2.x, direction.x);
        direction.y = xsimd::select(mask_2, dir_2.y, direction.y);
        

        auto contains = simplex_contains(simplex);
        if(is_3.get(0)) std::cout << "far vertex: " << contains.get(0) << "\n";
        auto mask = contains == -1; // true if stop
        mask = mask && is_3; // true if stop AND num_v is 3
        active = active && !mask;

        if(!mask.get(0)) {
            std::cout << simplex.vertices[0].m.x.get(0) << ", " << simplex.vertices[0].m.y.get(0) << " " << simplex.vertices[1].m.x.get(0) << ", " << simplex.vertices[1].m.y.get(0) << " " << simplex.vertices[2].m.x.get(0) << ", " << simplex.vertices[2].m.y.get(0) << " ";
            std::cout << direction.x.get(0) << ". " << direction.y.get(0) << " " << simplex.num_v.get(0) << "\n";
        }
        // insert pos into simplex;
        simplex.erase_vertices(contains, !mask && is_3);

        // switching dir
        mask_2 = bint_to_bfloat(simplex.num_v == 2);
        
        dir_2 = simplex.vertices[0].m - simplex.vertices[1].m; // the line
        dir_2.normalize();
        temp = dir_2.x;
        dir_2.x = dir_2.y;
        dir_2.y = -temp; // dir is normal to the line

        rel_origin_pos = -simplex.vertices[1].m; // normal at vertex 1, is the origin in the direction of the normal? else flip the normal
        dd = dir_2.dot(rel_origin_pos) < 0;
        dir_2.x = xsimd::select(dd, -dir_2.x, dir_2.x);
        dir_2.y = xsimd::select(dd, -dir_2.y, dir_2.y);
        
        direction.x = xsimd::select(mask_2, dir_2.x, direction.x);
        direction.y = xsimd::select(mask_2, dir_2.y, direction.y);

        active = active && bfloat_to_bint(active_total);
    }

    /*active = active_total.mask();

    while(!none(active)) {

    }

    std::vector<Polygon> pv = from_simplex(simplex);

    for(int i = 0; i < N; ++i) {
        Polygon p = pv[0];

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
                if(isnan(collision_normal.x)) return {};

                std::vector<Collision_data> v;

                return Collision_data(0, 0, cp_a, cp_b, collision_normal);
            } else {
                p.expand({point_m, point_a, point_b});
            }
        }

        return Collision_data();
    }*/
    
    alignas(32) bool ret[N];
    active_total.store_aligned(ret);
    std::vector<bool> ret_vec(input.size());
    for(int i = 0; i < input.size(); ++i) {
        ret_vec[i] = ret[i];
        std::cout << ret_vec[i];
    }

    std::cout << "\n\n";

    return ret_vec;
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
            vec2 point_a = support_func(a_vertices, ca.radius.x, direction);

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
    return a.start.x < b.start.x;
}

std::vector<uint64_t> Physics_system::sweep_and_prune(std::vector<uint32_t>& input) {
    for(uint32_t i : input) {
        if(!inserted_sap.contains(i)) {
            Sap_point s;
            s.id = i;
            sap_points.push_back(s);
            inserted_sap.insert(i);
        }
    }

    for(Sap_point& p : sap_points) {
        Collider& c = ecs.get_component<Collider>(p.id);
        p.start = {c.bounding_box.x, c.bounding_box.y};
        p.end = {c.bounding_box.z, c.bounding_box.w};
    }

    std::vector<uint64_t> xc;

    // x
    std::sort(sap_points.begin(), sap_points.end(), compare_x);

    for(int i = 0; i < sap_points.size(); ++i) {
        Sap_point p_i = sap_points[i];
        for(int j = i + 1; j < sap_points.size(); ++j) {
            Sap_point p_j = sap_points[j];

            if(p_j.start.x < p_i.end.x) {
                uint32_t a = p_i.id;
                uint32_t b = p_j.id;
                if(a > b) {
                    uint32_t temp = a;
                    a = b;
                    b = temp;
                }

                uint64_t c = (uint64_t(a) << 32) | uint64_t(b);

                if(p_i.start.y < p_j.end.y && p_j.start.y < p_i.end.y) xc.push_back(c);
            } else break;
        }
    }

    return xc;
}

void Physics_system::physics_loop() {
    profiler.reset();

    std::vector<uint32_t> input;
    for(uint32_t a : collectors[0].entities) {
        Collider& ac = ecs.get_component<Collider>(a);
        Transform& at = ecs.get_component<Transform>(a);

        vec4 bounding_box = calculate_bounding_box(ac, at);
        ac.bounding_box = bounding_box;

        input.push_back(a);
        
        ac.colliding = false;
    }

    std::vector<uint64_t> collisions = sweep_and_prune(input);
    profiler.step("sweep and prune");

    const uint32_t num_threads = 64;
    std::vector<std::thread> threads(num_threads);
    std::vector<std::vector<Collision_data>> cdata(num_threads);
    std::vector<std::vector<uint64_t>> threads_collisions(num_threads);

    uint32_t num_collisions = 0;
    float num_per_thread = float(collisions.size()) / num_threads;
    for(uint64_t i : collisions) {
        float f = float(num_collisions) / num_per_thread;
        uint32_t fi = min(uint32_t(f), num_threads - 1);

        threads_collisions[fi].push_back(i);

        ++num_collisions;
    }
    profiler.step("load possible collisions");

    auto thread_GJK = [&](uint32_t j) {
        std::vector<uint64_t> cache;
        uint32_t ii = 0;
        for(uint64_t i : threads_collisions[j]) {
            ++ii;
            cache.push_back(i);

            if(cache.size() >= N - 1 || ii >= threads_collisions[j].size() - 1) {
                std::vector<Collision_input> inputs;
                for(int j = 0; j < N; ++j) {
                    if(j < cache.size()) {
                        uint32_t a = cache[j] & 0xFFFFFFFF;
                        uint32_t b = cache[j] >> 32;
                        
                        Collider& ca = ecs.get_component<Collider>(a);
                        Transform& ta = ecs.get_component<Transform>(a);

                        Collider& cb = ecs.get_component<Collider>(b);
                        Transform& tb = ecs.get_component<Transform>(b);
                        
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

                std::vector<bool> cc = collision(inputs);

                uint32_t j = 0;
                /*for(auto c : cc) {
                    if(c.has_value()) {
                        Collision_input& ci = inputs[j];

                        ci.ca->colliding = true;
                        ci.cb->colliding = true;

                        bool insert = true;

                        if(ci.cb->is_static) {
                            if(ci.ca->is_static) insert = false;
                            else {
                                c->pa = transpose(ci.ta->orientation) * c->pa;
                                c->pb = ci.ta->position + c->pb;
                                c->a = ci.a;
                                c->b = 0xFFFFFFFF;
                            }
                        } else if(ci.ca->is_static) {
                            c->a = ci.b;
                            c->b = 0xFFFFFFFF;
                            
                            vec2 temp = c->pa;
                            c->pa = transpose(ci.tb->orientation) * (c->pb + (ci.ta->position - ci.tb->position));
                            c->pb = ci.ta->position + temp;
                        } else {
                            c->pa = transpose(ci.ta->orientation) * (c->pa);
                            c->pb = transpose(ci.tb->orientation) * (c->pb + (ci.ta->position - ci.tb->position));
                            c->a = ci.a;
                            c->b = ci.b;
                        }

                        if(insert) cdata[j].push_back(c.value());
                    }

                    ++j;
                }*/
            }

            cache.clear();
        }
    };

    for(int i = 0; i < num_threads; ++i) {
        threads[i] = std::thread(thread_GJK, i);
    }
    
    for(int i = 0; i < num_threads; ++i) {
        threads[i].join();
    }
    profiler.step("GJK");
    
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

    solve_constraints(collision_constraints);
    profiler.step("solve");

    for(uint32_t a : collectors[0].entities) {
        Collider& ca = ecs.get_component<Collider>(a);
        Transform& ta = ecs.get_component<Transform>(a);

        if(!ca.is_static) {
            ta.position += ca.velocity * physics_step;

            if(ca.allow_rotation) {
                mat2 rotation = rotate(ca.angular_velocity * physics_step, vec3(0, 0, 1));
                ta.orientation = rotation * ta.orientation;
            }

            if(ca.allow_gravity) {
                vec2 g = get_gravity(ta.position) * -20.0f;

                ca.velocity += g * physics_step;
            }
        }
    }
    profiler.step("add velocities");

    for(Collision_constraint& c : collision_constraints) {
        for(col_constraint& cc : c.constraints) {
            vec2 distance = cc.pa - cc.pb;

            float dot_normal = dot(distance, cc.d->normal);
            float v = length(distance - cc.d->normal * dot_normal);
            
            if(dot_normal > 0.01 || v > 0.01) {
                uint64_t key = uint64_t(c.a) | (uint64_t(c.b) << 32);
                
                auto& d = collision_table[key];
                d.erase(d.begin() + (uint64_t(cc.d) - uint64_t(d.data())) / sizeof(Collision_data));
                if(d.size() == 0) collision_table.erase(key);
            }
        }
    }
    profiler.step("prune collisions");
}

void Physics_system::call() {
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


void Physics_system::apply_impulse(Collider* c, vec2 impulse, vec2 point) {
    c->velocity += impulse / c->mass;
    c->angular_velocity += cross(vec3(point, 0.0f), vec3(impulse, 0.0f)).z / c->inertia;
}

void Physics_system::solve_constraints(std::vector<Collision_constraint>& collisions) {
    int iterations = 6;
    float spring = 0.5f;
    float softness = 0.05f;

    for(Constraint& data : constraints) {
        data.ca = &ecs.get_component<Collider>(data.a);
        data.ta = &ecs.get_component<Transform>(data.a);
        if(data.b != 0xFFFFFFFF) {
            data.cb = &ecs.get_component<Collider>(data.b);
            data.tb = &ecs.get_component<Transform>(data.b);
        }

        data.get_points();
        data.get_values();
    }

    for(Collision_constraint& data : collisions) {
        data.ca = &ecs.get_component<Collider>(data.a);
        data.ta = &ecs.get_component<Transform>(data.a);
        if(data.b != 0xFFFFFFFF) {
            data.cb = &ecs.get_component<Collider>(data.b);
            data.tb = &ecs.get_component<Transform>(data.b);
        }

        data.get_points();
        data.get_value();

        for(col_constraint& c : data.constraints) {
            c.lambdaN = c.d->prev_lambdaN;
            c.lambdaT = c.d->prev_lambdaT;

            if(data.b == 0xFFFFFFFF) {
                vec2 impulse = c.normal * c.lambdaN;

                apply_impulse(data.ca, impulse, c.pa - data.ta->position);

                vec2 friction_impulse = c.tangent * c.lambdaT;

                apply_impulse(data.ca, friction_impulse, c.pa - data.ta->position);
            } else {                
                c.inertiaN += calculate_inverse_mass(data.cb, data.tb, c.normal, c.pb - data.tb->position);
                c.inertiaT += calculate_inverse_mass(data.cb, data.tb, c.tangent, c.pb - data.tb->position);

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
            for(pos_constraint& c : data.pos) {
                uint32_t i = 0;
                for(vec2 v : c.vs) {
                    float bg = c.baumgarte[i] * spring / physics_step;

                    float inertia = c.inertia[i];

                    vec2 velocity = calculate_point_velocity(data.ca, c.pa - data.ta->position);

                    if(data.b == 0xFFFFFFFF) {
                        float L = -dot(velocity, v) + bg;
                        L /= inertia;
                        L -= softness * c.lambda[i];
                        float new_lambda = c.lambda[i] + L;
                        c.lambda[i] = new_lambda;

                        vec2 impulse = v * L;

                        apply_impulse(data.ca, impulse, c.pa - data.ta->position);
                    } else {
                        velocity -= calculate_point_velocity(data.cb, c.pb - data.tb->position);

                        float L = -dot(velocity, v) + bg;
                        L /= inertia;
                        L -= softness * c.lambda[i];
                        float new_lambda = c.lambda[i] + L;
                        c.lambda[i] = new_lambda;

                        vec2 impulse = v * L;

                        apply_impulse(data.ca, impulse, c.pa - data.ta->position);
                        apply_impulse(data.cb, -impulse, c.pb - data.tb->position);
                    }

                    ++i;
                }
            }
            
            for(rot_constraint& c : data.rot) {
                float angular_delta = -c.baumgarte;
                float inertia = c.inertia;

                float bg = angular_delta * spring / physics_step;

                float angular_velocity = data.ca->angular_velocity - data.cb->angular_velocity;

                float L = -angular_velocity + bg;
                L /= inertia;
                //L -= softness * data.lambda;
                float new_lambda = c.lambda + L;
                c.lambda = new_lambda;

                data.ca->angular_velocity += L / data.ca->inertia;
                data.cb->angular_velocity -= L / data.cb->inertia;
            }
        }
        
        for(Collision_constraint& data : collisions) {
            for(col_constraint& cc : data.constraints) {
                float inertia = cc.inertiaN;

                vec2 velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position);

                float diff = cc.baumgarte * spring / physics_step;

                if(cc.d->b == 0xFFFFFFFF) {
                    float v = dot(velocity, cc.d->normal);

                    float L = -v - diff; 
                    L /= inertia;
                    L -= softness * cc.lambdaN;
                    
                    vec2 limits = vec2(0.0f, __FLT_MAX__);

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
                    
                    vec2 limits = vec2(0.0f, __FLT_MAX__);

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

void Collision_constraint::get_points() {
    for(col_constraint& c : constraints) {
        vec2 point_a = ta->orientation * c.d->pa + ta->position;

        c.pa = point_a;

        if(c.d->b == 0xFFFFFFFF) {
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

        c.inertiaN = Physics_system::calculate_inverse_mass(ca, ta, c.normal, c.pa - ta->position);
        c.inertiaT = Physics_system::calculate_inverse_mass(ca, ta, c.tangent, c.pa - ta->position);
    }
}


void Constraint::get_points() {
    for(pos_constraint& pc : pos) {
        pc.pa = ta->orientation * pc.a + ta->position;

        if(b != 0xFFFFFFFF) {
            pc.pb = tb->orientation * pc.b + tb->position;
        } else {
            pc.pb = pc.b;
        }
    }
}

void Constraint::get_values() {
    for(pos_constraint& pc : pos) {
        pc.baumgarte.resize(pc.vs.size());
        pc.lambda.resize(pc.vs.size());
        pc.inertia.resize(pc.vs.size());

        vec2 diff = pc.pa - pc.pb;
        uint32_t i = 0;
        for(vec2 v : pc.vs) {
            float v_diff = dot(diff, v);
            pc.baumgarte[i] = -v_diff;
            pc.lambda[i] = 0.0f;
            pc.inertia[i] = Physics_system::calculate_inverse_mass(ca, ta, v, pc.pa - ta->position);

            if(b != 0xFFFFFFFF) pc.inertia[i] += Physics_system::calculate_inverse_mass(cb, tb, v, pc.pb - tb->position);

            ++i;
        }
    }

    for(rot_constraint& rc : rot) {
        vec2 dir_a = ta->orientation * rc.a;
        vec2 dir_b;

        if(b == 0xFFFFFFFF) dir_b = rc.b;
        else {
            dir_b = tb->orientation * rc.b;
        }

        float angle = acos(clamp(dot(dir_a, dir_b), -1.0f, 1.0f));

        if(cross(vec3(dir_a, 0), vec3(dir_b, 0)).z > 0) {
            angle = -angle;
        }

        rc.baumgarte = angle;
        rc.lambda = 0.0f;
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
    current_pos = 0;
    prev_time = get_time();

    iterations = 0;
}

void Profiler::reset() {
    current_pos = 0;
    prev_time = get_time();

    ++iterations;
}

void Profiler::step(std::string name) {
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