#include "erosion.hpp"
#include "random.hpp"

mat3 rotate_to(vec3 a, vec3 b) {
    vec3 cross_p = cross(a, b);
    float d = dot(a, b);
    float f = acos(d);
    vec3 n = normalize(cross_p);

    if(isinf(n.x) || isnan(n.x) || f == 0 || isnan(f)) {
        mat3 rot_mat = identity<mat3>();
        if(dot(a, b) < 0) rot_mat = mat3(rot_mat[0], -rot_mat[1], -rot_mat[2]);
        return rot_mat;
    }
    mat3 matrix = rotate(f, n);

    return matrix;
}

vec3 lcolor(float f) {
    f = fract(f);
    f *= 6.0f;

    float ff = fract(f);
    if(f < 1.0f) {
        return vec3(1.0f, ff, 0.0f);
    } else if(f < 2.0f) {
        return vec3(1.0f - ff, 1.0f, 0.0f);
    } else if(f < 3.0f) {
        return vec3(0.0f, 1.0f, ff);
    } else if(f < 4.0f) {
        return vec3(0.0f, 1.0f - ff, 1.0f);
    } else if(f < 5.0f) {
        return vec3(ff, 0.0f, 1.0f);
    } else {
        return vec3(1.0f, 0.0f, 1.0f - ff);
    }
}

Erosion_system::Erosion_system() {
    size = ivec2(512, 512);

    tiles.resize(size.x * size.y);

    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            vec2 position = vec2(x, y);

            terrain_tile tile;
            tile.self = {x, y};
            tile.elevation = Noise_gen::perlin_noise(vec3(position, 0.0f), 256, 8, 0xE2, 0.425f);
            tile.water = 1.0f;

            /*
            std::vector<ivec2> neighbors = {
                ivec2(x - 1, y),
                ivec2(x + 1, y),
                ivec2(x, y - 1),
                ivec2(x, y + 1),
            };*/

            std::vector<ivec2> neighbors = {
                ivec2(x - 1, y - 1),
                ivec2(x, y - 1),
                ivec2(x + 1, y - 1),
                ivec2(x - 1, y),
                ivec2(x + 1, y),
                ivec2(x - 1, y + 1),
                ivec2(x, y + 1),
                ivec2(x + 1, y + 1),
            };
            std::vector<float> distances = {
                1.414f,
                1.0f,
                1.414f,
                1.0f,
                1.0f,
                1.414f, 
                1.0f, 
                1.414f
            };
            /*
            std::vector<float> distances = {
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f,
                1.0f, 
                1.0f,
            };
            */

            std::vector<ivec2> new_neighbors;
            std::vector<float> new_distances;
            new_neighbors.reserve(neighbors.size());
            new_distances.reserve(neighbors.size());
            for(int i = 0; i < neighbors.size(); ++i) {
                ivec2 v = neighbors[i];
                if(v.x < 0 || v.x >= size.x || v.y < 0 || v.y >= size.y) continue;
                new_neighbors.push_back(v);
                new_distances.push_back(distances[i]);
            }

            tile.neighbors = new_neighbors;
            tile.distances = new_distances;

            tiles[i] = tile;
        }
    }

    update_texture();
}

void Erosion_system::call() {

}

struct less {
    bool operator()(terrain_tile* a, terrain_tile* b) {
        return a->elevation > b->elevation;
    }
};

void Erosion_system::update_texture() {
    float water_level = -0.05f;
    std::vector<uint8_t> texture(size.x * size.y * 4);

    if(mode == MAP_MODE_ELEVATION) {
        float shadow_constant = 0.0f;//2000.0f;

        for(int y = 0; y < size.y; ++y) {
            for(int x = 0; x < size.x; ++x) {
                ivec2 ii = {x, y};
                int i = x + y * size.x;
                terrain_tile& tt = tiles[i];

                float c = clamp(tiles[i].elevation, 0.0f, 1.0f);

                vec3 color = lcolor(c * 6.0f) * 0.5f + 0.35f;

                if(shadow_constant != 0.0f) {
                    vec3 normal = vec3(0.0f);
                
                    for(ivec2 n : tt.neighbors) {
                        terrain_tile& nt = tiles[n.x + n.y * size.x];
                        vec3 delta = vec3(n, nt.elevation * shadow_constant) - vec3(ii, tt.elevation * shadow_constant);
                        vec3 dd = vec3(n - ii, 0.0f);
                        mat3 rt = rotate_to(normalize(dd), normalize(delta));

                        normal += rt * vec3(0.0f, 0.0f, 1.0f);
                    }

                    normal = normalize(normal);

                    vec3 sun_dir = normalize(vec3(-1.0f, 0.5f, 1.0f));
                    float light = dot(normal, sun_dir);

                    color *= clamp(light, 0.0f, 1.0f) * 0.75f + 0.25f;
                }

                //color = mix(color * 0.5f + 0.5f, vec3(0.0f, 0.0f, 1.0f), clamp(tiles[i].water, 0.0f, 1.0f));

                vec<4, uint8_t> bit_color = {color * 255.0f, 255};

                texture[i * 4] = bit_color[0];
                texture[i * 4 + 1] = bit_color[1];
                texture[i * 4 + 2] = bit_color[2];
                texture[i * 4 + 3] = bit_color[3];
            }
        }

        if(map_texture) {
            map_texture.reset();
        }

        map_texture = std::shared_ptr<Texture>(new Texture(texture.data(), ivec3(size, 1), GL_TEXTURE_2D, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, 0));
    } else if(mode == MAP_MODE_BASIN) {
        std::unordered_map<uint32_t, ivec2> basins_from;
        std::unordered_map<ivec2, uint32_t, Hash_coord> basins_to;
        std::unordered_map<uint32_t, bool> basins_ocean;

        for(int y = 0; y < size.y; ++y) {
            for(int x = 0; x < size.x; ++x) {
                ivec2 vi = {x, y};
                uint32_t i = x + y * size.x;

                terrain_tile& tt = tiles[i];

                tt.water = 0.0f;
                tt.basin = 0xFFFFFFFF;
                tt.upstream.clear();
            }
        }

        auto flood_fill = [&](ivec2 i, float water_elev) {
            std::unordered_set<ivec2, Hash_coord> seen;
            std::priority_queue<terrain_tile*, std::deque<terrain_tile*>, less> open;
            open.push(&tiles[i.x + i.y * size.x]);

            std::unordered_set<ivec2, Hash_coord> lake_vs;
            
            while(true) {
                terrain_tile* current = open.top();
                open.pop();

                if(current->elevation < water_elev) {
                    current->water = water_elev - current->elevation;

                    lake_vs.insert(current->self);
                    for(ivec2 n : current->neighbors) {
                        if(!seen.contains(n)) {
                            open.push(&tiles[n.x + n.y * size.x]);
                            seen.insert(n);
                        }
                    }
                }

                seen.insert(current->self);
            
                if(open.size() == 0) break;
            }

            return lake_vs;
        };

        auto connect = [&](ivec2 root) {
            std::unordered_set<ivec2, Hash_coord> seen;
            std::vector<ivec2> path = {root};
            std::vector<uint32_t> path_i = {0};

            terrain_tile& root_tile = tiles[root.x + root.y * size.x];

            while(true) {
                if(path.size() == 0) break;

                terrain_tile& current = tiles[path.back().x + path.back().y * size.x];
                if(current.upstream.size() <= path_i.back()) {
                    current.basin = root_tile.basin;

                    path.pop_back();
                    path_i.pop_back();
                } else {
                    ivec2 next = current.upstream[path_i.back()];
                    if(seen.contains(next)) {
                        ++path_i[path_i.size() - 1];
                    } else {
                        seen.insert(next);
                        ++path_i[path_i.size() - 1];

                        path.push_back(next);
                        path_i.push_back(0);
                    }
                }
            }   
        };

        for(int y = 0; y < size.y; ++y) {
            for(int x = 0; x < size.x; ++x) {
                ivec2 vi = {x, y};
                uint32_t i = x + y * size.x;

                terrain_tile& tt = tiles[i];

                float min_delta = FLT_MAX;
                float min_value = FLT_MAX;
                int j = 0;
                for(ivec2 n : tt.neighbors) {
                    float s = tt.distances[j];

                    terrain_tile& nt = tiles[n.x + n.y * size.x];
                    float delta = (nt.elevation - tt.elevation) / s;

                    if(delta < min_delta) {
                        min_delta = delta;
                        tt.downstream = n;

                        min_value = nt.elevation;
                    }

                    ++j;
                }

                if(tt.elevation <= water_level) {
                    tt.basin = 0xFFFFFFFF;
                } else if(min_value <= water_level || min_value > tt.elevation) {
                    tt.basin = basins_from.size();
                    basins_from.emplace(basins_from.size(), vi);
                    basins_to.emplace(vi, basins_to.size());

                    if(min_value <= water_level) basins_ocean.emplace(tt.basin, true);
                    else basins_ocean.emplace(tt.basin, false);
                } else {
                    terrain_tile& nt = tiles[tt.downstream.x + tt.downstream.y * size.x];
                    nt.upstream.push_back(vi);
                }
            }
        }

        uint32_t b = 0;
        std::vector<uint32_t> erase;

        for(auto [v, basin] : basins_to) {
            std::vector<ivec2> path = {v};
            std::vector<uint32_t> path_i = {0};
            std::vector<float> flow = {0.0f};

            while(true) {
                if(path.size() == 0) break;

                terrain_tile& current = tiles[path.back().x + path.back().y * size.x];
                if(current.upstream.size() <= path_i.back()) {
                    current.basin = basin;
                    float f = flow.back();
                    current.flow = f;

                    path.pop_back();
                    path_i.pop_back();
                    flow.pop_back();

                    if(flow.size() != 0) flow[flow.size() - 1] += f;
                } else {
                    ivec2 next = current.upstream[path_i.back()];
                    ++path_i[path_i.size() - 1];

                    path.push_back(next);
                    path_i.push_back(0);
                    flow.push_back(1.0f);
                }
            }  
        }

        std::cout << "a";

        for(auto [v, basin] : basins_to) {
            std::unordered_set<ivec2, Hash_coord> seen;
            std::vector<ivec2> path = {v};
            std::vector<uint32_t> path_i = {0};
            std::vector<float> flow = {0.0f};

            while(true) {
                if(path.size() == 0) break;

                terrain_tile& current = tiles[path.back().x + path.back().y * size.x];
                if(current.upstream.size() <= path_i.back()) {
                    float f = flow.back();
                    current.flow = f;

                    path.pop_back();
                    path_i.pop_back();
                    flow.pop_back();

                    if(flow.size() != 0) flow[flow.size() - 1] += f;
                } else {
                    ivec2 next = current.upstream[path_i.back()];

                    if(seen.contains(next)) {
                        ++path_i[path_i.size() - 1];
                    } else {
                        seen.insert(next);
                        ++path_i[path_i.size() - 1];

                        path.push_back(next);
                        path_i.push_back(0);
                        flow.push_back(1.0f);   
                    }
                }
            }  

            terrain_tile& vt = tiles[v.x + v.y * size.x];
            
            bool create_lake = false;
            if(basins_ocean.contains(basin) && basins_ocean[basin] == false) create_lake = true;

            if(create_lake && vt.water == 0.0f) {
                ++b;

                float max_elev = vt.elevation;
                float prev_elev = vt.elevation;

                std::unordered_set<ivec2, Hash_coord> seen;
                std::priority_queue<terrain_tile*, std::deque<terrain_tile*>, less> open;
                open.push(&vt);
                seen.insert(v);

                terrain_tile* current;
                
                while(true) {
                    current = open.top();
                    open.pop();

                    if(current->basin != basin) {
                        if(current->elevation >= max_elev) max_elev = current->elevation;
                        break;
                    };

                    if(current->elevation >= max_elev) {
                        max_elev = current->elevation;
                    }

                    for(ivec2 n : current->neighbors) {
                        if(!seen.contains(n)) {
                            open.push(&tiles[n.x + n.y * size.x]);
                            seen.insert(n);
                        }
                    }

                    if(open.size() == 0) break;
                }  
                
                auto lake_vs = flood_fill(v, max_elev);
                bool d = false;

                for(ivec2 n : current->neighbors) {
                    terrain_tile& t = tiles[n.x + n.y * size.x];

                    if(t.elevation == max_elev) {
                        current->upstream.push_back(n);
                        t.upstream.insert(t.upstream.end(), lake_vs.begin(), lake_vs.end());
                        d = true;
                        break;
                    }
                }

                if(!d) current->upstream.insert(current->upstream.end(), lake_vs.begin(), lake_vs.end());

                if(lake_vs.size() == 0) {
                    current->upstream.push_back(v);
                }

                connect(current->self);

                erase.push_back(basin);
            }

            /*
            path = {v};
            path_i = {0};

            while(true) {
                if(path.size() == 0) break;
                //std::cout << path.size() << "\n";

                terrain_tile& current = tiles[path.back().x + path.back().y * size.x];
                if(current.upstream.size() <= path_i.back()) {
                    if(create_lake && current.elevation <= min_elev) {
                        current.water = current.elevation - min_elev;
                    }

                    path.pop_back();
                    path_i.pop_back();
                } else {
                    ivec2 next = current.upstream[path_i.back()];
                    ++path_i[path_i.size() - 1];

                    path.push_back(next);
                    path_i.push_back(0);
                }
            }   
            */
        }

        
        std::cout << "b";

        for(uint32_t basin : erase) {
            ivec2 r = basins_from[basin];
            basins_to.erase(r);
            basins_from.erase(basin);
        }

        
        for(auto [v, basin] : basins_to) {
            std::unordered_set<ivec2, Hash_coord> seen;
            std::vector<ivec2> path = {v};
            std::vector<uint32_t> path_i = {0};
            std::vector<float> flow = {0.0f};

            while(true) {
                if(path.size() == 0) break;

                terrain_tile& current = tiles[path.back().x + path.back().y * size.x];
                if(current.upstream.size() <= path_i.back()) {
                    current.basin = basin;
                    float f = flow.back();
                    current.flow = f;

                    path.pop_back();
                    path_i.pop_back();
                    flow.pop_back();

                    if(flow.size() != 0) flow[flow.size() - 1] += f;
                } else {
                    ivec2 next = current.upstream[path_i.back()];
                    if(seen.contains(next)) {
                        ++path_i[path_i.size() - 1];
                    } else {
                        seen.insert(next);
                        ++path_i[path_i.size() - 1];

                        path.push_back(next);
                        path_i.push_back(0);
                        flow.push_back(1.0f);
                    }
                }
            }   
        }

        for(int y = 0; y < size.y; ++y) {
            for(int x = 0; x < size.x; ++x) {
                ivec2 ii = {x, y};
                int i = x + y * size.x;
                terrain_tile& tt = tiles[i];

                float c = clamp(tiles[i].elevation, 0.0f, 1.0f);

                vec3 color = lcolor(float(hash(uint64_t(tt.basin)) & 0xFFFFFFFF) / 0xFFFFFFFF);// * 0.125f + vec3(1.0f, 1.0f, 0.5f) * 0.75f;
                if(basins_ocean.contains(tt.basin) && basins_ocean[tt.basin]) color = color * 0.125f + vec3(1.0f, 0.0f, 0.0f) * 0.875f;
                else color = color * 0.125f + vec3(1.0f, 1.0f, 0.0f) * 0.875f;
                if(tt.basin == 0xFFFFFFFF) color = vec3(0.65f, 0.65f, 1.0f);
                if(tt.water != 0.0f) color = vec3(0.45f, 0.45f, 1.0f);

                color = mix(color, vec3(0.45f, 0.45f, 1.0f), clamp(tt.flow / 1024.0f, 0.0f, 1.0f));
                if(tt.mark) color = vec3(1.0f);
                //if(tt.flow > 1024.0f) color = vec3(0.0f);

                vec<4, uint8_t> bit_color = {color * 255.0f, 255};

                texture[i * 4] = bit_color[0];
                texture[i * 4 + 1] = bit_color[1];
                texture[i * 4 + 2] = bit_color[2];
                texture[i * 4 + 3] = bit_color[3];
            }
        }

        if(map_texture) {
            map_texture.reset();
        }

        map_texture = std::shared_ptr<Texture>(new Texture(texture.data(), ivec3(size, 1), GL_TEXTURE_2D, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, 0));
    } else if(mode == MAP_MODE_FLOW) {
        for(int y = 0; y < size.y; ++y) {
            for(int x = 0; x < size.x; ++x) {
                ivec2 ii = {x, y};
                int i = x + y * size.x;
                terrain_tile& tt = tiles[i];

                float c = clamp(tiles[i].elevation, 0.0f, 1.0f);

                vec3 color = mix(vec3(1.0f), vec3(0.0f, 0.0f, 1.0f), clamp(tt.flow / 512.0f, 0.0f, 1.0f));

                if(tt.elevation < water_level || tt.water != 0.0f) color = vec3(0.0f, 0.0f, 1.0f);

                vec<4, uint8_t> bit_color = {color * 255.0f, 255};

                texture[i * 4] = bit_color[0];
                texture[i * 4 + 1] = bit_color[1];
                texture[i * 4 + 2] = bit_color[2];
                texture[i * 4 + 3] = bit_color[3];
            }
        }

        if(map_texture) {
            map_texture.reset();
        }

        map_texture = std::shared_ptr<Texture>(new Texture(texture.data(), ivec3(size, 1), GL_TEXTURE_2D, {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, 0));
    }
}

void Erosion_system::sim_step() {

}

/*
void Erosion_system::sim_step() {
    float precipitation_constant = 0.1f;
    float flow_constant = 0.5f;
    float erosion_constant = 5.0f;

    std::vector<float> factors(size.y * size.x);

    std::vector<terrain_tile> new_tiles = tiles;
    for(terrain_tile& t : new_tiles) {
        t.flux_in = 0.0f;
        t.flux_out = 0.0f;
    }

    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            ivec2 vi = {x, y};
            uint32_t i = x + y * size.x;

            terrain_tile& tt = tiles[i];
            terrain_tile& tt2 = new_tiles[i];
            //tt2.water += 0.01;

            for(ivec2 n : tt.neighbors) {
                float len = length(vec2(n - vi));
                terrain_tile& nt = tiles[n.x + n.y * size.x];
                terrain_tile& nt2 = new_tiles[n.x + n.y * size.x];

                float slope = (nt.elevation - tt.elevation) / len * 0.25f;
                if(slope < 0.0f) {
                    tt2.flux_out += -slope * tt.water;
                }
            }
            
            float factor = 1.0f;
            if(tt2.flux_out > tt.water) {
                factor = tt.water / tt2.flux_out;
            }
            factors[i] = factor;
            tt2.flux_out *= factor;

            for(ivec2 n : tt.neighbors) {
                float len = length(vec2(n - vi));
                terrain_tile& nt = tiles[n.x + n.y * size.x];
                terrain_tile& nt2 = new_tiles[n.x + n.y * size.x];

                float slope = (nt.elevation - tt.elevation) / len * 50.0f;
                if(slope < 0.0f) {
                    nt2.flux_in += -slope * tt.water * factor;
                }
            }
        }
    }

    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            ivec2 vi = {x, y};
            uint32_t i = x + y * size.x;

            terrain_tile& tt = tiles[i];
            terrain_tile& tt2 = new_tiles[i];

            tt2.water += tt2.flux_in;
            tt2.water -= tt2.flux_out;

            float factor = factors[i];

            for(ivec2 n : tt.neighbors) {
                float len = length(vec2(n - vi));
                terrain_tile& nt = tiles[n.x + n.y * size.x];
                terrain_tile& nt2 = new_tiles[n.x + n.y * size.x];

                float slope = (nt.elevation - tt.elevation) / len * 50.0f;
                if(slope < 0.0f) {
                    float delta_sediment = -slope * tt.sediment * factor;
                    nt2.sediment += delta_sediment;
                    tt2.sediment -= delta_sediment;
                }
            }
        }
    }

    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            ivec2 vi = {x, y};
            uint32_t i = x + y * size.x;

            terrain_tile& tt = tiles[i];
            terrain_tile& tt2 = new_tiles[i];

            float maximum = 0.0f;
            int j = 0;
            for(ivec2 n : tt.neighbors) {
                float len = tt.distances[j];
                terrain_tile& nt = tiles[n.x + n.y * size.x];
                float slope = nt.elevation - tt.elevation;
                if(slope < 0.0f) {
                    if(maximum = 0.0f) maximum = slope;
                    else maximum = max(maximum, slope);
                }

                ++j;
            }

            float capacity = tt2.flux_out;
            float delta = tt2.sediment - capacity;

            if(delta > 0.0f) { // more sediment than capacity, deposit
                tt2.elevation += delta;
                tt2.sediment -= delta;
            } else { // more capacity than sediment, erode
                float erode = min(-delta, -maximum);
                tt2.elevation -= erode;
                tt2.sediment += erode;
            }
        }
    }

    /*
    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            ivec2 vi = {x, y};
            uint32_t i = x + y * size.x;

            terrain_tile& tt = tiles[i];
            terrain_tile& tt2 = new_tiles[i];
            //tt2.water += 0.01;

            float lowest_slope = 0.0f;
            float total_slope = 0.0f;
            for(ivec2 n : tt.neighbors) {
                float len = length(vec2(n - vi));
                terrain_tile& nt = tiles[n.x + n.y * size.x];

                float slope = (nt.elevation - tt.elevation) / len;
                lowest_slope = min(lowest_slope, slope);
                if(slope < 0.0f) total_slope += slope;
            }

            float capacity = -lowest_slope * tt.water * 0.5f;
            float delta = tt.sediment - capacity;

            if(delta > 0.0f) { // more sediment than capacity, deposit
                tt2.elevation += delta;
                tt2.sediment -= delta;
            } else { // more capacity than sediment, erode
                float erode = min(-lowest_slope, -delta);
                tt2.elevation -= erode;
                tt2.sediment += erode;
            }
            
            float flow_factor = 0.5f * -lowest_slope / (1.0f / size.x);
            flow_factor = min(1.0f, flow_factor);
            
            for(ivec2 n : tt.neighbors) {
                terrain_tile& nt = tiles[n.x + n.y * size.x];

                float slope = nt.elevation - tt.elevation;

                if(slope < 0.0f) {
                    float fraction = slope / total_slope;
                    terrain_tile& nt2 = new_tiles[n.x + n.y * size.x];

                    nt2.water += fraction * tt.water * flow_factor;
                    nt2.sediment += fraction * tt.sediment * flow_factor;
                    
                    tt2.water -= fraction * tt.water * flow_factor;
                    tt2.sediment -= fraction * tt.sediment * flow_factor;
                }
            }
        }
    }
    
    tiles = new_tiles;
    update_texture();
}
*/