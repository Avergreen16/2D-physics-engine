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

avie_matrix empty(std::size_t columns, std::size_t rows) {
    avie_matrix matrix(columns, rows);

    for(int c = 0; c < columns; ++c) {
        for(int r = 0; r < rows; ++r) {
            matrix(c, r) = 0.0f;
        }
    }

    return matrix;
}

avie_matrix identity(std::size_t columns, std::size_t rows) {
    avie_matrix matrix(columns, rows);

    for(int c = 0; c < columns; ++c) {
        for(int r = 0; r < rows; ++r) {
            if(c == r) matrix(c, r) = 1.0f;
            else matrix(c, r) = 0.0f;
        }
    }

    return matrix;
}

avie_matrix transpose(avie_matrix& input) {
    avie_matrix result(input.rows, input.columns);

    for(int c = 0; c < input.columns; ++c) {
        for(int r = 0; r < input.rows; ++r) {
            result(r, c) = input(c, r);
        }
    }

    return result;
}

avie_matrix transpose(avie_matrix&& input) {
    avie_matrix result(input.rows, input.columns);

    for(int c = 0; c < input.columns; ++c) {
        for(int r = 0; r < input.rows; ++r) {
            result(r, c) = input(c, r);
        }
    }

    return result;
}

avie_matrix operator*(avie_matrix a, avie_matrix b) {
    std::size_t c0 = a.columns;
    std::size_t r0 = a.rows;
    std::size_t c1 = b.columns;
    std::size_t r1 = b.rows;

    avie_matrix result = empty(c1, r0);

    for(int ra = 0; ra < r0; ++ra) {
        for(int cb = 0; cb < c1; ++cb) {
            for(int v = 0; v < r1; ++v) {
                double vv = a(v, ra) * b(cb, v);
                result(cb, ra) += vv;
            }
        }
    }

    return result;
}

avie_matrix clip(uvec2 range, avie_matrix a) {
    avie_matrix clipped_matrix(1, range.y);

    for(int i = 0; i < range.y; ++i) {
        clipped_matrix(0, i) = a(0, i + range.x);
    }

    return clipped_matrix;
}

avie_matrix clip(uvec4 range, avie_matrix a) {
    avie_matrix clipped_matrix(range.z, range.w);

    for(int j = 0; j < range.w; ++j) {
        for(int i = 0; i < range.z; ++i) {
            clipped_matrix(i, j) = a(i + range.x, j + range.y);
        }
    }

    return clipped_matrix;
}

void write(avie_matrix matrix) {
    std::size_t columns = matrix.columns;
    std::size_t rows = matrix.rows;
    std::string write_string;

    for(int r = 0; r < rows; ++r) {
        for(int c = 0; c < columns; ++c) {
            write_string += std::to_string(matrix(c, r));
            if(c != columns - 1) write_string += " ";
        }
        if(r != rows - 1) write_string += "\n";
    }

    std::cout << write_string;
}

void write(block_sparse_matrix& matrix) {
    std::size_t columns = matrix.columns.size();
    std::size_t rows = matrix.rows.size();
    std::vector<std::string> strings;

    for(int r = 0; r < rows; ++r) {
        for(int c = 0; c < columns; ++c) {
            if(matrix.columns[c].contains(r)) {
                auto& sub_matrix = matrix.matrices[{c, r}];

                for(int r2 = 0; r2 < sub_matrix.rows; ++r2) {
                    for(int c2 = 0; c2 < sub_matrix.columns; ++c2) {
                        int row = matrix.row_widths[r].x + r2;
                        if(strings.size() <= row) strings.resize(row + 1);

                        if(isnan(matrix.matrices[{c, r}](c2, r2))) strings[row] += "N ";
                        else strings[row] += "X ";
                    }
                }
            } else {
                for(int r2 = 0; r2 < matrix.row_widths[r].y; ++r2) {
                    for(int c2 = 0; c2 < matrix.column_widths[c].y; ++c2) {
                        int row = matrix.row_widths[r].x + r2;
                        if(strings.size() <= row) strings.resize(row + 1);

                        strings[row] += "- ";
                    }
                }
            }
        }
    }

    std::string write_string;
    for(std::string str : strings) {
        write_string += str + "\n";
    }

    std::cout << write_string;
}

vec2 get_error(avie_matrix& a, avie_matrix& b) {
    vec2 error = vec2(0.0f);

    for(int r = 0; r < a.rows; ++r) {
        for(int c = 0; c < a.columns; ++c) {
            float av = a(c, r);
            float bv = b(c, r);
            if(av == 0.0f) error.y = max(error.y, bv);
            else error.x = max(error.x, abs(bv - av) / av);
        }
    }

    return error;
}

avie_matrix UTDU_solve(avie_matrix A, avie_matrix b) {
    // assumes A is square and b.rows == A.columns == A.rows
    avie_matrix U = identity(A.columns, A.rows);
    avie_matrix D = identity(A.columns, A.rows);

    for(int i = 0; i < A.columns; ++i) {
        D(i, i) = A(i, i);
        for(int k = 0; k <= i - 1; ++k) {
            D(i, i) -= U(i, k) * U(i, k) * D(k, k);
        }

        for(int j = i + 1; j < A.columns; ++j) {
            U(j, i) = A(j, i);

            for(int k = 0; k <= i - 1; ++k) {
                U(j, i) -= U(j, k) * U(i, k) * D(k, k);
            }

            U(j, i) /= D(i, i);
        }
    }


    // UT substitution (UT * z = b)
    avie_matrix z = empty(1, A.columns);
    for(int i = 0; i < A.columns; ++i) {
        float total = 0;
        for(int j = 0; j < i; ++j) total += z(0, j) * U(i, j);

        z(0, i) = b(0, i) - total;
    }

    // D substitution (D * y = z);
    avie_matrix y = empty(1, A.columns);
    for(int i = 0; i < A.columns; ++i) {
        y(0, i) = z(0, i) / D(i, i);
    }

    // U substitution (U * x = y)
    avie_matrix x = empty(1, A.columns);
    for(int i = A.columns - 1; i >= 0; --i) {
        float total = 0;
        for(int j = i + 1; j < A.columns; ++j) total += x(0, j) * U(j, i);
        x(0, i) = y(0, i) - total;
    }

    return x;
}

avie_matrix invert(avie_matrix A) {
    // assumes A is square and b.rows == A.columns == A.rows
    avie_matrix U = identity(A.columns, A.rows);
    avie_matrix D = identity(A.columns, A.rows);

    for(int i = 0; i < A.columns; ++i) {
        D(i, i) = A(i, i);
        for(int k = 0; k <= i - 1; ++k) {
            D(i, i) -= U(i, k) * U(i, k) * D(k, k);
        }

        for(int j = i + 1; j < A.columns; ++j) {
            U(j, i) = A(j, i);

            for(int k = 0; k <= i - 1; ++k) {
                U(j, i) -= U(j, k) * U(i, k) * D(k, k);
            }

            U(j, i) /= D(i, i);
        }
    }

    avie_matrix Ainv = empty(A.columns, A.rows);
    avie_matrix b = empty(1, A.rows);
    for(int i = 0; i < b.rows; ++i) {
        if(i) b(0, i - 1) = 0;
        b(0, i) = 1;

        // UT substitution (UT * z = b)
        avie_matrix z = empty(1, A.columns);
        for(int i = 0; i < A.columns; ++i) {
            float total = 0;
            for(int j = 0; j < i; ++j) total += z(0, j) * U(i, j);

            z(0, i) = b(0, i) - total;
        }

        // D substitution (D * y = z);
        avie_matrix y = empty(1, A.columns);
        for(int i = 0; i < A.columns; ++i) {
            y(0, i) = z(0, i) / D(i, i);
        }

        // U substitution (U * x = y)
        avie_matrix x = empty(1, A.columns);
        for(int i = A.columns - 1; i >= 0; --i) {
            float total = 0;
            for(int j = i + 1; j < A.columns; ++j) total += x(0, j) * U(j, i);
            x(0, i) = y(0, i) - total;
        }

        for(int j = 0; j < A.columns; ++j) Ainv(i, j) = x(0, j); 
    }

    return Ainv;
}

avie_matrix operator*(block_sparse_matrix& A, avie_matrix b) {
    avie_matrix r = empty(1, b.rows);
    
    for(uint32_t i = 0; i < A.columns.size(); ++i) {
        uvec2 column_range = A.column_widths[i];
        avie_matrix clip_b = clip(column_range, b);

        for(uint32_t j : A.columns[i]) {
            uvec2 range_x = A.column_widths[i];
            uvec2 range_y = A.row_widths[j];

            uvec2 coord = {i, j};
            if(A.matrices.contains(coord)) {
                avie_matrix& mat = A.matrices[coord];

                avie_matrix result = mat * clip_b;

                for(int k = 0; k < range_y.y; ++k) {
                    r(0, range_y.x + k) += result(0, k);
                }
            }
        }
    }

    return r;
}

block_sparse_matrix operator*(block_sparse_matrix& A, block_sparse_matrix& B) {
    block_sparse_matrix result;

    for(int ra = 0; ra < A.rows.size(); ++ra) {
        for(int cb = 0; cb < B.columns.size(); ++cb) {
            for(int v = 0; v < A.columns.size(); ++v) {
                uvec2 va = {v, ra};
                uvec2 vb = {cb, v};

                if(A.rows[ra].contains(v) && B.columns[cb].contains(v)) {
                    avie_matrix m = A.matrices[va] * B.matrices[vb];

                    uvec2 new_v = {cb, ra};

                    if(!result.matrices.contains(new_v)) result.insert(new_v, empty(m.columns, m.rows));

                    result.matrices[new_v] += m;
                }
            }
        }
    }

    result.compute_ranges();

    return result;
}

vec2 get_error(block_sparse_matrix& A, block_sparse_matrix& B) {
    vec2 result = {0, 0};

    for(int y0 = 0; y0 < A.rows.size(); ++y0) {
        for(int x0 = 0; x0 < A.columns.size(); ++x0) {
            uvec2 v = {x0, y0};

            if(A.matrices.contains(v) && B.matrices.contains(v)) {
                avie_matrix& am = A.matrices[v];
                avie_matrix& bm = B.matrices[v];

                for(int y1 = 0; y1 < am.rows; ++y1) {
                    for(int x1 = 0; x1 < am.columns; ++x1) {
                        float a = am(x1, y1);
                        float b = bm(x1, y1);

                        if(a == 0.0f) {
                            result.y = max(result.y, b);
                        } else {
                            result.x = max(result.x, abs(b - a) / a);
                        }
                    }
                }
            } else if(A.matrices.contains(v)) {
                avie_matrix& am = A.matrices[v];

                for(int y1 = 0; y1 < am.rows; ++y1) {
                    for(int x1 = 0; x1 < am.columns; ++x1) {
                        float a = am(x1, y1);

                        result.y = max(result.y, a);
                    }
                }
            } else if(B.matrices.contains(v)) {
                avie_matrix& bm = B.matrices[v];

                for(int y1 = 0; y1 < bm.rows; ++y1) {
                    for(int x1 = 0; x1 < bm.columns; ++x1) {
                        float b = bm(x1, y1);

                        result.y = max(result.y, b);
                    }
                }
            }
        }
    }

    return result;
}

block_sparse_matrix transpose(block_sparse_matrix& A) {
    block_sparse_matrix result;

    for(auto& [u, mat] : A.matrices) {
        result.insert({u.y, u.x}, transpose(mat));
    }

    result.compute_ranges();

    return result;
}


//

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

    struct transform_cache {
        vec2 position;
        mat2 orientation;
    };

    std::map<uint32_t, transform_cache> cache;

    for(uint32_t entity : collectors[0].entities) {
        Transform& ta = ecs.get_component<Transform>(entity);
        Collider& ca = ecs.get_component<Collider>(entity);

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
        
        transform_cache c;

        c.position = ta.position;
        c.orientation = ta.orientation;

        cache.emplace(entity, c);
    }

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
        position_solve(collision_constraints);
        //profiler.step("solve positions");
        //velocity_solve(collision_constraints);
        //profiler.step("solve velocity");
    }
    friction_solve(collision_constraints);
    
    // compute velocities
    for(uint32_t entity : collectors[0].entities) {
        Transform& ta = ecs.get_component<Transform>(entity);
        Collider& ca = ecs.get_component<Collider>(entity);

        transform_cache& c = cache[entity];

        if(!ca.is_static) {
            vec2 delta_pos = (ta.position - c.position);
            mat2 rot_o = ta.orientation * transpose(c.orientation);
            vec2 r = rot_o * vec2(1, 0);

            float delta_rot = atan2(r.y, r.x);

            ca.velocity += delta_pos / physics_step;
            ca.angular_velocity += delta_rot / physics_step;
        }
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
    if(c->allow_rotation) c->angular_velocity += cross(vec3(point, 0.0f), vec3(impulse, 0.0f)).z / c->inertia;
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
    float spring = 0.1f;
    float softness = 0.1f;
    float spring_constraint = 0.5f;
    float softness_constraint = 0.03f;
    float fraction = 0.75f;

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

                if(-cc.baumgarte > 0.0f) {
                    float inertia = cc.inertiaN;
                    //cc.baumgarte += slop;

                    if(cc.d->b == NULL_ENTITY) {
                        vec2 direction = cc.normal;
                        float wa = cc.inertiaNa;

                        float compliance = 0.0001f;

                        vec2 delta_a = -1.0f / (wa + compliance / (physics_step * physics_step)) * direction * cc.baumgarte;

                        cc.lambdaN = length(delta_a);

                        apply_position(data.ca, data.ta, delta_a, cc.pa - data.ta->position);
                    } else {
                        vec2 direction = cc.normal;
                        float wa = cc.inertiaNa;
                        float wb = cc.inertiaNb;

                        float compliance = 0.0001f;

                        vec2 delta_b = 1.0f / (wa + wb + compliance / (physics_step * physics_step)) * direction * cc.baumgarte;
                        vec2 delta_a = -1.0f / (wa + wb + compliance / (physics_step * physics_step)) * direction * cc.baumgarte;
                        
                        cc.lambdaN = length(delta_a);

                        apply_position(data.ca, data.ta, delta_a, cc.pa - data.ta->position);
                        apply_position(data.cb, data.tb, delta_b, cc.pb - data.tb->position);   
                    }
                }

                cc.normal_lambda = cc.lambdaN;
            }
        }

        for(Constraint& data : constraints) {
            for(pos_constraint& c : data.pos) {
                data.refresh(c);

                for(int i = 0; i < c.inertia.size(); ++i) {
                    float inertia = c.inertia[i];

                    if(data.b == NULL_ENTITY) {
                        vec2 direction = c.vs[i];
                        float wa = c.inertia_a[i];

                        float compliance = 0.00001f;

                        vec2 delta_a = -1.0f / (wa + compliance / (physics_step * physics_step)) * direction * c.baumgarte[i];

                        apply_position(data.ca, data.ta, delta_a, c.pa - data.ta->position);
                    } else {
                        vec2 direction = c.vs[i];
                        float wa = c.inertia_a[i];
                        float wb = c.inertia_b[i];

                        float compliance = 0.00001f;

                        vec2 delta_b = 1.0f / (wa + wb + compliance / (physics_step * physics_step)) * direction * c.baumgarte[i];
                        vec2 delta_a = -1.0f / (wa + wb + compliance / (physics_step * physics_step)) * direction * c.baumgarte[i];

                        apply_position(data.ca, data.ta, delta_a, c.pa - data.ta->position);
                        apply_position(data.cb, data.tb, delta_b, c.pb - data.tb->position);   
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

    for(Collision_constraint& c : collisions) {
        for(col_constraint& cc : c.constraints) {
            cc.d->prev_lambdaN = cc.lambdaN;
            cc.d->prev_lambdaT = cc.lambdaT;
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

    /*
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
    }*/

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
    
    for(Featherstone_constraint& c : constraints_featherstone) {
        c.init();
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
            c.baumgarte = 0.0f;//max(c.baumgarte, -baumgarte_max);

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
        /*
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
        */
        
        for(Collision_constraint& data : collisions) {
            for(col_constraint& cc : data.constraints) {
                float inertia = cc.inertiaN;

                vec2 velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position);

                float diff = (cc.baumgarte) * spring * factor;

                if(cc.d->b == NULL_ENTITY) {
                    // friction

                    float normal_magnitude = cc.lambdaN;

                    velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position);

                    vec2 tangent_vector = vec2(cc.d->normal.y, -cc.d->normal.x);
                    float tangent_velocity = dot(velocity, tangent_vector);

                    
                    float inverse_mass = cc.inertiaT;

                    float mu = 0.9f;

                    float max_friction = abs(mu * cc.lambdaN);

                    float new_lambdaT = cc.lambdaT - tangent_velocity / inverse_mass;
                    new_lambdaT = clamp(new_lambdaT, -max_friction, max_friction);
                    float L = new_lambdaT - cc.lambdaT;
                    cc.lambdaT = new_lambdaT;

                    float Pt = L;

                    vec2 friction_impulse = tangent_vector * Pt;

                    apply_impulse(data.ca, friction_impulse, cc.pa - data.ta->position);
                } else {
                    // friction

                    float normal_magnitude = length(cc.lambdaN);

                    velocity = calculate_point_velocity(data.ca, cc.pa - data.ta->position) - calculate_point_velocity(data.cb, cc.pb - data.tb->position);
                    float tangent_velocity = dot(velocity, cc.tangent);
                    
                    inertia = cc.inertiaT;
                    
                    float mu = 0.9f;

                    float max_friction = abs(mu * cc.lambdaN);

                    float new_lambdaT = cc.lambdaT - tangent_velocity / inertia;
                    new_lambdaT = clamp(new_lambdaT, -max_friction, max_friction);
                    float L = new_lambdaT - cc.lambdaT;
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

void Physics_system::friction_solve(std::vector<Collision_constraint>& collisions) {
    int iterations = 6;

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
            c.baumgarte = 0.0f;//max(c.baumgarte, -baumgarte_max);

            c.lambdaN = c.d->prev_lambdaN;
            c.lambdaT = c.d->prev_lambdaT;

            if(data.b == NULL_ENTITY) {
                vec2 friction_impulse = c.tangent * c.lambdaT;

                apply_impulse(data.ca, friction_impulse, c.pa - data.ta->position);
            } else {                
                vec2 friction_impulse = c.tangent * c.lambdaT;

                apply_impulse(data.ca, friction_impulse, c.pa - data.ta->position);
                apply_impulse(data.cb, -friction_impulse, c.pb - data.tb->position);
            }
        }
    }

    for(int i = 0; i < iterations; ++i) {
        for(Collision_constraint& data : collisions) {
            for(col_constraint& cc : data.constraints) {
                data.refresh(cc);

                vec2 direction = vec2(cc.d->normal.y, -cc.d->normal.x);
                float diff = dot(direction, cc.pa - cc.pb);
                float mu = 1.0f;

                diff = sign(diff) * max(abs(diff), abs(cc.lambdaN * mu));

                if(cc.d->b == NULL_ENTITY) {
                    float wa = cc.inertiaTa;

                    float compliance = 0.00001f;

                    vec2 delta_a = -1.0f / (wa + compliance / (physics_step * physics_step)) * direction * diff;

                    apply_position(data.ca, data.ta, delta_a, cc.pa - data.ta->position);
                } else {
                    float wa = cc.inertiaTa;
                    float wb = cc.inertiaTb;

                    float compliance = 0.00001f;

                    vec2 delta_b = 1.0f / (wa + wb + compliance / (physics_step * physics_step)) * direction * diff;
                    vec2 delta_a = -1.0f / (wa + wb + compliance / (physics_step * physics_step)) * direction * diff;

                    apply_position(data.ca, data.ta, delta_a, cc.pa - data.ta->position);
                    apply_position(data.cb, data.tb, delta_b, cc.pb - data.tb->position);   
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

vec2 angular_to_linear(vec2 pos, float angular_velocity) {
    return vec2(pos.y, -pos.x) * angular_velocity;
}

void Featherstone_constraint::init() {
    // node tree construction
    Input_system& is = ecs.get_system<Input_system>();
    constraints = local_constraints;
    if(is.held_constraint != NULL_ENTITY) {
        uint32_t i = 0;
        for(uint32_t entity : entities) {
            if(entity == is.held_object) {
                Constraint& cs = ecs.get_system<Physics_system>().constraints[is.held_constraint];

                constraints.push_back(cs.pos[0]);
                constraints[constraints.size() - 1].is_hold = true;
                break;
            }
            ++i;
        }
    }

    // IT CREATES A BRANCH

    nodes.clear();
    //if(!nodes.size()) {
        for(uint32_t e : entities) {
            node n;
            n.id = nodes.size();
            n.parent = NULL_ENTITY;
            
            n.matrix_index = e;

            nodes.push_back(n);
        }

        uint32_t i = 0;
        for(auto& constraint : constraints) {
            if(constraint.is_hold) {
                uint32_t child = 0;
                for(uint32_t entity : entities) {
                    if(entity == is.held_object) {
                        break;
                    }
                    ++child;
                }
            
                node n;
                n.is_body = false;
                n.id = nodes.size();

                n.parent = NULL_ENTITY;
                n.children.push_back(child);
                
                n.matrix_index = i;
                
                nodes.push_back(n);
                
                ++i;
            } else {
                node n;
                n.is_body = false;
                n.id = nodes.size();

                uint32_t a = i;
                uint32_t b = i + 1;

                n.parent = min(a, b);
                n.children.push_back(max(a, b));
                nodes[n.parent].children.push_back(n.id);
                
                n.matrix_index = i;
                
                nodes.push_back(n);
                
                ++i;
            }
            
        }
        
        // fill in parent
        for(node& n : nodes) {
            for(uint32_t c : n.children) {
                nodes[c].parent = n.id;
            }
        }

        node* current_node;
        for(node& n : nodes) {
            if(n.parent == NULL_ENTITY) {
                current_node = &n;
            }
        }

        int depth = 0;
        std::vector<uint32_t> path = {0};
        while(true) {
            if(current_node->children.size()) {
                ++depth;
                path.push_back(0);
                current_node = &nodes[current_node->children[0]];
            } else break;
        }
        while(true) {
            if(current_node->children.size() <= path.back()) {
                from_order.emplace(to_order.size(), current_node->id);
                to_order.emplace(current_node->id, to_order.size());

                if(path.size() > depth) path.pop_back();
                --depth;

                if(current_node->parent == NULL_ENTITY) break;
                
                current_node = &nodes[current_node->parent];
                ++path[path.size() - 1];
            } else {
                ++depth;
                current_node = &nodes[current_node->children[path.back()]];
                path.push_back(0);
            }
        }
    //}

    // matrix ordering

    // jacobian creation

    jacobians.clear();
    std::unordered_map<uint32_t, avie_matrix> mass_matrices;

    for(node& n : nodes) {
        if(n.is_body) {
            uint32_t a = entities[n.id];
            Collider* ca = &ecs.get_component<Collider>(a);
            Transform* ta = &ecs.get_component<Transform>(a);

            avie_matrix mm = empty(3, 3);
            mm(0, 0) = ca->mass;
            mm(1, 1) = ca->mass;
            mm(2, 2) = ca->inertia;

            mass_matrices.emplace(n.id, mm);
        } else {
            if(n.parent == NULL_ENTITY) {
                uint32_t a = entities[n.children[0]];
                pos_constraint& pc = constraints[n.matrix_index];
                
                Collider* ca = &ecs.get_component<Collider>(a);
                Transform* ta = &ecs.get_component<Transform>(a);

                pc.pa = ta->orientation * pc.a + ta->position;
                pc.pb = pc.b;

                avie_matrix j0 = empty(3, pc.vs.size());
                
                uint32_t vi = 0;
                for(auto v : pc.vs) { // compute jacobians
                    vec2 rel_pa = pc.pa - ta->position;
                    float ra = cross(vec3(rel_pa, 0), vec3(v, 0)).z;

                    j0(0, vi) = v.x;
                    j0(1, vi) = v.y;
                    j0(2, vi) = ra;

                    ++vi;
                }

                uvec2 j0_pos = {n.children[0], n.id};

                jacobians.emplace(j0_pos, j0);
            } else {
                uint32_t a = entities[n.parent];
                uint32_t b = entities[n.children[0]];
                pos_constraint& pc = constraints[n.matrix_index];
                
                Collider* ca = &ecs.get_component<Collider>(a);
                Transform* ta = &ecs.get_component<Transform>(a);
                Collider* cb = &ecs.get_component<Collider>(b);
                Transform* tb = &ecs.get_component<Transform>(b);

                pc.pa = ta->orientation * pc.a + ta->position;

                if(b != NULL_ENTITY) {
                    pc.pb = tb->orientation * pc.b + tb->position;
                } else {
                    pc.pb = pc.b;
                }

                avie_matrix j0 = empty(3, pc.vs.size());
                avie_matrix j1 = empty(3, pc.vs.size());
                
                uint32_t vi = 0;
                for(auto v : pc.vs) { // compute jacobians
                    vec2 rel_pa = pc.pa - ta->position;
                    float ra = cross(vec3(rel_pa, 0), vec3(-v, 0)).z;

                    vec2 rel_pb = pc.pb - tb->position;
                    float rb = cross(vec3(rel_pb, 0), vec3(v, 0)).z;

                    j0(0, vi) = -v.x;
                    j0(1, vi) = -v.y;
                    j0(2, vi) = ra;
                    
                    j1(0, vi) = v.x;
                    j1(1, vi) = v.y;
                    j1(2, vi) = rb;

                    ++vi;
                }

                uvec2 j0_pos = {n.parent, n.id};
                uvec2 j1_pos = {n.children[0], n.id};

                jacobians.emplace(j0_pos, j0);
                jacobians.emplace(j1_pos, j1);
            }
        }
    }

    // build sparse matrix

    H.clear();

    for(node n : nodes) {
        uint32_t ii = to_order[n.id];

        if(n.is_body) {
            H.insert({ii, ii}, mass_matrices[n.id]);

            if(n.parent != NULL_ENTITY) {
                avie_matrix& jacobian = jacobians[{n.id, n.parent}];
                jacobian = transpose(jacobian);

                uvec2 pos = {to_order[n.parent], ii};

                H.insert(pos, -jacobian);
                H.insert({pos.y, pos.x}, -transpose(jacobian));
            }
        } else {
            if(n.parent != NULL_ENTITY) {
                avie_matrix& jacobian = jacobians[{n.parent, n.id}];

                uvec2 pos = {to_order[n.parent], ii};

                H.insert(pos, -jacobian);
                H.insert({pos.y, pos.x}, -transpose(jacobian));
            }
        }
    }
    H.compute_ranges();

    // solve for D and U
    block_sparse_matrix D;
    Dn.clear();
    U.clear();
    D.clear();

    for(int i = 0; i < nodes.size(); ++i) {
        node& n = nodes[from_order[i]];

        avie_matrix Di;
        if(n.is_body) Di = H.matrices[{i, i}];
        else {
            pos_constraint& constraint = constraints[n.matrix_index];
            Di = empty(constraint.vs.size(), constraint.vs.size());
        }

        for(uint32_t child : n.children) {
            uint32_t j = to_order[child];

            Di -= transpose(U.matrices[{i, j}]) * D.matrices[{j, j}] * U.matrices[{i, j}];
        }

        avie_matrix Dinv = invert(Di);
        
        if(n.parent != NULL_ENTITY) {
            uint32_t j = to_order[n.parent];
            avie_matrix U_ji = Dinv * H.matrices[{j, i}];
            U.insert({j, i}, U_ji);
        }

        U.insert({i, i}, identity(Di.columns, Di.rows));
        Dn.insert({i, i}, Dinv);
        D.insert({i, i}, Di);
    }

    U.compute_ranges();
    Dn.compute_ranges();
    D.compute_ranges();
}

void Featherstone_constraint::solve() {
    // now the fun part
    // getting x (which is [y, lambda])

    // build b
    uint32_t num_bodies = entities.size();
    uint32_t num_constraints = 0;
    for(auto& constraint : constraints) {
        num_constraints += constraint.vs.size();
    }
    avie_matrix B = empty(1, H.size.y);

    uint32_t i = 0;
    for(auto [index, node_id] : from_order) {
        node& n = nodes[node_id];

        if(n.is_body) {
            i += 3;
        } else {
            if(n.parent == NULL_ENTITY) {
                pos_constraint& constraint = constraints[n.matrix_index];

                node& na = nodes[n.children[0]];

                uint32_t a = na.matrix_index;

                Collider* ca = &ecs.get_component<Collider>(a);
                Transform* ta = &ecs.get_component<Transform>(a);

                // now get the amount the velocity is violating the constraint (and maybe throw in a lil bit of baumgarte :3)
                
                vec2 pv_a = Physics_system::calculate_point_velocity(ca, constraint.pa - ta->position);

                vec2 rel_velocity = pv_a;
                vec2 rel = constraint.pa - constraint.pb;

                for(vec2 v : constraint.vs) {
                    B(0, i) = (dot(rel_velocity, v) + dot(v, rel) / 0.02f * 0.35f);

                    ++i;
                }
            } else {
                pos_constraint& constraint = constraints[n.matrix_index];

                node& na = nodes[n.parent];
                node& nb = nodes[n.children[0]];

                uint32_t a = na.matrix_index;
                uint32_t b = nb.matrix_index;

                Collider* ca = &ecs.get_component<Collider>(a);
                Transform* ta = &ecs.get_component<Transform>(a);
                Collider* cb = &ecs.get_component<Collider>(b);
                Transform* tb = &ecs.get_component<Transform>(b);

                // now get the amount the velocity is violating the constraint (and maybe throw in a lil bit of baumgarte :3)
                
                vec2 pv_a = Physics_system::calculate_point_velocity(ca, constraint.pa - ta->position);
                vec2 pv_b = Physics_system::calculate_point_velocity(cb, constraint.pb - tb->position);

                vec2 rel_velocity = pv_b - pv_a;
                vec2 rel = constraint.pb - constraint.pa;

                for(vec2 v : constraint.vs) {
                    B(0, i) = (dot(rel_velocity, v) + dot(v, rel) / 0.02f * 1.0f);

                    ++i;
                }
            }
        }
    }

    // UT substitution (UT * z = b)
    avie_matrix z = empty(1, B.rows);
    for(int i = 0; i < U.columns.size(); ++i) {
        uvec2 range = U.column_widths[i];

        avie_matrix total = empty(1, range.y);

        for(int j = 0; j < i; ++j) {
            if(U.rows[j].contains(i)) {
                uvec2 range_row = U.row_widths[j];

                avie_matrix& matrix = U.matrices[{i, j}];
                avie_matrix z_clip = clip(range_row, z);
                total += transpose(matrix) * z_clip;
            }
        }
        
        for(int k = 0; k < range.y; ++k) {
            z(0, range.x + k) = B(0, range.x + k) - total(0, k);
        }
    }

    // D substitution (D * y = z);
    avie_matrix y = empty(1, B.rows);
    for(int i = 0; i < U.columns.size(); ++i) {
        uvec2 range = U.column_widths[i];

        avie_matrix zm = clip(range, z);
        zm = Dn.matrices[{i, i}] * zm;

        for(int k = 0; k < range.y; ++k) {
            y(0, range.x + k) = zm(0, k);
        }
    }

    // U substitution (U * x = y)
    avie_matrix x = empty(1, B.rows);
    for(int i = U.rows.size() - 1; i >= 0; --i) {
        uvec2 range = U.row_widths[i];
        
        avie_matrix total = empty(1, range.y);

        for(int j = i + 1; j < U.columns.size(); ++j) {
            uvec2 range_column = U.column_widths[j];

            if(U.rows[i].contains(j)) {
                auto& matrix = U.matrices[{j, i}];
                avie_matrix x_clip = clip(range_column, x);
                total += matrix * x_clip;
            }
        }

        for(int k = 0; k < range.y; ++k) {
            x(0, range.x + k) = y(0, range.x + k) - total(0, k);
        }
    }

    for(node& n : nodes) {
        if(!n.is_body) {
            if(n.parent == NULL_ENTITY) {
                uint32_t i = to_order[n.id];
                vec2 range = U.column_widths[i];

                pos_constraint& constraint = constraints[n.matrix_index];

                node& child = nodes[n.children[0]];

                uint32_t a = child.matrix_index;
                
                Collider* ca = &ecs.get_component<Collider>(a);
                Transform* ta = &ecs.get_component<Transform>(a);
        
                avie_matrix& jacobian_a = jacobians[{child.id, n.id}];

                bool c = false;
                uint32_t ii = 0;
                for(vec2 v : constraint.vs) {
                    float lambda = x(0, range.x + ii);

                    vec2 impulse = v * lambda;

                    if(!isnan(lambda)) {
                        c = true;

                        Physics_system::apply_impulse(ca, impulse, constraint.pa - ta->position);

                        ++ii;
                    }
                }
                
                if(c) {
                    //std::cout << "\n\n";
                }
            } else {
                uint32_t i = to_order[n.id];
                vec2 range = U.column_widths[i];

                pos_constraint& constraint = constraints[n.matrix_index];

                node& parent = nodes[n.parent];
                node& child = nodes[n.children[0]];

                uint32_t a = parent.matrix_index;
                uint32_t b = child.matrix_index;
                
                Collider* ca = &ecs.get_component<Collider>(a);
                Transform* ta = &ecs.get_component<Transform>(a);
                Collider* cb = &ecs.get_component<Collider>(b);
                Transform* tb = &ecs.get_component<Transform>(b);
        
                avie_matrix& jacobian_a = jacobians[{parent.id, n.id}];
                avie_matrix& jacobian_b = jacobians[{child.id, n.id}];

                bool c = false;
                uint32_t ii = 0;
                for(vec2 v : constraint.vs) {
                    float lambda = x(0, range.x + ii);

                    vec2 impulse = v * lambda;

                    if(!isnan(lambda)) {
                        //std::cout << lambda << "\n";
                        c = true;

                        //ca->velocity += vec2(jacobian_a(0, ii), jacobian_a(1, ii)) * lambda / ca->mass;
                        //ca->angular_velocity += jacobian_a(2, ii) * lambda / ca->inertia;
                        
                        //cb->velocity += vec2(jacobian_b(0, ii), jacobian_b(1, ii)) * lambda / cb->mass;
                        //cb->angular_velocity += jacobian_b(2, ii) * lambda / cb->inertia;

                        Physics_system::apply_impulse(ca, -impulse, constraint.pa - ta->position);
                        Physics_system::apply_impulse(cb, impulse, constraint.pb - tb->position);

                        ++ii;
                    }
                }
                
                if(c) {
                    //std::cout << "\n\n";
                }
            }
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
        c.inertiaTa = Physics_system::calculate_inverse_mass(ca, ta, c.tangent, c.pa - ta->position);

        c.inertiaN = c.inertiaNa;
        c.inertiaT = c.inertiaTa;
        
        if(b != NULL_ENTITY) {
            c.inertiaNb = Physics_system::calculate_inverse_mass(cb, tb, c.normal, c.pb - tb->position);
            c.inertiaTb = Physics_system::calculate_inverse_mass(cb, tb, c.tangent, c.pb - tb->position);

            c.inertiaN += c.inertiaNb;
            c.inertiaT += c.inertiaTb;
        }
    }
}

void Collision_constraint::refresh(col_constraint& c) {
    if(ca->flag) {
        vec2 point_a = ta->orientation * c.d->pa + ta->position;
        c.pa = point_a;

        c.inertiaNa = Physics_system::calculate_inverse_mass(ca, ta, c.normal, c.pa - ta->position);
        c.inertiaTa = Physics_system::calculate_inverse_mass(ca, ta, c.tangent, c.pa - ta->position);
    }

    if(c.d->b == NULL_ENTITY) {
        c.pb = c.d->pb;
    } else {
        if(cb->flag) {
            vec2 point_b = tb->orientation * c.d->pb + tb->position;
            c.pb = point_b;
            c.inertiaNb = Physics_system::calculate_inverse_mass(cb, tb, c.normal, c.pb - tb->position);
            c.inertiaTb = Physics_system::calculate_inverse_mass(cb, tb, c.tangent, c.pb - tb->position);
        }
    }

    if((c.d->b == NULL_ENTITY) ? ca->flag : (ca->flag || cb->flag)) {
        vec2 diff = c.pa - c.pb;
        float dd = dot(diff, c.d->normal);
        c.baumgarte = dd;

        c.inertiaN = c.inertiaNa + c.inertiaNb;
        c.inertiaT = c.inertiaTa + c.inertiaTb;

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