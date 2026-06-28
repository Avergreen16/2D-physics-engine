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

//

Transform null_transform = {vec2(0.0f), identity<mat2>()};

Physics_system::Physics_system() {
    Signature s = ecs.update_signature<Collider>();
    ecs.update_signature<Transform>(s);
    collectors.push_back(Collector{s, false});

    s = ecs.update_signature<Soft_body>();
    collectors.push_back(Collector{s, false});
}

vec2 Physics_system::transform_vertices(Transform& t, Collision_shape& c, std::vector<vec2>& vertices, vec2 origin) {
    vec2 center = vec2(0.0f);

    vec2 o = t.position - origin;

    for(vec2 vertex : c.vertices) {
        vertex = t.orientation * (c.orientation * vertex + c.position) + o;
        vertices.push_back(vertex);

        center += vertex;
    }

    return center / float(c.vertices.size());
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

std::vector<Collision_data> Physics_system::collision(Collision_input& input) {
    std::vector<Collision_data> ret;

    Return_tag tag;

    if(input.ca->BVH.size()) {
        if(input.cb->BVH.size()) {
            std::vector<uint64_t> pairs = traverse_BVH(*input.ta, input.ca->BVH, *input.tb, input.cb->BVH);

            for(uint64_t pair : pairs) {
                uint32_t a = pair & 0xFFFFFFFF;
                uint32_t b = pair >> 32;

                Collision_shape& sa = input.ca->shapes[a];
                Collision_shape& sb = input.cb->shapes[b];

                std::vector<Collision_data> r = collision(*input.ta, sa, *input.tb, sb, tag);
                ret.insert(ret.end(), r.begin(), r.end());
            }
        } else {
            std::vector<Return_tag> tags;
            std::vector<std::vector<Collision_data>> data;

            for(Collision_shape& sb : input.cb->shapes) {
                std::vector<uint32_t> shapes = traverse_BVH(*input.ta, input.ca->BVH, *input.tb, sb.bounding_box);

                for(uint32_t shape : shapes) {
                    Collision_shape& sa = input.ca->shapes[shape];

                    std::vector<Collision_data> r = collision(*input.ta, sa, *input.tb, sb, tag);

                    tags.push_back(tag);
                    data.push_back(r);
                }
            }

            // edges

            std::unordered_set<ivec2, Hash_coord> set;

            for(uint32_t i = 0; i < tags.size(); ++i) {
                Return_tag& tag = tags[i];
                if(tag.type == COLLISION_TYPE_EDGE) {
                    ret.insert(ret.end(), data[i].begin(), data[i].end());

                    set.insert(tag.va[0]);
                    set.insert(tag.va[1]);
                }
            }

            // vertices

            for(uint32_t i = 0; i < tags.size(); ++i) {
                Return_tag& tag = tags[i];
                if(tag.type == COLLISION_TYPE_VERTEX) {
                    if(set.find(tag.va[0]) == set.end()) {
                        ret.insert(ret.end(), data[i].begin(), data[i].end());

                        set.insert(tag.va[0]);
                    }
                }
            }
        }
    } else if(input.cb->BVH.size()) {
        std::vector<Return_tag> tags;
        std::vector<std::vector<Collision_data>> data;

        for(Collision_shape& sa : input.ca->shapes) {
            std::vector<uint32_t> shapes = traverse_BVH(*input.tb, input.cb->BVH, *input.ta, sa.bounding_box);

            for(uint32_t shape : shapes) {
                Collision_shape& sb = input.cb->shapes[shape];

                std::vector<Collision_data> r = collision(*input.ta, sa, *input.tb, sb, tag);

                tags.push_back(tag);
                data.push_back(r);
                //ret.insert(ret.end(), r.begin(), r.end());
            }
        }

        // edges

        std::unordered_set<ivec2, Hash_coord> set;

        for(uint32_t i = 0; i < tags.size(); ++i) {
            Return_tag& tag = tags[i];
            if(tag.type == COLLISION_TYPE_VERTEX) {
                ret.insert(ret.end(), data[i].begin(), data[i].end());

                set.insert(tag.vb[0]);
                set.insert(tag.vb[1]);
            }
        }

        // vertices

        for(uint32_t i = 0; i < tags.size(); ++i) {
            Return_tag& tag = tags[i];
            if(tag.type == COLLISION_TYPE_EDGE) {
                if(set.find(tag.vb[0]) == set.end()) {
                    ret.insert(ret.end(), data[i].begin(), data[i].end());

                    set.insert(tag.vb[0]);
                }
            }
        }
    } else {
        for(Collision_shape& sa : input.ca->shapes) {
            for(Collision_shape& sb : input.cb->shapes) {
                std::vector<Collision_data> r = collision(*input.ta, sa, *input.tb, sb, tag);

                ret.insert(ret.end(), r.begin(), r.end());
            }
        }
    }

    return ret;
}

std::vector<RS_Collision_data> Physics_system::collision(RS_Collision_input& input) {
    std::vector<RS_Collision_data> ret;

    Return_tag tag;

    if(input.ca->BVH.size()) {
        std::vector<uint64_t> pairs = traverse_BVH(*input.ta, input.ca->BVH, null_transform, input.sb->BVH);

        std::vector<Return_tag> tags;
        std::vector<std::vector<Collision_data>> data;

        for(uint64_t pair : pairs) {
            uint32_t a = pair & 0xFFFFFFFF;
            uint32_t b = pair >> 32;

            Collision_shape& sa = input.ca->shapes[a];

            uint32_t b0 = b;
            uint32_t b1 = (b + 1) % input.sb->points.size();
            vec2 pb0 = input.sb->points[b0].position;
            vec2 pb1 = input.sb->points[b1].position;

            Collision_shape shape;
            shape.vertices = {vec2(0.0f), pb1 - pb0};
            shape.radius = vec2(input.sb->inflate);
            shape.position = pb0;
            shape.orientation = identity<mat2>();

            std::vector<Collision_data> r = collision(*input.ta, sa, null_transform, shape, tag);

            std::vector<RS_Collision_data> rs;
            for(Collision_data& d : r) {
                RS_Collision_data data;

                data.a = input.a;
                data.b = input.b;
                data.pa = d.pa;
                data.pb0 = b0;
                data.pb1 = b1;

                vec2 pos = d.pb + input.ta->position;
                vec2 rel = pos - pb0;

                float ref_len = length(pb1 - pb0);
                vec2 dir = (pb1 - pb0) / ref_len;

                float dist = dot(dir, rel);

                float blend = clamp(dist / ref_len, 0.0f, 1.0f);
                vec2 offset = pos - (pb0 + (pb1 - pb0) * blend);

                data.blend = blend;
                data.offset = offset;

                if(data.blend == 0.0f || data.blend == 1.0f) data.normal = d.normal;
                else data.normal = vec2(0.0f, 0.0f);

                rs.push_back(data);
            }
            ret.insert(ret.end(), rs.begin(), rs.end());
        }
    } else {
        std::vector<uint32_t> vs = traverse_BVH(null_transform, input.sb->BVH, *input.ta, input.ca->bounding_box);

        std::vector<Return_tag> tags;
        std::vector<std::vector<Collision_data>> data;

        for(uint32_t v : vs) {
            uint32_t a = 0;
            uint32_t b = v;

            Collision_shape& sa = input.ca->shapes[a];

            uint32_t b0 = b;
            uint32_t b1 = (b + 1) % input.sb->points.size();
            vec2 pb0 = input.sb->points[b0].position;
            vec2 pb1 = input.sb->points[b1].position;

            Collision_shape shape;
            shape.vertices = {vec2(0.0f), pb1 - pb0};
            shape.radius = vec2(input.sb->inflate);
            shape.position = pb0;
            shape.orientation = identity<mat2>();

            std::vector<Collision_data> r = collision(*input.ta, sa, null_transform, shape, tag);

            std::vector<RS_Collision_data> rs;
            for(Collision_data& d : r) {
                RS_Collision_data data;

                data.a = input.a;
                data.b = input.b;
                data.pa = d.pa;
                data.pb0 = b0;
                data.pb1 = b1;

                vec2 pos = d.pb + input.ta->position;
                vec2 rel = pos - pb0;

                float ref_len = length(pb1 - pb0);
                vec2 dir = (pb1 - pb0) / ref_len;

                float dist = dot(dir, rel);

                float blend = clamp(dist / ref_len, 0.0f, 1.0f);
                vec2 offset = pos - (pb0 + (pb1 - pb0) * blend);

                data.blend = blend;
                data.offset = offset;

                if(data.blend == 0.0f || data.blend == 1.0f) data.normal = d.normal;
                else data.normal = vec2(0.0f, 0.0f);

                rs.push_back(data);
            }
            ret.insert(ret.end(), rs.begin(), rs.end());
        }
        /*
        std::vector<Return_tag> tags;
        std::vector<std::vector<Collision_data>> data;

        for(Collision_shape& sa : input.ca->shapes) {
            std::vector<uint32_t> shapes = traverse_BVH(null_transform, input.sb->BVH, *input.ta, sa.bounding_box);

            for(uint32_t shape : shapes) {
                Collision_shape& sa = input.ca->shapes[shape];

                uint32_t b0 = shape;
                uint32_t b1 = (shape + 1) % input.sb->points.size();
                vec2 pb0 = input.sb->points[b0].position;
                vec2 pb1 = input.sb->points[b1].position;

                Collision_shape s;
                s.vertices = {vec2(0.0f), pb1 - pb0};
                s.position = pb0;

                std::vector<Collision_data> r = collision(*input.ta, sa, null_transform, s, tag);

                std::vector<RS_Collision_data> rs;
                for(Collision_data& d : r) {
                    RS_Collision_data data;

                    data.a = input.a;
                    data.b = input.b;
                    data.pa = d.pa;
                    data.pb0 = b0;
                    data.pb1 = b1;

                    vec2 pos = d.pb + input.ta->position;
                    float blend = length(pos - pb0) / length(pb1 - pb0);

                    data.blend = blend;

                    rs.push_back(data);
                }
                ret.insert(ret.end(), rs.begin(), rs.end());
            }
        }

        /*

        // edges

        std::unordered_set<ivec2, Hash_coord> set;

        for(uint32_t i = 0; i < tags.size(); ++i) {
            Return_tag& tag = tags[i];
            if(tag.type == COLLISION_TYPE_EDGE) {
                ret.insert(ret.end(), data[i].begin(), data[i].end());

                set.insert(tag.va[0]);
                set.insert(tag.va[1]);
            }
        }

        // vertices

        for(uint32_t i = 0; i < tags.size(); ++i) {
            Return_tag& tag = tags[i];
            if(tag.type == COLLISION_TYPE_VERTEX) {
                if(set.find(tag.va[0]) == set.end()) {
                    ret.insert(ret.end(), data[i].begin(), data[i].end());

                    set.insert(tag.va[0]);
                }
            }
        }
         */
    }

    return ret;
}

std::vector<Collision_data> Physics_system::collision(Transform& ta, Collision_shape& ca, Transform& tb, Collision_shape& cb, Return_tag& tag) {
    std::vector<Collision_data> data;

    std::vector<vec2> a_vertices;
    std::vector<vec2> b_vertices;

    float limit = 0.00001;

    vec2 center_a = transform_vertices(ta, ca, a_vertices, ta.position);
    vec2 center_b = transform_vertices(tb, cb, b_vertices, ta.position);

    Simplex simplex;

    vec2 direction = glm::normalize(center_b - center_a);
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
            vec2 point_a = support_func(a_vertices, ca.radius, direction, ta.orientation);
            vec2 point_b = support_func(b_vertices, cb.radius, -direction, tb.orientation);

            vec2 point_m = point_a - point_b;

            for(Simplex_vertex& v : simplex.vertices) {
                vec2 difference = point_m - v.m;

                if(glm::dot(difference, direction) < limit) return {};
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

                    vec2 point_a = support_func(a_vertices, ca.radius, direction, ta.orientation);
                    vec2 point_b = support_func(b_vertices, cb.radius, -direction, tb.orientation);

                    vec2 point_m = point_a - point_b;

                    float dist = dot(point_m, r.normal);

                    float limit_2 = 0.0001;

                    if(abs(dist - dot(r.vertices[0].m, r.normal)) < limit_2) {
                        vec2 cp_a = r.vertices[0].a * r.weights.x + r.vertices[1].a * r.weights.y;
                        vec2 cp_b = r.vertices[0].b * r.weights.x + r.vertices[1].b * r.weights.y;

                        vec2 separation_vector = cp_b - cp_a;

                        vec2 collision_normal;

                        vec2 main_dir = center_a - center_b;

                        if(r.vertices[0].a != r.vertices[1].a) {
                            vec2 sv = r.vertices[0].a - r.vertices[1].a;
                            collision_normal = normalize(vec2(sv.y, -sv.x));

                            if(dot(collision_normal, main_dir) < 0.0f) collision_normal = -collision_normal;

                            tag.type = COLLISION_TYPE_EDGE;
                            tag.va[0] = ivec2(r.vertices[0].a * 8.0f);
                            tag.va[1] = ivec2(r.vertices[1].a * 8.0f);
                            tag.vb[0] = ivec2(r.vertices[0].b * 8.0f);
                        } else {
                            vec2 sv = r.vertices[0].b - r.vertices[1].b;
                            collision_normal = normalize(vec2(sv.y, -sv.x));

                            if(dot(collision_normal, main_dir) < 0.0f) collision_normal = -collision_normal;

                            tag.type = COLLISION_TYPE_VERTEX;
                            tag.va[0] = ivec2(r.vertices[0].a * 8.0f);
                            tag.vb[0] = ivec2(r.vertices[0].b * 8.0f);
                            tag.vb[1] = ivec2(r.vertices[1].b * 8.0f);
                        }

                        if(isnan(collision_normal.x)) {
                            std::cout << "ERROR: ISNAN\n";
                            return {};
                        }

                        // clipping

                        vec2 pa = support_func(a_vertices, ca.radius, -collision_normal, ta.orientation);
                        float da0 = FLT_MAX;
                        float da1 = -FLT_MAX;

                        vec2 pb = support_func(b_vertices, cb.radius, collision_normal, tb.orientation);
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

bool Physics_system::collision_point(std::vector<vec2> vs, vec2 radius, vec2 point) {
    std::vector<vec2> a_vertices;

    float limit = 0.00001;

    for(vec2 v : vs) {
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
            vec2 point_a = support_func(a_vertices, radius, direction);

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

    if(collision_table.find(a) == collision_table.end()) {
        collision_table.emplace(a, std::vector<Collision_data>());
    }

    std::vector<Collision_data>& v = collision_table[a];

    for(int i = v.size() - 1; i >= 0; --i) {
        Collision_data& d = v[i];
        vec2 diff_a = d.pa - c.pa;
        vec2 diff_b = d.pb - c.pb;

        if(length(diff_a) < contact_sep && length(diff_b) < contact_sep) return;
    }

    v.push_back(c);
}

void Physics_system::insert_collision(RS_Collision_data c) {
    uint64_t a = uint64_t(c.a) | (uint64_t(c.b) << 32);

    if(RS_collision_table.find(a) == RS_collision_table.end()) {
        RS_collision_table.emplace(a, std::vector<RS_Collision_data>());
    }

    std::vector<RS_Collision_data>& v = RS_collision_table[a];

    Soft_body sb = ecs.get_component<Soft_body>(c.b);
    vec2 cpb = sb.points[c.pb0].position * (1.0f - c.blend) + sb.points[c.pb1].position * c.blend + c.offset;

    for(int i = v.size() - 1; i >= 0; --i) {
        RS_Collision_data& d = v[i];
        vec2 dpb = sb.points[d.pb0].position * (1.0f - d.blend) + sb.points[d.pb1].position * d.blend + d.offset;

        vec2 diff_a = d.pa - c.pa;
        vec2 diff_b = dpb - cpb;

        if(length(diff_a) < contact_sep && length(diff_b) < contact_sep) return;
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

Bounding_box Physics_system::transform(Transform& t, Bounding_box& b) {
    Bounding_box ret;

    vec2 center = (b.minimum + b.maximum) * 0.5f;
    vec2 r = b.maximum - center;

    center = t.orientation * center + t.position;

    mat2 m = {abs(t.orientation[0]), abs(t.orientation[1])};

    r = m * r;

    ret.minimum = center - r;
    ret.maximum = center + r;

    return ret;
}

bool Physics_system::collision(Transform& ta, Bounding_box& a, Transform& tb, Bounding_box& b) {
    Transform t;
    t.position = transpose(ta.orientation) * (tb.position - ta.position);
    t.orientation = transpose(ta.orientation) * tb.orientation;

    Bounding_box nb = transform(t, b);

    return collision(a, nb);
}

bool Physics_system::collision(Bounding_box& a, Bounding_box& b) {
    bool bx = a.minimum.x < b.maximum.x && b.minimum.x < a.maximum.x;
    bool by = a.minimum.y < b.maximum.y && b.minimum.y < a.maximum.y;

    return bx && by;
}

struct spacial_data {
    uint32_t i;
    Bounding_box bb;
};

std::vector<uint64_t> Physics_system::broad_phase(std::vector<input_data>& input) {
    const std::size_t max_i = 4;
    const float ratio = 4.0f;
    const float start_size = 4.0f;

    std::unordered_set<uint64_t> set;

    std::array<std::unordered_map<ivec2, std::vector<spacial_data>, Hash_coord>, max_i> spacial;

    auto insert_into = [&](input_data& ii) {
        Bounding_box bb = transform(*ii.transform, ii.bounding_box);

        vec2 size = bb.maximum - bb.minimum;

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

        vec2 min = bb.minimum / bucket_size;
        vec2 max = bb.maximum / bucket_size;

        ivec2 mmin = ivec2(floor(min));
        ivec2 mmax = ivec2(floor(max));

        spacial_data sd;
        sd.i = ii.id;
        sd.bb = bb;

        for(int y = mmin.y; y <= mmax.y; ++y) {
            for(int x = mmin.x; x <= mmax.x; ++x) {
                ivec2 i = {x, y};
                auto& bucket = spacial_scale->operator[](i);

                bucket.push_back(sd);
            }
        }
    };

    for(auto& ii : input) {
        insert_into(ii);
    }

    float bucket_size = start_size;
    for(int k = 0; k < max_i; ++k) {
        auto& s = spacial[k];
        for(auto& [key, v] : s) {
            for(int i = 0; i < v.size(); ++i) {
                auto vi = v[i];

                // loop through all shapes in same level
                for(int j = i + 1; j < v.size(); ++j) {
                    auto vj = v[j];

                    if(collision(vi.bb, vj.bb)) {
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
                    if(spacial[m].find(k2) != spacial[m].end()) {
                        auto& v2 = spacial[m].at(k2);

                        for(int j = 0; j < v2.size(); ++j) {
                            auto vj = v2[j];

                            if(collision(vi.bb, vj.bb)) {
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

    return std::vector<uint64_t>(set.begin(), set.end());
}

void Collider::create_bounding_box() {
    bounding_box.minimum = vec2(FLT_MAX, FLT_MAX);
    bounding_box.maximum = vec2(-FLT_MAX, -FLT_MAX);

    for(Collision_shape& cs : shapes) {
        cs.bounding_box.minimum = vec2(FLT_MAX, FLT_MAX);
        cs.bounding_box.maximum = vec2(-FLT_MAX, -FLT_MAX);

        for(vec2 v : cs.vertices) {
            v = cs.orientation * v + cs.position;

            cs.bounding_box.minimum = min(cs.bounding_box.minimum, v - cs.radius);
            cs.bounding_box.maximum = max(cs.bounding_box.maximum, v + cs.radius);
        }

        bounding_box.minimum = min(bounding_box.minimum, cs.bounding_box.minimum);
        bounding_box.maximum = max(bounding_box.maximum, cs.bounding_box.maximum);
    }
}

void Collider::create_BVH() {
    create_bounding_box();

    BVH_node root;
    for(int i = 0; i < shapes.size(); ++i) root.children.push_back(i);

    BVH.push_back(root);

    uint32_t ca;
    uint32_t cb;

    auto split = [&](BVH_node& node) {
        uint32_t index = 0;

        Bounding_box centers;

        for(int i : node.children) {
            Collision_shape& shape = shapes[i];
            vec2 center = (shape.bounding_box.minimum + shape.bounding_box.maximum) * 0.5f;

            node.bounding_box.minimum = min(node.bounding_box.minimum, shape.bounding_box.minimum);
            node.bounding_box.maximum = max(node.bounding_box.maximum, shape.bounding_box.maximum);

            centers.minimum = min(centers.minimum, center);
            centers.maximum = max(centers.maximum, center);
        }

        if(node.children.size() > 1) {
            vec2 size = centers.maximum - centers.minimum;
            vec2 center = (centers.minimum + centers.maximum) * 0.5f;

            BVH_node child_a;
            BVH_node child_b;

            int ii = 0;
            if(size.y > size.x) ii = 1;

            for(int i : node.children) {
                Collision_shape& shape = shapes[i];

                float c = (shape.bounding_box.minimum[ii] + shape.bounding_box.maximum[ii]) * 0.5f;
                if(c < center[ii]) child_a.children.push_back(i);
                else child_b.children.push_back(i);
            }

            ca = BVH.size();
            cb = BVH.size() + 1;

            node.children = {ca, cb};

            BVH.push_back(child_a);
            BVH.push_back(child_b);

            return true;
        } else return false;
    };

    std::vector<uint32_t> open_nodes = {0};
    std::vector<uint32_t> new_open_nodes = {};

    while(true) {
        if(open_nodes.size() == 0) break;

        for(uint32_t n : open_nodes) {
            if(split(BVH[n])) {
                new_open_nodes.push_back(ca);
                new_open_nodes.push_back(cb);
            }
        }

        open_nodes = std::move(new_open_nodes);
        new_open_nodes.clear();
    }
}

void Soft_body::create_BVH() {
    BVH.clear();

    std::vector<Bounding_box> bbs;
    for(int i = 0; i < points.size(); ++i) {
        auto& pa = points[i];
        auto& pb = points[(i + 1) % points.size()];

        Bounding_box bb;
        bb.minimum = min(pa.position - inflate, pb.position - inflate);
        bb.maximum = max(pa.position + inflate, pb.position + inflate);

        bbs.push_back(bb);
    }

    BVH_node root;
    for(int i = 0; i < bbs.size(); ++i) root.children.push_back(i);

    BVH.push_back(root);

    uint32_t ca;
    uint32_t cb;

    auto split = [&](BVH_node& node) {
        uint32_t index = 0;

        Bounding_box centers;

        for(int i : node.children) {
            Bounding_box& bounding_box = bbs[i];
            vec2 center = (bounding_box.minimum + bounding_box.maximum) * 0.5f;

            node.bounding_box.minimum = min(node.bounding_box.minimum, bounding_box.minimum);
            node.bounding_box.maximum = max(node.bounding_box.maximum, bounding_box.maximum);

            centers.minimum = min(centers.minimum, center);
            centers.maximum = max(centers.maximum, center);
        }

        if(node.children.size() > 1) {
            vec2 size = centers.maximum - centers.minimum;
            vec2 center = (centers.minimum + centers.maximum) * 0.5f;

            BVH_node child_a;
            BVH_node child_b;

            int ii = 0;
            if(size.y > size.x) ii = 1;

            for(int i : node.children) {
                Bounding_box& bounding_box = bbs[i];

                float c = (bounding_box.minimum[ii] + bounding_box.maximum[ii]) * 0.5f;
                if(c < center[ii]) child_a.children.push_back(i);
                else child_b.children.push_back(i);
            }

            ca = BVH.size();
            cb = BVH.size() + 1;

            node.children = {ca, cb};

            BVH.push_back(child_a);
            BVH.push_back(child_b);

            return true;
        } else return false;
    };

    std::vector<uint32_t> open_nodes = {0};
    std::vector<uint32_t> new_open_nodes = {};

    while(true) {
        if(open_nodes.size() == 0) break;

        for(uint32_t n : open_nodes) {
            if(split(BVH[n])) {
                new_open_nodes.push_back(ca);
                new_open_nodes.push_back(cb);
            }
        }

        open_nodes = std::move(new_open_nodes);
        new_open_nodes.clear();
    }

    bounding_box = BVH[0].bounding_box;
}

std::vector<uint32_t> Physics_system::traverse_BVH(Transform& ta, std::vector<BVH_node>& ca, Transform& tb, Bounding_box& bb) {
    std::vector<uint32_t> front_buffer = {0};
    std::vector<uint32_t> back_buffer;
    std::vector<uint32_t> shapes;

    while(true) {
        if(front_buffer.size() == 0) break;

        for(uint32_t i : front_buffer) {
            BVH_node& node = ca[i];

            if(Physics_system::collision(ta, node.bounding_box, tb, bb)) {
                if(node.children.size() > 1) {
                    back_buffer.push_back(node.children[0]);
                    back_buffer.push_back(node.children[1]);
                } else shapes.push_back(node.children[0]);
            }
        }

        front_buffer = back_buffer;
        back_buffer.clear();
    }

    return shapes;
}

uint32_t depth = 0;

std::vector<uint64_t> Physics_system::traverse_BVH(Transform& ta, std::vector<BVH_node>& ca, Transform& tb, std::vector<BVH_node>& cb) {
    std::vector<uint64_t> front_buffer = {0};
    std::vector<uint64_t> back_buffer;
    std::vector<uint64_t> shape_pairs;

    while(true) {
        if(front_buffer.size() == 0) break;

        for(uint64_t i : front_buffer) {
            uint32_t ai = i & 0xFFFFFFFF;
            uint32_t bi = i >> 32;

            BVH_node& node_a = ca[ai];
            BVH_node& node_b = cb[bi];

            if(Physics_system::collision(ta, node_a.bounding_box, tb, node_b.bounding_box)) {
                if(node_a.children.size() == 1) {
                    if(node_b.children.size() == 1) {
                        shape_pairs.push_back(uint64_t(node_a.children[0]) | (uint64_t(node_b.children[0]) << 32));
                    } else {
                        back_buffer.push_back(uint64_t(ai) | (uint64_t(node_b.children[0]) << 32));
                        back_buffer.push_back(uint64_t(ai) | (uint64_t(node_b.children[1]) << 32));
                    }
                } else if(node_b.children.size() == 1) {
                    back_buffer.push_back(uint64_t(node_a.children[0]) | (uint64_t(bi) << 32));
                    back_buffer.push_back(uint64_t(node_a.children[1]) | (uint64_t(bi) << 32));
                } else {
                    back_buffer.push_back(uint64_t(node_a.children[0]) | (uint64_t(node_b.children[0]) << 32));
                    back_buffer.push_back(uint64_t(node_a.children[1]) | (uint64_t(node_b.children[0]) << 32));
                    back_buffer.push_back(uint64_t(node_a.children[0]) | (uint64_t(node_b.children[1]) << 32));
                    back_buffer.push_back(uint64_t(node_a.children[1]) | (uint64_t(node_b.children[1]) << 32));
                }
            }
        }

        front_buffer = back_buffer;
        back_buffer.clear();
    }

    return shape_pairs;
}

void Soft_body::create(std::vector<vec2> vs, vec2 pos, float mass) {
    float single_mass = mass / vs.size();

    vec2 center = vec2(0.0f);

    for(vec2 v : vs) center += v;
    center /= vs.size();

    target_center = center + pos;

    for(vec2 v : vs) {
        targets.push_back(v - center);

        Soft_body_point point;
        point.position = v + pos;
        point.velocity = vec2(0.0f);
        point.mass = single_mass;

        points.push_back(point);
    }
}


void Physics_system::physics_loop() {
    std::vector<input_data> input;

    for(uint32_t entity : collectors[1].entities) {
        Soft_body& soft_body = ecs.get_component<Soft_body>(entity);

        soft_body.create_BVH();

        input_data ii;
        ii.bounding_box = soft_body.bounding_box;
        ii.transform = &null_transform;
        ii.id = entity;

        input.push_back(ii);
    }

    for(uint32_t a : collectors[0].entities) {
        Collider& ac = ecs.get_component<Collider>(a);
        Transform& at = ecs.get_component<Transform>(a);

        ac.colliding_with.clear();
        ac.colliding_normal.clear();

        if(ac.bounding_box.minimum.x == FLT_MAX) ac.create_bounding_box();

        Mesh& m = ecs.get_component<Mesh>(a);

        //

        input_data ii;
        ii.bounding_box = ac.bounding_box;
        ii.transform = &at;
        ii.id = a;

        input.push_back(ii);
    }

    std::vector<uint64_t> collisions = broad_phase(input);

    const uint32_t num_threads = 12;
    std::vector<std::thread> threads(num_threads);
    std::vector<std::vector<Collision_data>> cdata(num_threads);
    std::vector<std::vector<RS_Collision_data>> rs_cdata(num_threads);
    std::vector<std::vector<uint64_t>> threads_collisions(num_threads);

    uint32_t num_collisions = 0;
    float num_per_thread = float(collisions.size()) / num_threads;
    for(uint64_t i : collisions) {
        float f = float(num_collisions) / num_per_thread;
        uint32_t fi = min(uint32_t(f), num_threads - 1);

        threads_collisions[fi].push_back(i);

        ++num_collisions;
    }

    int N = 8;

    bool bb = false;

    auto thread_GJK = [&](uint32_t t) {

        std::vector<uint64_t> cache;
        uint32_t ii = 0;
        for(uint64_t i : threads_collisions[t]) {
            ++ii;
            cache.push_back(i);

            if(cache.size() == N || ii >= threads_collisions[t].size() - 1) {
                std::vector<Collision_input> inputs;
                std::vector<RS_Collision_input> rs_inputs;
                for(int j = 0; j < N; ++j) {
                    if(j < cache.size()) {
                        uint32_t a = cache[j] & NULL_ENTITY;
                        uint32_t b = cache[j] >> 32;

                        if(ecs.has_component<Soft_body>(a)) {
                            if(ecs.has_component<Soft_body>(b)) {

                            } else {
                                bb = true;
                                Collider& ca = ecs.get_component<Collider>(b);
                                Transform& ta = ecs.get_component<Transform>(b);

                                Soft_body& sb = ecs.get_component<Soft_body>(a);

                                if(ca.non_colliding.find(b) != ca.non_colliding.end() || sb.non_colliding.find(a) != sb.non_colliding.end() ) continue;

                                RS_Collision_input ci;
                                ci.a = b;
                                ci.b = a;
                                ci.ca = &ca;
                                ci.ta = &ta;
                                ci.sb = &sb;

                                rs_inputs.push_back(ci);

                            }
                        } else if(ecs.has_component<Soft_body>(b)) {
                            bb = true;
                            Collider& ca = ecs.get_component<Collider>(a);
                            Transform& ta = ecs.get_component<Transform>(a);

                            Soft_body& sb = ecs.get_component<Soft_body>(b);

                            if(ca.non_colliding.find(b) != ca.non_colliding.end() || sb.non_colliding.find(a) != sb.non_colliding.end() ) continue;

                            RS_Collision_input ci;
                            ci.a = a;
                            ci.b = b;
                            ci.ca = &ca;
                            ci.ta = &ta;
                            ci.sb = &sb;

                            rs_inputs.push_back(ci);
                        } else {
                            Collider& ca = ecs.get_component<Collider>(a);
                            Transform& ta = ecs.get_component<Transform>(a);

                            Collider& cb = ecs.get_component<Collider>(b);
                            Transform& tb = ecs.get_component<Transform>(b);

                            if(ca.non_colliding.find(b) != ca.non_colliding.end() || cb.non_colliding.find(a) != cb.non_colliding.end() ) continue;

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
                }

                for(int j = 0; j < inputs.size(); ++j) {
                    std::vector<Collision_data> cv = collision(inputs[j]);

                    if(cv.size()) {
                        for(Collision_data& c : cv) {
                            Collision_input& ci = inputs[j];

                            Mesh& am = ecs.get_component<Mesh>(ci.a);
                            Mesh& bm = ecs.get_component<Mesh>(ci.b);

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

                for(int j = 0; j < rs_inputs.size(); ++j) {
                    std::vector<RS_Collision_data> cv = collision(rs_inputs[j]);

                    if(cv.size()) {
                        for(RS_Collision_data& c : cv) {
                            RS_Collision_input& ci = rs_inputs[j];

                            bool insert = true;

                            if(ci.ca->is_static) {
                                c.pa = transpose(ci.ta->orientation) * c.pa + ci.ta->position;
                                c.a = NULL_ENTITY;
                                c.b = ci.b;
                            } else {
                                c.pa = transpose(ci.ta->orientation) * (c.pa);
                                c.a = ci.a;
                                c.b = ci.b;
                            }

                            if(insert) rs_cdata[t].push_back(c);
                        }
                    }
                }

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

    for(auto& c : cdata) {
        for(Collision_data& collision_data : c) {
            insert_collision(collision_data);
        }
    }
    for(auto& c : rs_cdata) {
        for(RS_Collision_data& collision_data : c) {
            insert_collision(collision_data);
        }
    }

    collision_constraints.clear();
    collision_constraints.resize(collision_table.size());

    uint32_t i = 0;
    for(auto& [k, d] : collision_table) {
        Collision_constraint cc;
        uint32_t a = k & 0xFFFFFFFF;
        uint32_t b = k >> 32;
        cc.a = a;
        cc.b = b;

        cc.ca = &ecs.get_component<Collider>(cc.a);
        cc.ta = &ecs.get_component<Transform>(cc.a);
        cc.ca->colliding_with.push_back(cc.b);
        cc.ca->colliding_normal.push_back(d[0].normal);

        if(cc.b != NULL_ENTITY) {
            cc.cb = &ecs.get_component<Collider>(cc.b);
            cc.tb = &ecs.get_component<Transform>(cc.b);
            cc.cb->colliding_with.push_back(cc.a);
            cc.cb->colliding_normal.push_back(-d[0].normal);
        }

        //

        for(int i = 0; i < d.size(); ++i) {
            col_constraint col;

            Collision_data& c = d[i];

            col.d = &c;

            cc.constraints.push_back(col);
        }
        collision_constraints[i] = cc;
        ++i;
    }

    //

    RS_collision_constraints.clear();
    RS_collision_constraints.resize(RS_collision_table.size());

    //

    i = 0;
    for(auto& [k, d] : RS_collision_table) {
        RS_Collision_constraint cc;
        uint32_t a = k & 0xFFFFFFFF;
        uint32_t b = k >> 32;
        cc.a = a;
        cc.b = b;

        if(cc.a != NULL_ENTITY) {
            cc.ca = &ecs.get_component<Collider>(cc.a);
            cc.ta = &ecs.get_component<Transform>(cc.a);
            cc.ca->colliding_with.push_back(cc.b);
            cc.ca->colliding_normal.push_back(d[0].normal);
        }

        cc.sb = &ecs.get_component<Soft_body>(cc.b);

        //

        for(int i = 0; i < d.size(); ++i) {
            RS_col_constraint col;

            RS_Collision_data& c = d[i];

            col.d = &c;
            col.ib0 = c.pb0;
            col.ib1 = c.pb1;
            col.blend = c.blend;
            col.offset = c.offset;

            cc.constraints.push_back(col);
        }
        RS_collision_constraints[i] = cc;
        ++i;
    }

    //

    Input_system& is = ecs.get_system<Input_system>();

    uint32_t start = constraints.size();

    //constraints.insert(constraints.end(), is.slime_constraints.begin(), is.slime_constraints.end());

    //
    for(int i = 0; i < substeps; ++i) {
        velocity_solve();

        integrate();
    }

    std::cout << substeps << " " << velocity_iterations << "\n";

    //constraints.erase(constraints.begin() + start, constraints.end());

    // prune

    std::vector<uint64_t> remove_table;
    std::vector<uint64_t> RS_remove_table;

    for(Collision_constraint& c : collision_constraints) {
        uint64_t key = uint64_t(c.a) | (uint64_t(c.b) << 32);
        auto& d = collision_table[key];

        std::vector<uint32_t> n_erase;

        for(col_constraint& cc : c.constraints) {
            vec2 distance = cc.pa - cc.pb;

            float dot_normal = dot(distance, cc.d->normal);
            float v = length(distance - cc.d->normal * dot_normal);

            if(dot_normal > contact_sep || v > contact_sep) {

                n_erase.push_back((uint64_t(cc.d) - uint64_t(d.data())) / sizeof(Collision_data));
            }
        }

        uint32_t i = 0;
        for(uint32_t n : n_erase) {
            d.erase(d.begin() + (n - i));
            ++i;
        }

        if(d.size() == 0) remove_table.push_back(key);
    }

    for(auto& v : RS_collision_constraints) {
        for(auto& vv : v.constraints) {
            //renderer.points.push_back(v.sb->points[vv.ib0].position * (1.0f - vv.blend) + v.sb->points[vv.ib1].position * vv.blend);
            //renderer.normals.push_back(vv.normal);
            //renderer.cs.push_back(vec3(1.0f, 0.75f, 0.25f));
        }
    }

    for(auto& [key, c] : RS_collision_table) {
        std::vector<uint32_t> n_erase;

        uint32_t a = key & 0xFFFFFFFF;
        uint32_t b = key >> 32;

        Soft_body& sb = ecs.get_component<Soft_body>(b);
        Transform& ta = ecs.get_component<Transform>(a);

        for(auto& cc : c) {
            vec2 pa = ta.orientation * cc.pa + ta.position;
            vec2 pb = sb.points[cc.pb0].position * (1.0f - cc.blend) + sb.points[cc.pb1].position * cc.blend;
            vec2 distance = pa - pb;

            vec2 normal = cc.normal;

            if(length(cc.normal) == 0.0) {
                normal = sb.points[cc.pb1].position - sb.points[cc.pb0].position;
                normal = normalize(vec2(normal.y, -normal.x));
            }

            float dot_normal = dot(distance, normal);
            float v = length(distance - normal * dot_normal);

            if(dot_normal > contact_sep || v > contact_sep) {

                n_erase.push_back((uint64_t(&cc) - uint64_t(c.data())) / sizeof(RS_Collision_data));
            }
        }

        uint32_t i = 0;
        for(uint32_t n : n_erase) {
            c.erase(c.begin() + (n - i));
            ++i;
        }

        if(c.size() == 0) RS_remove_table.push_back(key);
    }

    for(uint64_t k : remove_table) collision_table.erase(k);
    for(uint64_t k : RS_remove_table) RS_collision_table.erase(k);
}

void Physics_system::integrate() {
    for(uint32_t entity : collectors[0].entities) {
        Transform& ta = ecs.get_component<Transform>(entity);
        Collider& ca = ecs.get_component<Collider>(entity);

        if(!ca.is_static) {
            ta.position += ca.velocity * sub_dt;

            if(ca.allow_rotation) {
                mat2 rotation = rotate(ca.angular_velocity * sub_dt, vec3(0, 0, 1));
                ta.orientation = rotation * ta.orientation;
            }
        }

        if(!ca.is_static) {
            if(ca.allow_gravity) {
                vec2 g = get_gravity(ta.position) * -20.0f;

                ca.velocity += g * sub_dt;
            }
        }
    }

    for(uint32_t entity : collectors[1].entities) {
        Soft_body& s = ecs.get_component<Soft_body>(entity);

        for(auto& p : s.points) {
            p.position += p.velocity * sub_dt;

            if(s.allow_gravity) {
                vec2 g = get_gravity(p.position) * -20.0f;

                p.velocity += g * sub_dt;
            }
        }
    }
}

void Physics_system::call() {
    Input_system& input_system = ecs.get_system<Input_system>();

    if(sim_active) {
        physics_time += core.delta_time;

        uint32_t frames = 0;

        while(physics_time >= physics_step) {
            physics_loop();
            physics_time -= physics_step;
            ++frames;

            if(frames >= max_frames) {
                physics_time = min(physics_time, physics_step);
                break;
            }
        }
    }
}


void Physics_system::apply_impulse(Collider* c, vec2 impulse, vec2 point) {
    c->velocity += impulse / c->mass;
    if(c->allow_rotation) c->angular_velocity += cross(vec3(point, 0.0f), vec3(impulse, 0.0f)).z / c->inertia;
}

void Physics_system::apply_impulse(Soft_body* c, vec2 impulse, ivec2 ids, float blend) {
    vec2 i0 = impulse * (1.0f - blend) / c->points[ids.x].mass;
    vec2 i1 = impulse * blend / c->points[ids.y].mass;

    c->points[ids.x].velocity += i0;
    c->points[ids.y].velocity += i1;
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

void Physics_system::velocity_solve() {
    float spring = 0.35f;
    float softness = 0.005f;

    float spring_constraint = 0.35f;
    float softness_constraint = 0.005f;
    float factor = 1.0f / physics_step;
    float factor_constraint = 1.0f / physics_step;

    for(Collision_constraint& data : collision_constraints) {
        data.get_points();
        data.get_value();

        float baumgarte_max = 1.0f;

        for(col_constraint& c : data.constraints) {
            c.baumgarteN = max(c.baumgarteN, -baumgarte_max);

            c.lambdaN = c.d->lambdaN;
            c.lambdaT = c.d->lambdaT;

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

    for(RS_Collision_constraint& data : RS_collision_constraints) {
        data.get_points();
        data.get_value();

        for(RS_col_constraint& c : data.constraints) {
            c.lambdaN = 0.0f;
            c.lambdaT = 0.0f;
        }
    }

    for(uint32_t soft_body : collectors[1].entities) {
        Soft_body& sb = ecs.get_component<Soft_body>(soft_body);

        sb.compute_match();
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
            if(data.b != NULL_ENTITY) data.cb->angular_velocity -= twirl / data.cb->inertia;
        }
    }

    float distance_stiffness = 0.2f;
    float shape_stiffness = 0.0f;

    float target_length = 0.0f;
    float current_length = 0.0f;

    for(int i = 0; i < velocity_iterations; ++i) {
        for(uint32_t soft_body : collectors[1].entities) {
            target_length = 0.0f;
            current_length = 0.0f;

            vec2 avg_velocity = vec2(0.0f);
            float avg_av_num = 0.0f;
            float avg_av_den = 0.0f;

            Soft_body& sb = ecs.get_component<Soft_body>(soft_body);

            std::vector<float> jacobian(sb.points.size() * 2, 0.0f);
            float global_inertia = 0.0f;

            for(int j = 0; j < sb.points.size(); ++j) {
                uint32_t a = j;
                uint32_t b = (j + 1) % sb.points.size();

                vec2 oa = sb.targets[a];
                vec2 ob = sb.targets[b];

                auto &pa = sb.points[a];
                auto &pb = sb.points[b];

                //

                jacobian[a * 2] += 0.5f * pb.position.y;
                jacobian[b * 2] += 0.5f * -pa.position.y;

                jacobian[a * 2 + 1] += 0.5f * -pb.position.x;
                jacobian[b * 2 + 1] += 0.5f * pa.position.x;

                global_inertia += 1.0f / pa.mass;

                //

                float target_difference = length(oa - ob);
                float current_difference = length(pa.position - pb.position);
                vec2 direction = normalize(pa.position - pb.position);
                vec2 target_position = direction * target_difference;

                target_length += target_difference;
                current_length += current_difference;

                //

                float inertia = 1.0f / pa.mass + 1.0f / pb.mass;

                vec2 velocity = pa.velocity - pb.velocity;

                float v = dot(velocity, direction);

                float baumgarte = (current_difference - target_difference) * distance_stiffness * factor;

                float L = v + baumgarte;
                L /= inertia;

                vec2 impulse = direction * L;

                pa.velocity -= impulse / pa.mass;
                pb.velocity += impulse / pb.mass;

                //

                avg_velocity += pa.velocity;

                vec2 dist = pa.position - sb.target_center;

                float cr = dist.x * pa.velocity.y - dist.y * pa.velocity.x;
                avg_av_num += cr;
                avg_av_den += (dist.x * dist.x + dist.y * dist.y);
            }

            avg_velocity /= sb.points.size();
            avg_av_num /= avg_av_den;

            for(int j = 0; j < sb.points.size(); ++j) {
                uint32_t a = j;
                auto &pa = sb.points[a];

                vec2 diff = pa.position - sb.target_center;
                vec2 ang = vec2(avg_av_num * -diff.y, avg_av_num * diff.x);

                vec2 difference = pa.velocity - (avg_velocity + ang);

                diff = normalize(diff);

                //difference = diff * dot(diff, difference);

                pa.velocity -= difference * 0.0015f;
            }

            // area constraint

            float baumgarte = sb.current_area - sb.target_area * 0.95f;

            float C = 0.0f;

            for(int j = 0; j < sb.points.size(); ++j) {
                auto &pa = sb.points[j];

                C += jacobian[j * 2] * pa.velocity.x + jacobian[j * 2 + 1] * pa.velocity.y;
            }

            for(int j = 0; j < sb.points.size(); ++j) {
                uint32_t a = j;

                auto &pa = sb.points[a];

                vec2 d = vec2(jacobian[j * 2], jacobian[j * 2 + 1]);
                d *= -C / global_inertia / pa.mass - baumgarte * 0.1f * factor;

                pa.velocity += d;
            }
        }

        for(Collision_constraint& data : collision_constraints) {
            for(col_constraint& cc : data.constraints) {

                vec2 velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position);

                float diff = (cc.baumgarteN) * spring * factor;

                if(cc.d->b == NULL_ENTITY) {
                    float inertia = cc.inertiaNa;

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


                    float inverse_mass = cc.inertiaTa;

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
                    float inertia = cc.inertiaNa + cc.inertiaNb;

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

                    inertia = cc.inertiaTa + cc.inertiaTb;

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

        for(RS_Collision_constraint& data : RS_collision_constraints) {
            for(RS_col_constraint& cc : data.constraints) {
                vec2 velocity = -calculate_point_velocity(data.sb, {cc.ib0, cc.ib1}, cc.blend);

                float diff = (cc.baumgarteN) * spring * factor;

                if(cc.d->a == NULL_ENTITY) {
                    float inertia = cc.inertiaNb;

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

                    apply_impulse(data.sb, -impulse, {cc.ib0, cc.ib1}, cc.blend);

                    // friction

                    float normal_magnitude = length(impulse);

                    velocity = -calculate_point_velocity(data.sb, {cc.ib0, cc.ib1}, cc.blend);

                    vec2 tangent_vector = cc.tangent;
                    float tangent_velocity = dot(velocity, tangent_vector);

                    float inverse_mass = cc.inertiaTb;

                    float mu = 1.5f;

                    float max_friction = abs(mu * cc.lambdaN);

                    float new_lambdaT = cc.lambdaT - tangent_velocity / inverse_mass;
                    new_lambdaT = clamp(new_lambdaT, -max_friction, max_friction);
                    L = new_lambdaT - cc.lambdaT;
                    cc.lambdaT = new_lambdaT;

                    float Pt = L;

                    vec2 friction_impulse = tangent_vector * Pt;

                    apply_impulse(data.sb, -friction_impulse, {cc.ib0, cc.ib1}, cc.blend);
                } else {
                    velocity += calculate_point_velocity(data.ca, cc.pa - data.ta->position);

                    float inertia = cc.inertiaNa + cc.inertiaNb;

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
                    apply_impulse(data.sb, -impulse, {cc.ib0, cc.ib1}, cc.blend);

                    // friction

                    float normal_magnitude = length(impulse);

                    velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position) - calculate_point_velocity(data.sb, {cc.ib0, cc.ib1}, cc.blend);

                    vec2 tangent_vector = cc.tangent;
                    float tangent_velocity = dot(velocity, tangent_vector);

                    float inverse_mass = cc.inertiaTa + cc.inertiaTb;

                    float mu = 1.5f;

                    float max_friction = abs(mu * cc.lambdaN);

                    float new_lambdaT = cc.lambdaT - tangent_velocity / inverse_mass;
                    new_lambdaT = clamp(new_lambdaT, -max_friction, max_friction);
                    L = new_lambdaT - cc.lambdaT;
                    cc.lambdaT = new_lambdaT;

                    float Pt = L;

                    vec2 friction_impulse = tangent_vector * Pt;

                    apply_impulse(data.ca, friction_impulse, cc.pa - data.ta->position);
                    apply_impulse(data.sb, -friction_impulse, {cc.ib0, cc.ib1}, cc.blend);
                }
            }
        }

        

        for(int j = 0; j < constraints.size(); ++j) { 
        //for(Collision_constraint& data : collision_constraints) {
            int start = 0;//core.random.next() % collision_constraints.size();
            int dir = 1;

            if(i % 2 == 1) {
                start = constraints.size() - 1;
                dir = -1;
            }

            Constraint& data = constraints[start + j * dir];

            float max_grab = FLT_MAX;

            for(pos_constraint& c : data.pos) {
                //max_grab = c.limit;

                uint32_t i = 0;
                for(vec2 v : c.vs) {
                    float bg = -c.baumgarte[i] * spring_constraint * factor_constraint;

                    float inertia = c.inertia_a[i];

                    vec2 velocity = calculate_point_velocity(data.ca, c.pa - data.ta->position);

                    vec2 diff = c.pa - c.pb;
                    float len = length(diff);
                    vec2 vv = diff / len;

                    if(data.b == NULL_ENTITY) {
                        vec2 vel = velocity;
                        //if(c.tolerance != 0.0f) vel = vv * dot(vel, vv);

                        float L = -dot(vel, v) + bg;
                        L /= inertia;
                        L -= softness_constraint * c.lambda[i];

                        float new_lambda = c.lambda[i] + L;

                        //new_lambda = clamp(new_lambda, -max_grab / inertia, max_grab / inertia);
                        L = new_lambda - c.lambda[i];
                        c.lambda[i] = new_lambda;

                        vec2 impulse = v * L;

                        apply_impulse(data.ca, impulse, c.pa - data.ta->position);
                    } else {
                        inertia += c.inertia_b[i];

                        velocity -= calculate_point_velocity(data.cb, c.pb - data.tb->position);

                        vec2 vel = velocity;       
                        //if(c.tolerance != 0.0f) vel = vv * dot(vel, vv);

                        float L = -dot(vel, v) + bg;
                        L /= inertia;
                        L -= softness_constraint * c.lambda[i];

                        float new_lambda = c.lambda[i] + L;

                        //new_lambda = clamp(new_lambda, -max_grab / inertia, max_grab / inertia);
                        L = new_lambda - c.lambda[i];
                        c.lambda[i] = new_lambda;

                        //c.lambda[i] = 0.0f;
                        //L = 0.0f;

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

                if(data.b == NULL_ENTITY) {
                    float bg = angular_delta * spring_constraint * factor_constraint;

                    float angular_velocity = data.ca->angular_velocity;

                    float L = -angular_velocity + bg;
                    L /= inertia;
                    L -= softness_constraint * c.lambda;
                    float new_lambda = c.lambda + L;

                    //new_lambda = clamp(new_lambda, -max_grab / c.inertia, max_grab / c.inertia);
                    L = new_lambda - c.lambda;
                    c.lambda = new_lambda;

                    data.ca->angular_velocity += L / data.ca->inertia;
                } else {
                    float bg = angular_delta * spring_constraint * factor_constraint;

                    float angular_velocity = data.ca->angular_velocity - data.cb->angular_velocity;

                    float L = -angular_velocity + bg;
                    L /= inertia;
                    L -= softness_constraint * c.lambda;
                    float new_lambda = c.lambda + L;

                    //new_lambda = clamp(new_lambda, -max_grab / c.inertia, max_grab / c.inertia);
                    L = new_lambda - c.lambda;
                    c.lambda = new_lambda;

                    data.ca->angular_velocity += L / data.ca->inertia;
                    data.cb->angular_velocity -= L / data.cb->inertia;
                }
            }
        }
    }

    for(Collision_constraint& c : collision_constraints) {
        for(col_constraint& cc : c.constraints) {
            cc.d->lambdaN = cc.lambdaN;
            cc.d->lambdaT = cc.lambdaT;
        }
    }
}

vec2 angular_to_linear(vec2 pos, float angular_velocity) {
    return vec2(pos.y, -pos.x) * angular_velocity;
}

vec2 Physics_system::calculate_inertia(Collision_shape& c) {
    ivec2 num_points = ivec2(16);

    Transform temp;
    temp.position = vec2(0.0f);
    temp.orientation = identity<mat2>();

    vec2 minimum = vec2(FLT_MAX, FLT_MAX);
    vec2 maximum = vec2(-FLT_MAX, -FLT_MAX);

    for(vec2 v : c.vertices) {
        v = c.orientation * v + c.position;
        minimum = min(minimum, v - c.radius);
        maximum = max(maximum, v + c.radius);
    }

    vec2 size = (maximum - minimum) / vec2(num_points);

    vec2 accum = vec2(0.0f);
    float number = 0.0f;

    float inertia = 0.0f;

    float single_inertia = 1.0f / 12 * (size.x * size.x + size.y * size.y);

    for(int y = 0; y < num_points.y; ++y) {
        for(int x = 0; x < num_points.x; ++x) {
            vec2 point = minimum + vec2(x + 0.5f, y + 0.5f) / vec2(num_points) * (maximum - minimum);
            vec2 p = transpose(c.orientation) * (point - c.position);
            bool is_inside = collision_point(c.vertices, c.radius, p);

            if(is_inside) {
                ++number;
                accum += point;

                float dist = length(point);
                inertia += single_inertia + (dist * dist);
            }
        }
    }

    vec2 center = accum / number;

    //

    inertia /= number;
    inertia *= c.mass;
    c.inertia = inertia;

    return center;
}

vec2 Physics_system::calculate_inertia(Collider& c) {
    ivec2 num_points = ivec2(16);

    vec2 center = vec2(0.0f);
    c.mass = 0.0f;
    c.inertia = 0.0f;

    for(Collision_shape& cs : c.shapes) {
        vec2 com = calculate_inertia(cs);

        center += com * cs.mass;

        c.mass += cs.mass;
        if(c.allow_rotation) c.inertia += cs.inertia;
    }

    center /= c.mass;

    //

    for(auto& cs : c.shapes) cs.position -= center;

    float dist = length(center);
    c.inertia -= dist * dist * c.mass;

    return center;
}

vec2 Physics_system::calculate_point_velocity(Collider* c, vec2 point) {
    vec2 velocity = c->velocity;
    float angular_velocity = c->angular_velocity;
    vec2 linear_velocity = vec2(cross(vec3(0.0f, 0.0f, angular_velocity), vec3(point, 0.0f)));

    velocity += linear_velocity;

    return velocity;
}

vec2 Physics_system::calculate_point_velocity(Soft_body* c, ivec2 ids, float blend) {
    auto& pa = c->points[ids.x];
    auto& pb = c->points[ids.y];

    return pa.velocity * (1.0f - blend) + pb.velocity * blend;
}

float Physics_system::calculate_inverse_mass(Collider* c, Transform* t, vec2 impulse_dir, vec2 point) {
    float inverse_mass = 1.0f / c->mass;

    if(c->allow_rotation) {
        //d = cross(constraint.pos_a, constraint.normal);
        //constraint.inertiaNa = inv_mass + dot(d, inverse_tensor_a * d);
        /*
        float torque_per_unit = length(cross(vec3(point, 0.0f), vec3(impulse_dir, 0.0f)));

        float angular_velocity = torque_per_unit / c->inertia;

        vec2 linear_velocity = vec2(cross(vec3(0.0f, 0.0f, angular_velocity), vec3(point, 0.0f)));

        float angular_inertia = dot(linear_velocity, impulse_dir);
        */

        float d = cross(vec3(point, 0.0f), vec3(impulse_dir, 0.0f)).z;
        float i = d * (d / c->inertia);
        
        inverse_mass += i;
    }

    return inverse_mass;
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

        c.tangent = vec2(c.d->normal.y, -c.d->normal.x);
        c.normal = c.d->normal;

        c.inertiaNa = Physics_system::calculate_inverse_mass(ca, ta, c.normal, c.pa - ta->position);
        c.inertiaTa = Physics_system::calculate_inverse_mass(ca, ta, c.tangent, c.pa - ta->position);

        if(b != NULL_ENTITY) {
            c.inertiaNb = Physics_system::calculate_inverse_mass(cb, tb, c.normal, c.pb - tb->position);
            c.inertiaTb = Physics_system::calculate_inverse_mass(cb, tb, c.tangent, c.pb - tb->position);
        }

        c.baumgarteN = dot(diff, c.normal);
        c.baumgarteT = dot(diff, c.tangent);
    }
}

void Collision_constraint::refresh(col_constraint& c) {
    vec2 point_a = ta->orientation * c.d->pa + ta->position;
    c.pa = point_a;
    c.inertiaNa = Physics_system::calculate_inverse_mass(ca, ta, c.normal, c.pa - ta->position);
    c.inertiaTa = Physics_system::calculate_inverse_mass(ca, ta, c.tangent, c.pa - ta->position);

    if(c.d->b == NULL_ENTITY) {
        c.pb = c.d->pb;
    } else {
        vec2 point_b = tb->orientation * c.d->pb + tb->position;
        c.pb = point_b;
        c.inertiaNb = Physics_system::calculate_inverse_mass(cb, tb, c.normal, c.pb - tb->position);
        c.inertiaTb = Physics_system::calculate_inverse_mass(cb, tb, c.tangent, c.pb - tb->position);
    }

    vec2 diff = c.pa - c.pb;
    c.baumgarteN = dot(diff, c.normal);
    c.baumgarteT = dot(diff, c.tangent);
}

void Collision_constraint::refresh_C(col_constraint& c) {
    vec2 point_a = ta->orientation * c.d->pa + ta->position;
    c.pa = point_a;

    if(c.d->b == NULL_ENTITY) {
        c.pb = c.d->pb;
    } else {
        vec2 point_b = tb->orientation * c.d->pb + tb->position;
        c.pb = point_b;
    }

    vec2 diff = c.pa - c.pb;
    c.baumgarteN = dot(diff, c.normal);
    c.baumgarteT = dot(diff, c.tangent);
}

void Constraint::refresh(pos_constraint& c) {
    vec2 point_a = ta->orientation * c.a + ta->position;
    c.pa = point_a;
    for(int i = 0; i < c.vs.size(); ++i) c.inertia_a[i] = Physics_system::calculate_inverse_mass(ca, ta, c.vs[i], c.pa - ta->position);

    if(b != NULL_ENTITY) {
        vec2 point_b = tb->orientation * c.b + tb->position;
        c.pb = point_b;
        for(int i = 0; i < c.vs.size(); ++i) c.inertia_b[i] = Physics_system::calculate_inverse_mass(cb, tb, c.vs[i], c.pb - tb->position);
    }

    for(int i = 0; i < c.vs.size(); ++i) {
        vec2 diff = c.pa - c.pb;
        float dd = dot(diff, c.vs[i]);
        c.baumgarte[i] = dd;
    }
}


void RS_Collision_constraint::get_points() {
    for(RS_col_constraint& c : constraints) {
        if(c.d->a == NULL_ENTITY) {
            c.pa = c.d->pa;
        } else {
            vec2 point_a = ta->orientation * c.d->pa + ta->position;

            c.pa = point_a;
        }

        c.pb = sb->points[c.ib0].position * (1.0f - c.blend) + sb->points[c.ib1].position * c.blend + c.offset;
    }
}

void Soft_body::compute_match() {

    vec2 center = vec2(0.0f);
    target_center = vec2(0.0f);
    for(uint32_t i = 0; i < points.size(); ++i) {
        target_center += points[i].position;
        center += targets[i];
    }
    center /= float(points.size());
    target_center /= float(points.size());


    target_area = 0.0f;
    current_area = 0.0f;
    pressure = 0.0f;

    float s = 0.0f;
    float c = 0.0f;
    for(uint32_t i = 0; i < points.size(); ++i) {
        vec2 target = targets[i];
        auto& point = points[i];

        target -= center;
        vec2 p = point.position - target_center;

        c += dot(target, p);
        s += p.y * target.x - p.x * target.y;

        vec2 pa = point.position - target_center;
        vec2 pb = points[(i + 1) % points.size()].position - target_center;
        vec2 tpa = target - center;
        vec2 tpb = targets[(i + 1) % points.size()] - center;

        float current_area_section = abs(0.5f * (pa.x * pb.y - pa.y * pb.x));
        float target_area_section = abs(0.5f * (tpa.x * tpb.y - tpa.y * tpb.x));

        target_area += target_area_section;
        current_area += current_area_section;
    }

    s /= float(points.size());
    c /= float(points.size());

    float angle = atan2(s, c);

    target_ori = glm::rotate(angle, vec3(0.0f, 0.0f, 1.0f));
}

void RS_Collision_constraint::get_value() {
    for(RS_col_constraint& c : constraints) {
        vec2 diff = c.pa - c.pb;

        if(length(c.d->normal) == 0) {
            c.tangent = normalize(sb->points[c.ib1].position - sb->points[c.ib0].position);
            c.normal = vec2(c.tangent.y, -c.tangent.x);
        } else {

            c.tangent = vec2(c.d->normal.y, -c.d->normal.x);
            c.normal = c.d->normal;
        }

        if(c.d->a != NULL_ENTITY) {
            c.inertiaNa = Physics_system::calculate_inverse_mass(ca, ta, c.normal, c.pa - ta->position);
            c.inertiaTa = Physics_system::calculate_inverse_mass(ca, ta, c.tangent, c.pa - ta->position);
        }

        float inverse_mass = (1.0f / sb->points[c.ib0].mass * (1.0f - c.blend) + (1.0f / sb->points[c.ib1].mass * c.blend));
        c.inertiaNb = inverse_mass;
        c.inertiaTb = inverse_mass;

        c.baumgarteN = dot(diff, c.normal);
        c.baumgarteT = dot(diff, c.tangent);
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
        pc.inertia_a.resize(pc.vs.size());
        pc.inertia_b.resize(pc.vs.size());
        pc.baumgarte.resize(pc.vs.size());
        pc.lambda.resize(pc.vs.size(), 0.0f);

        vec2 diff = pc.pa - pc.pb;

        for(int i = 0; i < pc.vs.size(); ++i) {
            vec2 v = pc.vs[i];
            float v_diff = dot(diff, v);
            pc.baumgarte[i] = v_diff;

            pc.inertia_a[i] = Physics_system::calculate_inverse_mass(ca, ta, v, pc.pa - ta->position);
            if(b != NULL_ENTITY) pc.inertia_b[i] = Physics_system::calculate_inverse_mass(cb, tb, v, pc.pb - tb->position);
            
            //pc.lambda[i] = 0.0f;
        }
    }

    for(rot_constraint& rc : rot) {
        vec2 dir_a = ta->orientation * rc.a;
        vec2 dir_b;

        rc.inertia = 0.0f;


        if(b == NULL_ENTITY) dir_b = rc.b;
        else {
            dir_b = tb->orientation * rc.b;
            rc.inertia += 1.0f / cb->inertia;
        }

        float angle = acos(clamp(dot(dir_a, dir_b), -1.0f, 1.0f));

        if(cross(vec3(dir_a, 0), vec3(dir_b, 0)).z > 0) {
            angle = -angle;
        }

        rc.baumgarte = angle;
        rc.inertia += 1.0f / ca->inertia;
    }
}

vec2 get_gravity(vec2 pos) {
    Physics_system& ps = ecs.get_system<Physics_system>();

    //vec2 rel_pos = pos;
    //return normalize(vec2(rel_pos.x / (ps.gravity_aspect.x * ps.gravity_aspect.x), rel_pos.y / (ps.gravity_aspect.y * ps.gravity_aspect.y)));
    return vec2(0.0f, 1.0f);
}

std::vector<Collision_data> Physics_system::collide(Transform t, std::vector<vec2> vs, float radius) {
    std::vector<Collision_data> ret;

    Collider c;
    Collision_shape cs;
    cs.vertices = vs;
    cs.radius = vec2(radius);
    c.shapes.push_back(cs);
    c.create_bounding_box();

    for(uint32_t entity : collectors[0].entities) {
        Collider& ca = ecs.get_component<Collider>(entity);
        Transform& ta = ecs.get_component<Transform>(entity);

        if(collision(ta, ca.bounding_box, t, c.bounding_box)) {
            Collision_input ci;
            ci.ta = &ta;
            ci.ca = &ca;
            ci.a = entity;

            ci.tb = &t;
            ci.cb = &c;
            ci.b = 0xFFFFFFFF;

            auto cs = collision(ci);

            for(auto& cc : cs) {
                cc.a = ci.a;
                cc.b = ci.b;

                if(ci.ca->is_static) {
                    cc.a = NULL_ENTITY;

                    cc.pa = ci.ta->position + cc.pa;
                    cc.pb = transpose(ci.tb->orientation) * (cc.pb + (ci.ta->position - ci.tb->position));
                } else {
                    cc.pa = transpose(ci.ta->orientation) * (cc.pa);
                    cc.pb = transpose(ci.tb->orientation) * (cc.pb + (ci.ta->position - ci.tb->position));
                }
            }

            ret.insert(ret.end(), cs.begin(), cs.end());
        }
    }

    return ret;
}
