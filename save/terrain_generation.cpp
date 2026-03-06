#include "terrain_generation.hpp"

#include "input.hpp"

#include "stb_image.h"
#include "stb_image_write.h"

std::vector<ivec2> indices = {
    {0, 0}, {1, 0}, {1, 1},
    {0, 0}, {1, 1}, {0, 1}
};

std::string filepath;

vec3 find_circumcenter(vec3 a, vec3 b, vec3 c) {
    vec3 normal = normalize(cross(a - c, b - c));
    mat3 rotate = rotate_to(normal, vec3(0.0, 0.0, 1.0));

    vec2 pa_prime = vec2(rotate * (a - c));
    vec2 pb_prime = vec2(rotate * (b - c));
    vec2 pc_prime = vec2(0.0f);

    vec2 midpoint_ac = pa_prime * 0.5f;
    vec2 midpoint_bc = pb_prime * 0.5f;
    vec2 dir_ac = normalize(vec2(pa_prime.y, -pa_prime.x));
    vec2 dir_bc = normalize(vec2(pb_prime.y, -pb_prime.x));

    vec2 rel_origin = midpoint_ac - midpoint_bc;
    vec2 normal_bc = normalize(pb_prime);
    float dot_dir = dot(normal_bc, dir_ac);
    float dot_origin = dot(normal_bc, -rel_origin);
    float t = dot_origin / dot_dir;

    vec2 intersection_point = midpoint_ac + dir_ac * t;
    vec3 ip_prime = transpose(rotate) * vec3(intersection_point, 0.0) + c;

    vec3 circumcenter = ip_prime;

    return circumcenter;
}

vec3 get_color_elev(float elevation) {
    vec3 color;
    if(elevation > 2.0) color = mix(vec3(203, 183, 93) / 255.0f, vec3(1.0f), clamp((elevation - 2.0f) * 0.5f, 0.0f, 1.0f));
    else if(elevation > 0.0) color = mix(vec3(38, 144, 61) / 255.0f, vec3(203, 183, 93) / 255.0f, clamp(elevation * 0.5f, 0.0f, 1.0f));
    else color = mix(vec3(29, 66, 137) / 255.0f, vec3(29, 66, 137) / 255.0f * 0.1f, clamp(-elevation * 0.5f, 0.0f, 1.0f));

    return color;
};

vec3 hash_vec3(uint32_t a, uint32_t b) {
    vec3 output = vec3(hash_float(a ^ b), hash_float(a ^ b + 1), hash_float(a ^ b + 2));
    output = output * 2.0f - 1.0f;

    return output;
}

vec2 map_project(ivec2 position, ivec2 size) {
    vec2 coords = (vec2(position) + 0.5f) / vec2(size.xy());
    coords = coords * 2.0f - 1.0f;

    float theta = asin(coords.y);

    // lambda
    float longitude = (M_PI * coords.x) / cos(theta);

    // phi
    float latitude = asin((2.0f * theta + sin(2.0f * theta)) / M_PI);

    vec2 pos = vec2(longitude, latitude);
    pos.x = pos.x / M_PI * 0.5f + 0.5f;
    pos.y = pos.y / M_PI + 0.5f;

    return pos;
}

vec3 wrap(vec2 position) {
    float angle_y = (position.y - 0.5f) * M_PI;
    float angle_x = position.x * 2.0f * M_PI;

    float cos_y = cos(angle_y);
    vec3 pos = vec3(cos(angle_x) * cos_y, sin(angle_x) * cos_y, sin(angle_y));

    return pos;
}

void generate_planet_texture(uint32_t seed) {
    float continental_elevation = 0.625f;
    float oceanic_elevation = -0.625f;

    Random random(seed);

    vec3 color2 = vec3(0.0f);

    std::string path0 = "res/textures/biomes.png";
    ivec3 size0;
    uint8_t* data0 = stbi_load(path0.data(), &size0.x, &size0.y, &size0.z, 0);

    std::string path1 = "res/textures/biomes_ocean.png";
    ivec3 size1;
    uint8_t* data1 = stbi_load(path1.data(), &size1.x, &size1.y, &size1.z, 0);

    auto sample_texture = [&](vec2 pos, ivec3 tex_size, uint8_t* data, bool nearest = true) {
        if(nearest) {
            ivec2 origin = floor(pos * vec2(tex_size.xy()));
            ivec2 o0 = origin;
            o0 = clamp(o0, ivec2(0), tex_size.xy() - 1);
            int i0 = o0.y * tex_size.x + o0.x;

            vec3 c0 = vec3(data[i0 * tex_size.z], data[i0 * tex_size.z + 1], data[i0 * tex_size.z + 2]) / 255.0f;
            return c0;
        } else {
            ivec2 origin = floor(pos * vec2(tex_size.xy()) - 0.5f);
            vec2 blend = fract(pos * vec2(tex_size.xy()) - 0.5f);

            ivec2 o0 = origin;
            ivec2 o1 = origin + ivec2(1, 0);
            ivec2 o2 = origin + ivec2(0, 1);
            ivec2 o3 = origin + ivec2(1, 1);
            o0 = clamp(o0, ivec2(0), tex_size.xy() - 1);
            o1 = clamp(o1, ivec2(0), tex_size.xy() - 1);
            o2 = clamp(o2, ivec2(0), tex_size.xy() - 1);
            o3 = clamp(o3, ivec2(0), tex_size.xy() - 1);

            int i0 = o0.y * tex_size.x + o0.x;
            int i1 = o1.y * tex_size.x + o1.x;
            int i2 = o2.y * tex_size.x + o2.x;
            int i3 = o3.y * tex_size.x + o3.x;

            vec3 c0 = vec3(data[i0 * tex_size.z], data[i0 * tex_size.z + 1], data[i0 * tex_size.z + 2]) / 255.0f;
            vec3 c1 = vec3(data[i1 * tex_size.z], data[i1 * tex_size.z + 1], data[i1 * tex_size.z + 2]) / 255.0f;
            vec3 c2 = vec3(data[i2 * tex_size.z], data[i2 * tex_size.z + 1], data[i2 * tex_size.z + 2]) / 255.0f;
            vec3 c3 = vec3(data[i3 * tex_size.z], data[i3 * tex_size.z + 1], data[i3 * tex_size.z + 2]) / 255.0f;

            vec3 c01 = mix(c0, c1, blend.x);
            vec3 c23 = mix(c2, c3, blend.x);
            vec3 result = mix(c01, c23, blend.y);

            return result;
        }
    };

    auto get_color = [&](vec3 pos, float elev) {
        elev = elev * 4.0f;

        float temp = 1.0 - abs(dot(pos, vec3(0, 0, 1))) + Noise_gen::perlin_noise(pos, 0.2f, 3, seed) * 0.2f;
        float humidity = Noise_gen::perlin_noise(pos, 0.75f, 3, seed + 3) + 0.5f;
        
        vec3 color;
        if(elev < 0.0f) {
            color = sample_texture(vec2(humidity, 1.0 - temp), size1, data1);
        } else {
            humidity -= max(0.0, elev - 0.25) * 0.5f;
            temp -= max(0.0, elev - 0.25);

            color = sample_texture(vec2(humidity, 1.0 - temp), size0, data0);
        }

        return color;
    };
    
    /*
    int num_voronoi = 250;
    int num_plates = 15;
    int num_oceanic = 5;
    int num_major = 8;
    */

    float scale = 1.0f;

    int num_voronoi = 350 * scale;
    int num_plates = 15 * scale;
    int num_oceanic = 5 * scale;
    int num_major = 8 * scale;

    std::vector<uint32_t> plate_ids;
    std::vector<uint32_t> plate_ids2;
    for(int i = 0; i < num_plates; ++i) {
        plate_ids.push_back(i);
        plate_ids2.push_back(i);
    }
    for(int i = 0; i < num_plates; ++i) {
        int num = hash(i ^ seed) % (i + 1);
        uint32_t temp = plate_ids[i];
        plate_ids[i] = plate_ids[num];
        plate_ids[num] = temp;
        
        num = hash((i + 671) ^ seed) % (i + 1);
        temp = plate_ids2[i];
        plate_ids2[i] = plate_ids2[num];
        plate_ids2[num] = temp;
    }
    
    std::vector<tectonic_plate> plates;
    std::vector<voronoi_cell> voronoi;

    std::unordered_map<uint32_t, Connection> connections;
    std::unordered_map<uint32_t, Connection> interior_connections;
    std::unordered_map<uint32_t, std::set<uint32_t>> neighbors;
    std::vector<vec3> circumcenters;
    std::vector<vec3> points;

    std::set<uint32_t> available_cells;

    float golden_ratio = (1 + pow(5, 0.5f)) / 2.0f;
    mat3 ori = rotate_to(vec3(0.0f, 0.0f, 1.0f), random.unit_vector());
    for(int i = 0; i < num_plates; ++i) {
        tectonic_plate plate;
        plate.movement_dir = random.unit_vector() * (abs(random())) * 4.0f;
        
        if(plate_ids[i] < num_oceanic) plate.elev = -0.625f + 0.125f * random();
        else plate.elev = 0.625f + 0.125f * random();


        // get position
        float theta = 2.0f * M_PI * i * golden_ratio;
        float phi = 1.0f - 2.0f * float(i) / float(num_plates);
        phi = acos(phi);

        vec3 pos = {cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi)};
        plate.position = ori * pos;

        if(plate_ids2[i] < num_major) plate.weight = random() * 0.3f + 1.0f;
        else plate.weight = random() * 0.15f + 0.3f;

        plates.push_back(plate);
    }

    float area = 4 * M_PI;
    float avg_sep = sqrt(area / num_voronoi);
    float avg_sep2 = sqrt(area / num_plates);

    float base_growth = avg_sep2 * 0.65f;
    
    float unit = avg_sep / sqrt(4.0f * M_PI / 150);

    std::cout << area << " " << avg_sep << " " << avg_sep2 << " " << unit << " X\n";

    for(uint i = 0; i < num_plates; ++i) {
        tectonic_plate& plate = plates[i];

        // blobs

        int num_blobs = 1;
        
        float continent_scale = avg_sep2 * plate.weight * 0.5f;

        for(int j = 0; j < num_blobs; ++j) {
            Blob blob;
            blob.origin = normalize(plate.position + hash_vec3(i ^ 0x33415, j) * continent_scale * 0.35f);
            
            vec3 size = abs(hash_vec3(i ^ 0x66CC101, j));
            size[hash(i ^ 0x5501F3CD) % 3] = 1.0f;
            size *= continent_scale * 1.5f;
            blob.size = size;

            mat3 ori = rotate_to(vec3(0.0f, 0.0f, 1.0f), normalize(hash_vec3(i ^ 0x5726301, j)));
            blob.ori = ori;

            plate.blobs.push_back(blob);
        }
    }

    for(int i = 0; i < num_voronoi; ++i) {
        voronoi_cell cell;
        //float ii = float(i) / num_voronoi;
        float theta = 2.0f * M_PI * i * golden_ratio;
        float phi = 1.0f - 2.0f * i / num_voronoi;
        phi = acos(phi);

        vec3 pos = {cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi)};
        pos = normalize(pos + vec3(hash_float((i + 2000) ^ seed), hash_float((i + 8236) ^ seed), hash_float((i + 29137) ^ seed)) * avg_sep * 0.5f);

        cell.pos = pos;

        uint32_t p = 0xFFFFFFFF;
        uint32_t index = 0;
        float min_dist = FLT_MAX;
        for(auto& plate : plates) {
            float dist = length(cell.pos - plate.position) / plate.weight;

            if(dist < min_dist) {
                min_dist = dist;
                p = index;
            }

            ++index;
        }

        auto& plate = plates[p];
        if(min_dist < base_growth) {
            cell.plate = p;
            plate.cells.push_back(i);
        } else {
            available_cells.emplace(i);
        }


        voronoi.push_back(cell);
        points.push_back(cell.pos);
    }

    std::cout << "cells: " << voronoi.size() << " open: " << available_cells.size() << "\n";

    std::cout << "start\n";

    std::vector<int> edge_cells;

    std::vector<uint32_t> tri = triangulate(points);
    for(int i = 0; i < tri.size() / 3; ++i) {
        uint32_t a = tri[i * 3];
        uint32_t b = tri[i * 3 + 1];
        uint32_t c = tri[i * 3 + 2];

        uint32_t d = (max(a, b) << 16) | min(a, b);
        uint32_t e = (max(a, c) << 16) | min(a, c);
        uint32_t f = (max(b, c) << 16) | min(b, c);

        vec3 pa = points[a];
        vec3 pb = points[b];
        vec3 pc = points[c];

        vec3 circumcenter = find_circumcenter(pa, pb, pc);
        circumcenter = normalize(circumcenter);
        circumcenters.push_back(circumcenter);

        if(!connections.contains(d)) {
            Connection connection;
            connection.a = a;
            connection.b = b;
            connection.c = i;

            connections.emplace(d, connection);

            neighbors[a].emplace(b);
            neighbors[b].emplace(a);
        } else {
            Connection& connection = connections[d];
            connection.d = i;
        }

        if(!connections.contains(e)) {
            Connection connection;
            connection.a = a;
            connection.b = c;
            connection.c = i;

            connections.emplace(e, connection);

            neighbors[a].emplace(c);
            neighbors[c].emplace(a);
        } else {
            Connection& connection = connections[e];
            connection.d = i;
        }

        if(!connections.contains(f)) {
            Connection connection;
            connection.a = b;
            connection.b = c;
            connection.c = i;

            connections.emplace(f, connection);

            neighbors[b].emplace(c);
            neighbors[c].emplace(b);
        } else {
            Connection& connection = connections[f];
            connection.d = i;
        }
    }   

    auto map = [&](float a, float min0, float max0, float min1, float max1) {
        return min1 + (a - min0) * ((max1 - min1) / (max0 - min0));
    };

    auto secondary_details = [&](vec3 pos, uint32_t plate) {
        vec3 offset1 = vec3(hash_float(plate * 6 + 2736), hash_float(plate * 6 + 8263), hash_float(plate * 6 + 23626));
        vec3 offset2 = vec3(hash_float(plate * 6 + 5216), hash_float(plate * 6 + 926), hash_float(plate * 6 + 10113));
        float ridged_noise = Noise_gen::ridged_perlin_noise(pos + offset1, 0.3f, 1, seed);
        float mask = Noise_gen::perlin_noise(pos + offset2, 0.4f, 3, seed, 0.6f);
        float detail = Noise_gen::perlin_noise(pos + offset1 * 5.0f, 0.035f, 3, seed);

        ridged_noise = 1.0f - ridged_noise;
        ridged_noise = min(ridged_noise * 3.0f, 1.0f);
        ridged_noise = 1.0f - ridged_noise;

        float continent_noise = (Noise_gen::perlin_noise(pos + offset2, 0.1f, 6, seed, 0.5f) - 0.05f);
        continent_noise = max(continent_noise, 0.0f);

        return (ridged_noise + detail * 0.1f * pow(ridged_noise, 0.25f)) * clamp(mask * 2.5f, 0.0f, 1.0f);// + continent_noise;
    };

    uint32_t type = 0;
    
    struct divergent_boundary {
        uint32_t edge;
    };

    std::vector<divergent_boundary> boundaries;


    float sfo = 0.0f;

    auto sample_spreading = [&](uint32_t id, vec3 pos) {
        tectonic_plate& plate = plates[id];

        float seafloor_output = 0.0f;
        float spread_factor = 7.5f;

        float numerator = 0.0f;
        float denominator = 0.0f;
        float blend_value = 25.0f;
        float min_dist = FLT_MAX;
        for(auto boundary : boundaries) {
            // get dist
            Connection& connection = connections[boundary.edge];

            voronoi_cell& ca = voronoi[connection.a];
            voronoi_cell& cb = voronoi[connection.b];

            vec3 dir = circumcenters[connection.c] - circumcenters[connection.d];
            vec3 norm_dir = normalize(dir);
            vec3 line_origin = circumcenters[connection.d];
            float dist = dot(pos - line_origin, norm_dir);
            dist = clamp(dist, 0.0f, length(dir));
            vec3 closest_point = line_origin + dist * norm_dir;
            closest_point = normalize(closest_point);
            dist = length(closest_point - pos);

            // compute blend;

            float w = exp(-dist * unit * blend_value);
            
            tectonic_plate& ta = plates[ca.plate];
            tectonic_plate& tb = plates[cb.plate];

            vec3 delta_a;
            vec3 delta_b;
            vec3 normal;

            if(ca.plate == id) {
                normal = ca.pos - cb.pos;
                delta_a = cross(ca.pos, ta.movement_dir);
                delta_b = cross(cb.pos, tb.movement_dir);
            } else {
                normal = cb.pos - ca.pos;
                delta_a = cross(cb.pos, tb.movement_dir);
                delta_b = cross(ca.pos, ta.movement_dir);
            }
            normal = normalize(normal);
            vec3 relative_delta = delta_b - delta_a;
            
            float pdelta = dot(normal, relative_delta);
            //if(abs(pdelta) < 2.0f) pdelta = pow(abs(pdelta) / 2.0f, 3.0f) * 2.0f * abs(pdelta);

            float width = pdelta / spread_factor;
            //if(width < 0.0f) w *= 0.5f;

            denominator += w;
            numerator += width * w;

            // distance
            min_dist = min(min_dist, dist);
        }

        float thickness = Noise_gen::perlin_noise(pos, 0.5f, 2, seed);

        float width = numerator / denominator;
        float blend_width = (0.075f + 0.05f * thickness) * unit * 0.5f;

        seafloor_output = map(min_dist - width, (avg_sep2 * 0.5f) * clamp(width * 3.0f, 0.0f, 1.0f), -blend_width, 0.0f, 1.0f);
        //seafloor_output = map(min_dist - width, avg_sep2 * 0.5f, -blend_width, 0.0f, 1.0f);
        seafloor_output = clamp(seafloor_output, 0.0f, 1.0f);

        //return 0.0f;
        sfo = width * spread_factor;
        return width;
    };
    
    auto sample_base = [&](uint32_t plate, vec3 pos) {
        tectonic_plate& p = plates[plate];
        vec3 offset = hash_vec3(plate, 1);
        vec3 offset2 = hash_vec3(plate, 2);

        float min_dist = FLT_MAX;
        for(auto [id, connection] : connections) {
            vec3 dir = circumcenters[connection.c] - circumcenters[connection.d];
            vec3 norm_dir = normalize(dir);
            vec3 line_origin = circumcenters[connection.d];
            float dist = dot(pos - line_origin, norm_dir);
            dist = clamp(dist, 0.0f, length(dir));
            vec3 closest_point = line_origin + dist * norm_dir;
            closest_point = normalize(closest_point);
            dist = length(closest_point - pos);

            min_dist = min(dist, min_dist);
        }

        if(p.elev > 0.0f) {
            float elev = continental_elevation;
            float secondary = secondary_details(pos, plate);

            float dist_from = Noise_gen::perlin_noise(pos + hash_vec3(plate ^ 0x88F01, 89), 1.0f, 3, seed, 0.6f) * 8.0f - 0.0125f;
            dist_from *= avg_sep;
            dist_from = max(0.0f, dist_from);
            
            float spreading = sample_spreading(plate, pos);

            dist_from += spreading;

            //min_dist -= spreading;

            float thickness = avg_sep * 0.75f;
            float f = map(min_dist, dist_from - thickness, dist_from + thickness, 0.0f, 1.0f);
            
            float noise = Noise_gen::perlin_noise(pos + hash_vec3(plate ^ 0x10110F, 89), 1.0f, 5, seed, 0.575f);
            
            float v = mix(oceanic_elevation, continental_elevation, clamp(f, 0.0f, 1.0f));

            return vec2(v, oceanic_elevation);
        } else {
            float variation = 0.75f;
            float noise = Noise_gen::perlin_noise(pos + offset, 0.25 * unit, 4, seed, 0.575);
            return vec2(oceanic_elevation, 0.0f);
        }
    };

    // grow plates
    
    std::set<uint32_t> active_plates;
    uint32_t i = 0;
    for(tectonic_plate& plate : plates) {
        uint32_t index = random.next() % available_cells.size();

        auto iter = available_cells.begin();
        std::advance(iter, index);
        uint32_t cell = *iter;

        for(uint32_t cell : plate.cells) {
            for(auto n : neighbors[cell]) {
                auto new_cell = voronoi[n];
                if(new_cell.plate == 0xFFFFFFFF) plate.frontier.emplace(n);
            }
        }

        if(plate.frontier.size()) active_plates.emplace(i);
        ++i;
    }

    while(active_plates.size()) {
        std::vector<uint32_t> remove;
        for(uint32_t plate_id : active_plates) {
            tectonic_plate& plate = plates[plate_id];
            while(true) {
                uint32_t index = random.next() % plate.frontier.size();

                auto iter = plate.frontier.begin();
                std::advance(iter, index);
                uint32_t cell_id = *iter;

                auto& cell = voronoi[cell_id];

                if(cell.plate == 0xFFFFFFFF) {
                    if(abs(random()) < plate.weight) {
                        cell.plate = plate_id;
                        plate.frontier.erase(cell_id);

                        for(uint32_t neighbor_id : neighbors[cell_id]) {
                            auto& new_cell = voronoi[neighbor_id];
                            if(new_cell.plate == 0xFFFFFFFF) plate.frontier.emplace(neighbor_id);
                        }
                        
                        if(plate.frontier.size() == 0) remove.push_back(plate_id);
                    }
                    break;
                } else {
                    plate.frontier.erase(cell_id);
                }

                if(plate.frontier.size() == 0) {
                    remove.push_back(plate_id);
                    break;
                }
            }
        }

        for(auto plate : remove) {
            active_plates.erase(plate);
        }
    }

    std::cout << "tri: " << tri.size() << " " << connections.size() << "\n";

    std::array<float, 4> boundary_weights;

    vec3 diff;
    uint32_t t0;
    uint32_t t1;
    vec3 p0;
    vec3 p1;
    
    auto sample_edges = [&](vec3 pos, std::set<uint32_t> plate_set) {
        boundaries.clear();

        float min_dist = FLT_MAX;

        float numerator = 0.0f;
        float denominator = 0.0f;
        float blend_value = unit / 35.0f;
        float dist_from_divergent = FLT_MAX;
        float magnitude = 0.0f;

        std::fill(boundary_weights.begin(), boundary_weights.end(), 0.0f);

        for(auto [id, connection] : connections) {
            vec3 rel_pos = pos - circumcenters[connection.c];
            float segment_length = length(circumcenters[connection.d] - circumcenters[connection.c]);
            vec3 norm = (circumcenters[connection.d] - circumcenters[connection.c]) / segment_length;


            float d = dot(rel_pos, norm);
            float prev_d = d;
            d = clamp(d, 0.0f, segment_length);
            vec3 closest_point = circumcenters[connection.c] + norm * d;
            closest_point = normalize(closest_point);
            float dist = length(closest_point - pos);

            // get closest plate
            voronoi_cell& va = voronoi[connection.a];
            voronoi_cell& vb = voronoi[connection.b];
            tectonic_plate& ta = plates[va.plate];
            tectonic_plate& tb = plates[vb.plate];
            uint higher = 0;
            if(tb.elev > ta.elev) higher = 1;
            uint closer = 0;
            if(length(va.pos - pos) > length(vb.pos - pos)) closer = 1;

            float w = exp(-dist / blend_value);
            denominator += w;
            if(dist < min_dist) {
                min_dist = dist;
                diff = closest_point - pos;

                p0 = circumcenters[connection.c];
                p1 = circumcenters[connection.d];

                if(closer == 0) {
                    t0 = va.plate;
                    t1 = vb.plate;
                } else {
                    t0 = vb.plate;
                    t1 = va.plate;
                }
            }

            // collision types
            
            vec3 delta_a = cross(va.pos, ta.movement_dir);
            vec3 delta_b = cross(vb.pos, tb.movement_dir);
            vec3 normal = normalize(vb.pos - va.pos);

            float convergence = dot(normal, delta_b - delta_a);
            
            numerator += w * convergence;
            magnitude += w * (length(delta_b) + length(delta_a)) * 0.5f;
            
            float falloff = unit * 0.01f;
            if(higher == closer) boundary_weights[3] += w;
            else boundary_weights[3] += w * (1.0f - clamp(dist / falloff, 0.0f, 1.0f));

            float t = 0.0f;
            float th = 0.5f;

            //float weight_a = ta.stored_elev;
            //float weight_b = tb.stored_elev;
            float weight_a = connection.weight_a;
            float weight_b = connection.weight_b;
            float f = clamp((abs(weight_a) + abs(weight_b)) * 0.5f / th, 0.0f, 1.0f);

            if(weight_a > t && weight_b > t) {
                boundary_weights[0] += w * f;
            } else if(weight_a > t != weight_b > t) { 
                boundary_weights[1] += w * f;
            } else if(weight_a < t && weight_b < t) {
                boundary_weights[2] += w * f;
            }
            
            if(plate_set.contains(va.plate) || plate_set.contains(vb.plate)) {
                divergent_boundary db;
                db.edge = id;
                boundaries.push_back(db);
            } 
        }

        numerator /= denominator;
        magnitude /= denominator;

        boundary_weights[0] /= denominator;
        boundary_weights[1] /= denominator;
        boundary_weights[2] /= denominator;
        boundary_weights[3] /= denominator;

        return vec3(min_dist, numerator, magnitude);
    };

    auto blend_width = [](float x, float offset, float v, float v2 = -1.0f) {
        if(v2 < 0.0f) v2 = v;

        float a = x - offset;
        if(a < 0.0) {
            return clamp((abs(a) - v) / (-v), 0.0f, 1.0f);
        } else {
            return clamp((abs(a) - v2) / (-v2), 0.0f, 1.0f);
        }
    };

    std::unordered_map<uint32_t, Connection> new_connections;
    for(auto [id, connection] : connections) {
        uint32_t a = connection.a;
        uint32_t b = connection.b;
        voronoi_cell& ca = voronoi[a];
        voronoi_cell& cb = voronoi[b];

        if(ca.plate != cb.plate) {
            tectonic_plate& ta = plates[ca.plate];
            tectonic_plate& tb = plates[cb.plate];

            vec3 delta_a = cross(ca.pos, ta.movement_dir);
            vec3 delta_b = cross(cb.pos, tb.movement_dir);
            vec3 direction = normalize(ca.pos - cb.pos);
            vec3 rel_movement = delta_a - delta_b;

            float convergence = dot(direction, rel_movement);

            connection.convergence = convergence;
            connection.vel_a = delta_a;
            connection.vel_b = delta_b;

            new_connections.emplace(id, connection);

            divergent_boundary db;
            db.edge = id;
            boundaries.push_back(db);
        } else {
            interior_connections.emplace(id, connection);
        }
    }
    connections = new_connections;

    // get weights
    for(auto& [id, connection] : connections) {
        uint32_t a = connection.a;
        uint32_t b = connection.b;
        voronoi_cell& ca = voronoi[a];
        voronoi_cell& cb = voronoi[b];
        
        tectonic_plate& ta = plates[ca.plate];
        tectonic_plate& tb = plates[cb.plate];
        
        vec3 central_pos = normalize((circumcenters[connection.c] + circumcenters[connection.d]) * 0.5f);
        
        vec2 elev_a = sample_base(ca.plate, central_pos);
        vec2 elev_b = sample_base(cb.plate, central_pos);
        
        connection.weight_a = elev_a.x;
        connection.weight_b = elev_b.x;
    }

    auto sample_elev = [&](vec3 pos, vec3 dx, vec3 dy) {
        uint32_t cell;
        float dist = FLT_MAX;
        for(int i = 0; i < voronoi.size(); ++i) {
            voronoi_cell& v = voronoi[i];
            float dd = length(pos - v.pos);

            if(dd < dist) {
                dist = dd;
                cell = i;
            }
        }
        
        // plate distances

        std::vector<uint32_t> plate_ids;
        std::vector<uint32_t> plate_cells;
        std::vector<float> plate_dists;
        int num = 20;

        auto insert_dist = [&](uint32_t plate, uint32_t cell, float dist) {
            bool plate_found = false;

            std::set<uint32_t> inserted_plates;

            int src_plate = plate;
            int src_cell = cell;
            float src_dist = dist;

            for(int j = 0; j < min(num, int(plate_ids.size())); ++j) {
                int dst_plate = plate_ids[j];
                int dst_cell = plate_cells[j];
                float dst_dist = plate_dists[j];

                if(dst_plate != src_plate && inserted_plates.contains(dst_plate)) {
                    plate_ids.erase(plate_ids.begin() + j);
                    plate_dists.erase(plate_dists.begin() + j);
                    plate_cells.erase(plate_cells.begin() + j);
                    --j;
                } else {
                    if(src_dist < dst_dist) {
                        if(dst_plate == src_plate) {
                            plate_dists[j] = src_dist;
                            plate_cells[j] = src_cell;
                            plate_found = true;
                            break;
                        } else {
                            inserted_plates.emplace(src_plate);
                            plate_ids[j] = src_plate;
                            plate_dists[j] = src_dist;
                            plate_cells[j] = src_cell;
                            
                            src_plate = dst_plate;
                            src_dist = dst_dist;
                            src_cell = dst_cell;
                        }
                    } else {
                        if(dst_plate == src_plate) {
                            plate_found = true;
                            break;
                        }
                    }
                }
            }

            if(!plate_found && plate_ids.size() < num) {
                plate_ids.push_back(src_plate);
                plate_cells.push_back(src_cell);
                plate_dists.push_back(src_dist);
            }
        };

        // compute closest points
        for(auto [id, connection] : connections) {
            vec3 rel_pos = pos - circumcenters[connection.c];
            float segment_length = length(circumcenters[connection.d] - circumcenters[connection.c]);
            vec3 norm = (circumcenters[connection.d] - circumcenters[connection.c]) / segment_length;

            float d = dot(rel_pos, norm);
            float prev_d = d;
            d = clamp(d, 0.0f, segment_length);
            vec3 closest_point = circumcenters[connection.c] + norm * d;
            closest_point = normalize(closest_point);
            float dist = length(closest_point - pos);

            uint32_t plate_a = voronoi[connection.a].plate;
            uint32_t plate_b = voronoi[connection.b].plate;
            
            insert_dist(plate_a, connection.a, dist);
            insert_dist(plate_b, connection.b, dist);
        }
        // insert plate that point is inside
        insert_dist(voronoi[cell].plate, cell, -plate_dists[0]);



        for(auto& plate : plates) {
            plate.temp_elev = plate.elev;
        }

        // terrain

        float elevation = 0.0f;
        float sum = 0.0f;
        float blend_factor = unit / 12.5f;
        float c = 0.5f;
        float threshold = 0.0f;

        //std::array<float, 4> boundary_weights;
        // 0 = continental x continental
        // 1 = continental x oceanic
        // 2 = oceanic x oceanic

        // distance to plate boundary

        std::set<uint32_t> ids = {plate_ids[0], plate_ids[1], plate_ids[2], plate_ids[3], plate_ids[4], plate_ids[5]};

        // continent addition

        bool close = false;

        color2 = vec3(0.0f);

        float spread = 0.0f;

        for(tectonic_plate& plate : plates) {
            plate.stored_elev = 0.0f;
        }

        for(int i = 0; i < 6; ++i) { // 4
            uint32_t id = plate_ids[i];
            tectonic_plate& plate = plates[id];
            float dist = plate_dists[i];
            
            vec3 offset = vec3(hash_float(id * 3), hash_float(id * 3 + 1), hash_float(id * 3 + 2));

            vec2 plate_elevation = sample_base(id, pos);
            float w = exp(-dist / blend_factor);
            
            // y = min_dist (distance to line)
            // z = width (thickness of line)

            plate.stored_elev = plate_elevation.x;

            if(plate.elev > 0.0f) {
                //plate_elevation.x = mix(plate_elevation.x, -0.5f, seafloor_output.x);
            }

            plate.temp_elev = plate_elevation.x;

            elevation += plate_elevation.x * w;
            sum += w;
            c += w * plate_elevation.x;

            spread += sfo * w;
        }
        
        vec3 edges = sample_edges(pos, ids);
        
        elevation /= sum;
        c /= sum;
        spread /= sum;

        // mountains and valleys

        float mountain_factor = 0.0f;

        float new_y = max(0.0f, abs(spread) - threshold) * -sign(spread);

        // convergent
        float convergent_continental = blend_width(edges.x, 0.0f, unit * 0.1f) * 4.0f;
        float divergent_continental = blend_width(edges.x, 0.0f, unit * 0.05f) * -1.0f;

        float convergent_mixed = blend_width(edges.x, 0.0f, unit * 0.075f) * -1.0f;
        float convergent_mixed2 = blend_width(edges.x, unit * 0.1f, unit * 0.075f) * 2.0f;
        float divergent_mixed = blend_width(edges.x, 0.0f, unit * 0.05f) * 0.5f;

        float convergent_oceanic = blend_width(edges.x, 0.0f, unit * 0.075f) * -0.35f;
        float divergent_oceanic = blend_width(edges.x, 0.0f, unit * 0.075f) * 0.4f;
        float convergent_oceanic2 = blend_width(edges.x, unit * 0.1f, unit * 0.005f, unit * 0.05f);
        convergent_oceanic2 = max(0.0f, (0.625f + Noise_gen::perlin_noise(pos + vec3(1, 3, -8), unit * 0.025f, 3, seed) * 0.25f) * convergent_oceanic2);

        float divergent_factor = clamp(-new_y, 0.0f, 1.0f);
        float convergent_factor = clamp(new_y, 0.0f, 1.0f);
        float continental = divergent_continental * divergent_factor + convergent_continental * convergent_factor;
        float mixed = divergent_mixed * divergent_factor + convergent_mixed * convergent_factor;
        float oceanic = divergent_oceanic * divergent_factor + convergent_oceanic * convergent_factor;
        float mixed2 = 0.0f;
        if(plates[plate_ids[0]].temp_elev > 0.0) mixed2 = convergent_mixed2 * convergent_factor;

        float transform_range = 2.0f;
        float magnitude_range = 1.0f;
        float magnitude_offset = 1.0f;
        float rr_range = 0.5f; // restraining and releasing
        float transform_factor = clamp(1.0f - abs(new_y) / transform_range, 0.0f, 1.0f) * max((edges.z - magnitude_offset) / magnitude_range, 0.0f);
        float restrain_release = clamp(new_y / rr_range, -1.0f, 1.0f);
        restrain_release = pow(abs(restrain_release), 0.4f) * sign(restrain_release);

        float transform_region = blend_width(edges.x, unit * 0.1f, unit * 0.1f);
        float transform_oceanic = pow(clamp(restrain_release, 0.0f, 1.0f), 0.5f) * transform_region;
        float transform_noise = 1.0f + Noise_gen::perlin_noise(pos + vec3(9, -6, 1), unit * 0.1f, 3, seed);

        float boundary_add = continental * boundary_weights[0]
        + (mixed + mixed2) * boundary_weights[1]
        + oceanic * boundary_weights[2]
        + convergent_oceanic2 * convergent_factor * clamp(boundary_weights[2] * boundary_weights[3], 0.0f, 1.0f)
        + transform_oceanic * transform_factor * boundary_weights[3] * transform_noise;
        
        //float mountain_factor = clamp((edges.x - gradient) / (-gradient), 0.0f, 1.0f);

        vec3 continental_color = vec4(0.0f);//mix(vec3(0.25f, 0.25f, 1.0f), vec3(1.0f, 0.25f, 0.25f), clamp((elevation / 0.2f) * 0.5f + 0.5f, 0.0f, 1.0f));
        //f(close) continental_color = vec3(0.01f);

        float line_width = 1.0f;

        vec3 normal = normalize(cross(p0, p1));

        float d0 = dot(diff, normal);
        float ddx = dot(dx, normal);
        float ddy = dot(dy, normal);

        float f = 0.51f * line_width;
        float epsilon = 0.0f;

        float dBL = d0 - ddx * f - ddy * f;
        float dBR = d0 + ddx * f - ddy * f;
        float dTL = d0 - ddx * f + ddy * f;
        float dTR = d0 + ddx * f + ddy * f;

        int count = (dBR <= epsilon) + (dBL <= epsilon) + (dTR <= epsilon) + (dTL <= epsilon);
        bool is_cross = count > 0 && count < 3;

        if(dot(p0 - pos, p1 - pos) > 0.001f) is_cross = false;
        
        //float minv = min(min(dBL, dBR), min(dTL, dTR));
        //float maxv = max(max(dBL, dBR), max(dTL, dTR));
        //bool is_cross = maxv > 0.0f && minv <= 0.0f;

        if(is_cross) {
            vec3 convergent_color = vec3(1.0f, 0.25f, 1.0f);
            vec3 transform_color = vec3(1.0f, 1.0f, 0.25f);
            vec3 divergent_color = vec3(0.25f, 1.0f, 1.0f);

            float threshold = 0.5f;
            float blend_value = 1.0f;

            if(edges.y > threshold) continental_color = mix(transform_color, convergent_color, clamp((edges.y - threshold) / blend_value, 0.0f, 1.0f));
            else if(edges.y < -threshold) continental_color = mix(transform_color, divergent_color, clamp((-edges.y - threshold) / blend_value, 0.0f, 1.0f));
            else continental_color = transform_color;

            //color2 = continental_color;
            //color2 = vec3(0.001f);
        }

        return elevation + boundary_add;
    };

    // image generator
        
    int image_size = 512;
    std::vector<uint8_t> pixels(image_size * (image_size / 2) * 4);

    auto get_pos = [&](vec2 input) {
        float angle_y = (input.y - 0.5f) * M_PI;
        float angle_x = input.x * 2.0f * M_PI;

        float cos_y = cos(angle_y);
        vec3 pos = vec3(cos(angle_x) * cos_y, sin(angle_x) * cos_y, sin(angle_y));
        
        uint octaves = 3;
        float frequency = avg_sep * 0.375f;
        float amplitude = avg_sep * (0.175f + 0.075f * Noise_gen::perlin_noise(pos + vec3(7, 0.6, 1.5), avg_sep, 2, seed));
        pos += vec3(Noise_gen::perlin_noise(pos + vec3(5, -6, 3), frequency, octaves, seed), Noise_gen::perlin_noise(pos + vec3(-4, 5, -1), frequency, octaves, seed), Noise_gen::perlin_noise(pos + vec3(5, 4, 1), frequency, octaves, seed)) * amplitude;
        pos = normalize(pos);

        return pos;
    };

    struct color_range {
        vec3 color;
        float elev;
    };

    std::vector<color_range> range;

    for(int y = 0; y < image_size / 2; ++y) {
        for(int x = 0; x < image_size; ++x) {
            int i = y * image_size + x;

            vec2 project = map_project(ivec2(x, y), ivec2(image_size, image_size / 2));
            vec2 project_x = map_project(ivec2(x + 1, y), ivec2(image_size, image_size / 2));
            vec2 project_y = map_project(ivec2(x, y + 1), ivec2(image_size, image_size / 2));

            vec3 pos = get_pos(project);
            vec3 posx = get_pos(project_x);
            vec3 posy = get_pos(project_y);
            
            vec3 dx = posx - pos;
            vec3 dy = posy - pos;
            
            vec3 color = vec3(0.0f);
            if(project.x >= 0.0f && project.x < 1.0f && project.y >= 0.0f && project.y < 1.0f) {
                float elevation = sample_elev(pos, dx, dy);

                float sea_level = 0.0f;
                elevation -= sea_level;                

                if(elevation > 2.0) color = mix(vec3(203, 183, 93) / 255.0f, vec3(1.0f), clamp((elevation - 2.0f) * 0.5f, 0.0f, 1.0f));
                else if(elevation > 0.0) color = mix(vec3(38, 144, 61) / 255.0f, vec3(203, 183, 93) / 255.0f, clamp(elevation * 0.5f, 0.0f, 1.0f));
                else if(elevation > -0.25) color = mix(vec3(29, 66, 137) / 255.0f * 0.75f + 0.25f, vec3(29, 66, 137) / 255.0f, clamp(-elevation / 0.1f, 0.0f, 1.0f));
                else color = mix(vec3(29, 66, 137) / 255.0f, vec3(29, 66, 137) / 255.0f * 0.1f, clamp(-(elevation + 0.25f) * 0.5f, 0.0f, 1.0f));

                if(false) {// shading
                    vec3 base_sun = normalize(vec3(1, 0, 1));

                    mat3 ori = rotate_to(vec3(0, 0, 1), pos);

                    vec3 delta_x = ori * vec3(1, 0, 0);
                    vec3 delta_y = ori * vec3(0, 1, 0);
                    float delta = 0.001;

                    vec3 px = normalize(pos + delta_x * delta);
                    vec3 py = normalize(pos + delta_y * delta);

                    float elev_x = sample_elev(px, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f));
                    float elev_y = sample_elev(py, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f));

                    float scale = 0.002f;

                    vec3 p0 = pos + pos * elevation * scale;
                    px = px + px * elev_x * scale;
                    py = py + py * elev_y * scale;
                    
                    vec3 normal = cross(px - p0, py - p0);
                    normal = normalize(normal);

                    base_sun = ori * base_sun;
                    float factor = dot(normal, base_sun);
                    factor = (factor - 0.5f) * 2.0f + 0.5f;
                    factor = clamp(factor, 0.0f, 1.0f);

                    color *= factor;
                }

                if(color2.x != 0.0f || color2.y != 0.0f || color2.z != 0.0f) {
                    color = color * 0.5f + color2 * 0.5f;
                }
            }

            pixels[i * 4] = color.x * 255;
            pixels[i * 4 + 1] = color.y * 255;
            pixels[i * 4 + 2] = color.z * 255;
            pixels[i * 4 + 3] = 255;

            if(x == (image_size - 1)) std::cout << std::to_string(int(float(y * image_size + x) / (image_size * image_size * 0.5f) * 100.0f)) << "%\n";
        }
    }

    std::cout << "finished\n";

    std::string filename = "output/map" + std::to_string(uint64_t(get_absolute_time() * 10)) + ".png";
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filename.c_str(), image_size, image_size / 2, 4, pixels.data(), 4 * image_size);

    filepath = filename;

    delete data0;
    delete data1;

    /*
    
    std::string path2 = "output\\map17632749113.png";//"output\\map_moon8456270.png";
    ivec3 size2;
    uint8_t* data2 = stbi_load(path2.data(), &size2.x, &size2.y, &size2.z, 0);

    std::vector<uint8_t> output_data(size2.x * size2.y * size2.z);

    float max_circumference = 2.0f * M_PI * 1.0f;

    for(int y = 0; y < size2.y; ++y) {
        for(int x = 0; x < size2.x; ++x) {
            vec2 coords = (vec2(x, y) + 0.5f) / vec2(size2.xy());
            coords = coords * 2.0f - 1.0f;

            float theta = asin(coords.y);

            // lambda
            float longitude = (M_PI * coords.x) / cos(theta);

            // phi
            float latitude = asin((2.0f * theta + sin(2.0f * theta)) / M_PI);

            vec2 pos = vec2(longitude, latitude);
            pos.x = pos.x / M_PI * 0.5f + 0.5f;
            pos.y = pos.y / M_PI + 0.5f;

            vec3 color = vec3(0.0f);
            if(pos.x >= 0.0f && pos.x < 1.0f && pos.y >= 0.0f && pos.y < 1.0f) {
                color = sample_texture(pos, size2, data2);
            }

            uint32_t i = y * size2.x + x;

            output_data[i * 4] = uint8_t(color.x * 255.0f);
            output_data[i * 4 + 1] = uint8_t(color.y * 255.0f);
            output_data[i * 4 + 2] = uint8_t(color.z * 255.0f);
            output_data[i * 4 + 3] = 255;
        }
    }
    
    std::string filename = "output/projection" + std::to_string(uint64_t(get_absolute_time() * 10)) + ".png";
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filename.c_str(), size2.x, size2.y, 4, output_data.data(), 4 * size2.x);
    */
};

/*
float angle = ((y + 0.5f) - (size2.y * 0.5f)) / size2.y * M_PI;
float width = cos(angle);

float new_x = (x - size2.x * 0.5f) / width + size2.x * 0.5f;

vec2 pos = (vec2(new_x, y) + 0.5f) / vec2(size2.xy());

vec3 color = vec3(0.0f);
if(pos.x >= 0.0f && pos.x < 1.0f) {
    color = sample_texture(pos, size2, data2);
}
*/

std::vector<Cell> c2;

std::vector<Cell> simulate_erosion(ivec2 terrain_size) {
    std::vector<Cell> cells;
    float kernel_radius = 1;

    auto sample_kernel = [&](vec2 pos) {
        float dist = length(pos);

        return max(0.0f, 1.0f - (dist / kernel_radius) * (dist / kernel_radius));
    };

    auto get_elev = [&](vec2 pos) {
        //float dist = length(pos - vec2(0.5f)) * 2.0f;
        //float elev = 1.0 - dist;
        float elev = Noise_gen::perlin_noise(vec3(pos, 0.517), 0.75f, 6, 13, 0.35f);
        return elev * 2.25f;
    };

    auto sample_elev = [&](vec2 pos) {
        ivec2 corner = floor(pos - 0.5f);

        ivec2 min_v = ivec2(0, 0);
        ivec2 max_v = terrain_size - 1;

        ivec2 a = corner;
        ivec2 b = corner + ivec2(1, 0);
        ivec2 c = corner + ivec2(0, 1);
        ivec2 d = corner + ivec2(1, 1);
        a = clamp(a, min_v, max_v);
        b = clamp(b, min_v, max_v);
        c = clamp(c, min_v, max_v);
        d = clamp(d, min_v, max_v);

        float ea = cells[a.y * terrain_size.x + a.x].elevation;
        float eb = cells[b.y * terrain_size.x + b.x].elevation;
        float ec = cells[c.y * terrain_size.x + c.x].elevation;
        float ed = cells[d.y * terrain_size.x + d.x].elevation;

        float e = mix(ea, eb, fract(pos.x - 0.5f));
        float f = mix(ec, ed, fract(pos.x - 0.5f));

        float g = mix(e, f, fract(pos.y - 0.5f));

        return g;
    };

    auto change_elev = [&](vec2 pos, float delta) {
        vec4 range = vec4(pos - kernel_radius, pos + kernel_radius);
        ivec4 grid_range = ivec4(floor(range.xy()), ceil(range.zw()));
        
        ivec2 min_v = ivec2(0, 0);
        ivec2 max_v = terrain_size - 1;

        float sum = 0.0f;

        for(int y = grid_range.y; y <= grid_range.w; ++y) {
            for(int x = grid_range.x; x <= grid_range.z; ++x) {
                ivec2 p = {x, y};

                vec2 kernel_pos = vec2(p) + 0.5f - pos;

                float kernel = sample_kernel(kernel_pos);
                sum += kernel;
            }
        }

        for(int y = grid_range.y; y <= grid_range.w; ++y) {
            for(int x = grid_range.x; x <= grid_range.z; ++x) {
                ivec2 p = {x, y};

                if(clamp(p, min_v, max_v) == p) {
                    vec2 kernel_pos = vec2(p) + 0.5f - pos;
                    float norm_kernel = sample_kernel(kernel_pos) / sum;
                    
                    Cell& cell = cells[p.y * terrain_size.x + p.x];
                    cell.elevation += delta * norm_kernel;
                }
            }
        }
    };

    std::vector<uint8_t> pixels(terrain_size.x * terrain_size.y * 4);

    cells.resize(terrain_size.x * terrain_size.y);
    
    for(int y = 0; y < terrain_size.y; ++y) {
        for(int x = 0; x < terrain_size.x; ++x) {
            int i = y * terrain_size.x + x;
            
            vec2 pos = (vec2(x, y) + 0.5f) / vec2(256);

            //vec2 offset = vec2(Noise_gen::perlin_noise(vec3(pos, 2.67f), 0.5f, 3, 1, 0.6), Noise_gen::perlin_noise(vec3(pos, 2.67f), 0.5f, 3, 2, 0.6)) * 0.3f;

            float elevation = get_elev(pos);

            cells[i].elevation = elevation;
        }
    }

    c2 = cells;

    uint32_t iterations = 70000;
    uint32_t max_loops = 1024;

    float sediment_constant = 0.5f;

    for(int i = 0; i < iterations; ++i) {
        vec2 pos = vec2(abs(core.random()) * terrain_size.x, abs(core.random()) * terrain_size.y);

        vec2 velocity = vec2(0.0f);
        float water = 1.0f;
        float sediment = 0.0f;

        float prev_elev = sample_elev(pos);

        for(int j = 0; j < max_loops; ++j) {
            ivec2 ppos = floor(pos + 0.5f);

            vec2 prev_pos = pos;

            if(clamp(ppos, ivec2(0), terrain_size - 1) != ppos) break;

            cells[ppos.y * terrain_size.x + ppos.x].touched = true;

            water *= (1.0f - 0.001f);

            if(water < 0.0f) break;

            bool br = false;

            float elevx = sample_elev(pos + vec2(1.0f, 0.0f));
            float elevy = sample_elev(pos + vec2(0.0f, 1.0f));
            float elevnx = sample_elev(pos + vec2(-1.0f, 0.0f));
            float elevny = sample_elev(pos + vec2(0.0f, -1.0f));

            vec2 gradient = vec2(elevx - elevnx, elevy - elevny) * 0.5f;
            
            velocity -= gradient * 0.5f;

            float speed = length(velocity);
            float min_step = kernel_radius;
            velocity = (speed > min_step) ? velocity / speed * min_step : velocity;
            pos += velocity;

            float prev_elev = sample_elev(prev_pos);
            float new_elev = sample_elev(pos);

            float max_erode = prev_elev - new_elev;
            max_erode = min(abs(max_erode), 0.1f) * sign(max_erode);

            // calculate capacity (higher when more water or moving fast down slope)
            float d = -dot(gradient, velocity);
            float capacity = d * water * 4.0f;
            capacity = max(capacity, 0.0f);
            
            // deposit (if carrying more than capacity or moving up a slope)
            // else erode

            // if going up, new_elev is more than prev_elev, so max erode will be negative
            // if going down, new_elev is less that prev_elev, so max erode will be positive
            // up -> negative, down -> positive

            float fraction = 0.025f;
            if(sediment > capacity) { // deposit
                float deposition_amount = clamp((sediment - capacity) * fraction, 0.0f, max(0.0f, -max_erode));
                change_elev(prev_pos, deposition_amount);
                sediment -= deposition_amount;
            } else { // erode
                float erosion_amount = clamp((capacity - sediment) * fraction, 0.0f, max(0.0f, max_erode));
                change_elev(prev_pos, -erosion_amount);
                sediment += erosion_amount;  
            }
        }
    }
    
    /*
    for(int y = 0; y < image_size; ++y) {
        for(int x = 0; x < image_size; ++x) {
            int i = y * image_size + x;

            float elevation = cells[i].elevation;
            
            vec3 color = mix(vec3(38, 144, 61) / 255.0f, vec3(203, 183, 93) / 255.0f, fract(elevation * 5.0));
            //if(elevation > 2.0) color = mix(vec3(203, 183, 93) / 255.0f, vec3(1.0f), clamp((elevation - 2.0f) * 0.5f, 0.0f, 1.0f));
            //else if(elevation >= 0.0) color = mix(vec3(38, 144, 61) / 255.0f, vec3(203, 183, 93) / 255.0f, clamp(elevation * 0.5f, 0.0f, 1.0f));
            //else color = mix(vec3(29, 66, 137) / 255.0f, vec3(29, 66, 137) / 255.0f * 0.1f, clamp(-elevation * 0.5f, 0.0f, 1.0f));

            //if(cells[i].touched) color = vec3(1.0f, 0.0f, 0.0f);

            pixels[i * 4] = color.x * 255;
            pixels[i * 4 + 1] = color.y * 255;
            pixels[i * 4 + 2] = color.z * 255;
            pixels[i * 4 + 3] = 255;

            if(x == (image_size - 1)) std::cout << std::to_string(int(float(y * image_size + x) / (image_size * image_size) * 100.0f)) << "%\n";
        }
    }
    
    std::string filename = "output/map" + std::to_string(uint64_t(get_absolute_time() * 10)) + ".png";
    stbi_flip_vertically_on_write(true);
    stbi_write_png(filename.c_str(), image_size, image_size, 4, pixels.data(), 4 * image_size);
    */

    return cells;
}

void create_erosion_mesh(std::vector<Cell> cells, uvec2 terrain_size, Transform transform) {
    std::vector<Mesh_vertex> mesh_vertices;
    std::vector<uint32_t> mesh_indices;

    std::unordered_map<uint32_t, vec3> vertex_normals;

    vec3 color = hsv_color(4.2, 0.25, 0.35);

    for(int y = 0; y < terrain_size.y - 1; ++y) {
        for(int x = 0; x < terrain_size.x - 1; ++x) {
            ivec2 corner = ivec2(x, y);

            for(auto i : indices) {
                ivec2 pos = corner + i;

                uint32_t ii = pos.y * terrain_size.x + pos.x;

                uint32_t index = pos.y * terrain_size.x + pos.x;

                float elev = cells[index].elevation;

                Mesh_vertex vertex;
                vertex.position = vec3(pos.x, pos.y, elev * 96.0f);
                vertex.bone_weights = vec4(1.0f, 1.0f, 1.0f, 0.0f);
                vertex.bone_ids.x = ii;
                vertex_normals[ii] = vec3(0.0f);

                mesh_indices.push_back(mesh_vertices.size());
                mesh_vertices.push_back(vertex);
            }
        }
    }

    vec3 mesh_avg = vec3(0.0f);
    for(int triangle = 0; triangle < mesh_indices.size() / 3; ++triangle) {
        Mesh_vertex& v0 = mesh_vertices[triangle * 3];
        Mesh_vertex& v1 = mesh_vertices[triangle * 3 + 1];
        Mesh_vertex& v2 = mesh_vertices[triangle * 3 + 2];

        mesh_avg += v0.position;
        mesh_avg += v1.position;
        mesh_avg += v2.position;

        vec3 normal = normalize(cross(v0.position - v2.position, v1.position - v2.position));
        vec3 n = normal;

        normal = normalize(round(normal / max(abs(normal.x), max(abs(normal.y), abs(normal.z)))));

        vec3 tex_x = cross(normal, vec3(0, 1, 0));
        if(length(tex_x) == 0.0f) tex_x = cross(normal, vec3(0, 0, 1));
        tex_x = normalize(tex_x);
        vec3 tex_y = normalize(cross(normal, tex_x));

        vec3 avg_pos = v0.position + v1.position + v2.position;
        avg_pos /= 3.0f;

        v0.tex_coords = vec2(dot(tex_x, v0.position), dot(tex_y, v0.position));
        v1.tex_coords = vec2(dot(tex_x, v1.position), dot(tex_y, v1.position));
        v2.tex_coords = vec2(dot(tex_x, v2.position), dot(tex_y, v2.position));

        v0.bone_weights = vec4(color, 0.0f);
        v1.bone_weights = vec4(color, 0.0f);
        v2.bone_weights = vec4(color, 0.0f);

        vertex_normals[v0.bone_ids.x] += n;
        vertex_normals[v1.bone_ids.x] += n;
        vertex_normals[v2.bone_ids.x] += n;
    }

    for(auto& [k, v] : vertex_normals) {
        v = normalize(v);
    }

    for(Mesh_vertex& mv : mesh_vertices) {
        mv.normal = vertex_normals[mv.bone_ids.x];
    }

    if(mesh_indices.size()) mesh_avg /= mesh_indices.size();

    Mesh_component mc;
    Transform tc = transform;
    tc.position += tc.orientation * vec3(-vec2(terrain_size) * 0.5f, 0.0f);

    std::shared_ptr<Mesh> mesh(new Mesh);
    mesh->add_vertices(mesh_vertices, mesh_indices);
    mesh->load_buffer();
    mc.mesh = mesh;
    std::shared_ptr<Texture> texture = create_image_texture("res/textures/tilesheet.png", ivec4(48, 128, 16, 16));
    mc.texture = texture;

    uint32_t entity = ecs.insert_entity();

    ecs.insert_component(entity, mc);
    ecs.insert_component(entity, tc);
}