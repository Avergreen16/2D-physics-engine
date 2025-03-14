#include "physics.hpp"
#include "render.hpp"

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

    for(vec2 v : vertices) {
        v += radius * direction;
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

    if(glm::dot(p - centroid, normal) > 0.0f) return 0;

    get_normal(points[0].m, points[2].m, points[1].m, normal, centroid);

    if(glm::dot(p - centroid, normal) > 0.0f) return 1;

    get_normal(points[0].m, points[1].m, points[2].m, normal, centroid);

    if(glm::dot(p - centroid, normal) > 0.0f) return 2;

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
            vec2 point_a = support_func(a_vertices, ca.radius, direction);
            vec2 point_b = support_func(b_vertices, cb.radius, -direction);

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
                    
                    vec2 point_a = support_func(a_vertices, ca.radius, direction);
                    vec2 point_b = support_func(b_vertices, cb.radius, -direction);

                    vec2 point_m = point_a - point_b;

                    float dist = dot(point_m, r.normal);

                    if(abs(dist - dot(r.vertices[0].m, r.normal)) < limit) {
                        vec2 cp_a = r.vertices[0].a * r.weights.x + r.vertices[1].a * r.weights.y;
                        vec2 cp_b = r.vertices[0].b * r.weights.x + r.vertices[1].b * r.weights.y;
                        
                        vec2 separation_vector = cp_a - cp_b;

                        vec2 collision_normal = normalize(separation_vector);

                        std::vector<Collision_data> v;

                        vec2 contact_point_b;
                        // mass == 0
                        ///contact_point_b = transpose(tb.orientation) * (contact_point_b + vec3(ta.position - tb.position));
                        // mass != 0
                        contact_point_b = cp_b + ta.position;

                        return Collision_data(0, 0, cp_a + ta.position, contact_point_b);
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

void Physics_system::call() {
    Render_system& rs = ecs.get_system<Render_system>();
    rs.marker_points.clear();

    for(uint32_t a : collectors[0].entities) {
        Collider& ca = ecs.get_component<Collider>(a);
        Transform& ta = ecs.get_component<Transform>(a);

        ca.colliding = false;
    }
    
    std::unordered_set<uint64_t> combinations;
    for(uint32_t a : collectors[0].entities) {
        for(uint32_t b : collectors[0].entities) {
            if(a != b) {
                uint64_t cv;
                if(a > b) cv = (((uint64_t)a) << 32) + b;
                else cv = (((uint64_t)b) << 32) + a;

                if(!combinations.contains(cv)) {
                    combinations.insert(cv);

                    Collider& ca = ecs.get_component<Collider>(a);
                    Transform& ta = ecs.get_component<Transform>(a);

                    Collider& cb = ecs.get_component<Collider>(b);
                    Transform& tb = ecs.get_component<Transform>(b);

                    std::optional<Collision_data> c = collision(ca, ta, cb, tb);

                    if(c.has_value()) {
                        ca.colliding = true;
                        cb.colliding = true;

                        rs.marker_points.push_back(c->pa);
                        rs.marker_points.push_back(c->pb);
                    }
                }
            }
        }
    }
}