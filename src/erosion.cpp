#include "erosion.hpp"
#include "random.hpp"
#include "core.hpp"
#include "input.hpp"

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
    if(f < 0.0f) f = 1.0f - f;
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
    size = ivec2(256, 256);

    tiles.resize(size.x * size.y);

    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            vec2 position = vec2(x, y);

            terrain_tile tile;
            tile.self = {x, y};
            tile.elevation = Noise_gen::perlin_noise(vec3(position, 0.0f), 256, 3, 0x120, 0.5f) + Noise_gen::perlin_noise(vec3(position, 0.0f), 24, 3, 0x16, 0.425f) * 0.125f;

            /*
            std::vector<ivec2> neighbors = {
                ivec2(x - 1, y),
                ivec2(x + 1, y),
                ivec2(x, y - 1),
                ivec2(x, y + 1),
            };*/
            
            /*
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
            */
           
            std::vector<ivec2> neighbors = {
                ivec2(x, y - 1),
                ivec2(x - 1, y),
                ivec2(x + 1, y),
                ivec2(x, y + 1),
            };
            std::vector<float> distances = {
                1.0f,
                1.0f,
                1.0f,
                1.0f, 
            };

            tile.neighbors = neighbors;
            tile.distances = distances;

            tiles[i] = tile;
        }
    }

    update_texture();
}

void Erosion_system::call() {
    if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
        Input_system& input_system = ecs.get_system<Input_system>();
        ivec2 pos = ivec2((floor(input_system.world_cursor_pos) + vec2(size)) / 2.0f);

        if(core.key_map[GLFW_KEY_LEFT_SHIFT]) {
            path.clear();
            if(pos.x >= 0 && pos.x < size.x && pos.y >= 0 && pos.y < size.y) {
                terrain_tile* t = &tiles[pos.x + pos.y * size.x];
                while(true) {
                    path.push_back(t->self);
                    if(t->downstream == ivec2(-1, -1)) break;
                    else {
                        t = &tiles[t->downstream.x + t->downstream.y * size.x];
                    }
                }
            }
        } else if(core.key_map[GLFW_KEY_LEFT_CONTROL]) {
            for(int y = 0; y < size.y; ++y) {
                for(int x = 0; x < size.x; ++x) {
                    tiles[x + y * size.x].mark = false;
                }
            }

            if(pos.x >= 0 && pos.x < size.x && pos.y >= 0 && pos.y < size.y) {
                std::unordered_set<ivec2, Hash_coord> seen;
                std::vector<ivec2> path = {pos};
                std::vector<uint32_t> path_i = {0};
                std::vector<float> flow = {0.0f};

                while(true) {
                    if(path.size() == 0) break;

                    terrain_tile& current = tiles[path.back().x + path.back().y * size.x];
                    if(current.upstream.size() <= path_i.back()) {
                        float f = flow.back();
                        current.flow = f;
                        current.mark = true;

                        path.pop_back();
                        path_i.pop_back();
                        flow.pop_back();

                        if(flow.size() != 0) flow[flow.size() - 1] += f;
                        else std::cout << "FLOW: " << f << "\n";
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

            mode = MAP_MODE_FLOW;
            update_texture();
        }
    }
}

struct less {
    bool operator()(terrain_tile* a, terrain_tile* b) {
        return a->elevation > b->elevation;
    }
};

void Erosion_system::update_texture() {
    auto is_valid = [&](ivec2 v) {
        return v.x >= 0 && v.y >= 0 && v.x < size.x && v.y < size.x;
    };

    float water_level = -0.0f;
    std::vector<uint8_t> texture(size.x * size.y * 4);

    vec3 river_color = vec3(0.5f, 0.5f, 1.0f);
    vec3 ocean_color = vec3(0.35f, 0.35f, 0.85f);

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
                        if(is_valid(n)) {
                            terrain_tile& nt = tiles[n.x + n.y * size.x];
                            vec3 delta = vec3(n, nt.elevation * shadow_constant) - vec3(ii, tt.elevation * shadow_constant);
                            vec3 dd = vec3(n - ii, 0.0f);
                            mat3 rt = rotate_to(normalize(dd), normalize(delta));

                            normal += rt * vec3(0.0f, 0.0f, 1.0f);
                        }
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
                        if(is_valid(n)) {
                            if(!seen.contains(n)) {
                                open.push(&tiles[n.x + n.y * size.x]);
                                seen.insert(n);
                            }
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
                    if(is_valid(n)) {
                        float s = tt.distances[j];

                        terrain_tile& nt = tiles[n.x + n.y * size.x];
                        float delta = (nt.elevation - tt.elevation) / s;

                        if(delta < min_delta) {
                            min_delta = delta;
                            tt.downstream = n;

                            min_value = nt.elevation;
                        }
                    }

                    ++j;
                }

                if(tt.elevation <= water_level) {
                    tt.basin = 0xFFFFFFFF;
                } else if(min_value <= water_level || min_value > tt.elevation) {
                    tt.basin = basins_from.size();
                    basins_from.emplace(basins_from.size(), vi);
                    basins_to.emplace(vi, basins_to.size());

                    tt.downstream = ivec2(-1);

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

        int i = 0;
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

            float volume_limit = vt.flow * 0.0002f;

            if(create_lake && vt.water == 0.0f) {
                ++b;

                float max_elev = vt.elevation;
                float prev_elev = vt.elevation;

                std::unordered_set<ivec2, Hash_coord> seen;
                std::priority_queue<terrain_tile*, std::deque<terrain_tile*>, less> open;
                open.push(&vt);
                seen.insert(v);

                terrain_tile* current;

                uint32_t num_tiles = 0;
                float total_volume = 0.0f;
                bool link = false;
                
                while(true) {
                    current = open.top();
                    open.pop();

                    if(current->basin != basin) {
                        if(current->elevation >= max_elev) max_elev = current->elevation;
                        link = true;
                        break;
                    };

                    if(current->elevation >= max_elev) {
                        prev_elev = max_elev;
                        max_elev = current->elevation;
                        total_volume += float(num_tiles) * (max_elev - prev_elev);
                    }

                    for(ivec2 n : current->neighbors) {
                        if(is_valid(n)) {
                            if(!seen.contains(n)) {
                                open.push(&tiles[n.x + n.y * size.x]);
                                seen.insert(n);
                            }
                        }
                    }

                    ++num_tiles;
                    total_volume += current->elevation - max_elev;
                    if(total_volume > volume_limit) break;

                    if(open.size() == 0) break;
                }  
                
                float elev;
                if(total_volume > volume_limit) elev = prev_elev;
                else elev = max_elev;

                auto lake_vs = flood_fill(v, elev);
                bool d = false;

                if(link) {
                    for(ivec2 n : current->neighbors) {
                        if(is_valid(n)) {
                            terrain_tile& t = tiles[n.x + n.y * size.x];

                            if(t.elevation == max_elev) {
                                t.downstream = current->self;
                                current->upstream.push_back(n);
                                for(ivec2 v : lake_vs) tiles[v.x + v.y * size.x].downstream = t.self;
                                t.upstream.insert(t.upstream.end(), lake_vs.begin(), lake_vs.end());
                                d = true;

                                break;
                            }
                        }
                    }

                    if(!d) {
                        for(ivec2 v : lake_vs) tiles[v.x + v.y * size.x].downstream = current->self;
                        current->upstream.insert(current->upstream.end(), lake_vs.begin(), lake_vs.end());
                    }

                    if(lake_vs.size() == 0) {
                        vt.downstream = current->self;
                        current->upstream.push_back(v);
                    }
                    
                    connect(current->self);
                    
                    erase.push_back(basin);
                }
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
            std::cout << i << " / " << basins_from.size() << "\n";
            ++i;
        }

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

                vec3 color = lcolor(float(hash(uint64_t(tt.basin * 7)) & 0xFFFFFFFF) / 0xFFFFFFFF) * 0.4f + 0.6f;

                if(tt.basin == 0xFFFFFFFF || tt.water != 0.0f) color = ocean_color;
                else {
                    color = mix(color, river_color, clamp(tt.flow / 1024.0f, 0.0f, 1.0f));
                }

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
        vec3 light = normalize(vec3(-0.5f, 1.0f, 1.0f));

        for(int y = 0; y < size.y; ++y) {
            for(int x = 0; x < size.x; ++x) {
                ivec2 ii = {x, y};
                int i = x + y * size.x;
                terrain_tile& tt = tiles[i];

                vec3 normal = vec3(0, 0, 1);
                
                for(ivec2 n : tt.neighbors) {
                    if(is_valid(n)) {
                        terrain_tile& nt = tiles[n.x + n.y * size.x];

                        normal += vec3(n - tt.self, 0.0f) * ((tt.elevation + tt.sediment) - (nt.elevation + nt.sediment)) * 80.0f;
                    }
                }

                float l = dot(light, normalize(normal));

                float c = clamp(tiles[i].elevation, 0.0f, 1.0f);

                vec3 high_color = vec3(145, 60, 28) / 255.0f;
                vec3 low_color = vec3(0.9f);

                vec3 color = low_color;//mix(low_color, high_color, clamp(difference * 25.0f, 0.0f, 1.0f));
                color *= clamp(l, 0.0f, 1.0f) * 0.4f + 0.6f;

                //color = mix(color, river_color, clamp(tt.water / 0.0005f, 0.0f, 1.0f));

                //if(tt.elevation < water_level || tt.water != 0.0f) color = ocean_color;

                if(tt.mark) color = mix(vec3(1.0f, 0.35f, 0.35f), color, 0.5f);

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
    auto is_valid = [&](ivec2 v) {
        return v.x >= 0 && v.y >= 0 && v.x < size.x && v.y < size.x;
    };

    std::vector<float> inverse_index = {3, 2, 1, 0};

    float precipitation = 0.000005f;

    // set tiles and precipitation
    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            
            terrain_tile& t = tiles[i];
            t.water += precipitation;
            t.outflow.resize(4);
            std::fill(t.outflow.begin(), t.outflow.end(), 0.0f);
        }
    }

    // compute outflow
    float outflow_factor = 0.125f;
    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            terrain_tile& t = tiles[i];

            float flow_in = 0.0f;

            std::vector<float> outflows(t.neighbors.size(), 0.0f);

            int j = 0;
            for(ivec2 v : t.neighbors) {
                if(is_valid(v)) {
                    int i_n = v.x + v.y * size.x;

                    terrain_tile& t_n = tiles[i_n];

                    float difference = t.elevation - (t_n.elevation);

                    float of = max(0.0f, t.outflow[j] + difference * outflow_factor * t.distances[j] * outflow_factor);
                    outflows[j] = of - t.outflow[j];

                    ++j;
                }
            }

            float sum = 0.0f;
            for(float f : outflows) sum += f;

            float K = min(1.0f, t.water / sum);
            if(isnan(K)) K = 1.0f;

            for(float& f : outflows) f *= K;
            
            j = 0;
            for(ivec2 v : t.neighbors) {
                t.outflow[j] += outflows[j];
                
                ++j;
            }
        }
    }

    // move water
    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            terrain_tile& t = tiles[i];

            float flow_out = 0.0f;
            float flow_in = 0.0f;

            // outflow
            for(float f : t.outflow) flow_out += f;
            std::vector<float> inflow(4, 0.0f);

            // inflow
            for(int i = 0; i < 4; ++i) {
                ivec2 i_n = t.neighbors[i];
                
                if(is_valid(i_n)) {
                    terrain_tile& t_n = tiles[i_n.x + i_n.y * size.x];

                    int inv_index = inverse_index[i];

                    flow_in += t_n.outflow[inv_index];

                    inflow[i] = t_n.outflow[inv_index];
                }
            }

            t.water_velocity = {
                -t.outflow[1] + t.outflow[2] + inflow[1] - inflow[2],
                -t.outflow[3] + t.outflow[0] + inflow[3] - inflow[0]
            };

            t.water_velocity *= 3.0f;

            float delta_flow = flow_in - flow_out;

            t.delta_flow = delta_flow;

            t.water_temp = t.water + delta_flow;
        }
    }

    //erosion and deposition
    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            terrain_tile& t = tiles[i];

            float max_slope = 0.0f;

            for(ivec2 i_n : t.neighbors) {
                if(is_valid(i_n)) {
                    terrain_tile& t_n = tiles[i_n.x + i_n.y * size.x];

                    float slope = (t.elevation + t.sediment) - (t_n.elevation + t_n.sediment);

                    max_slope = max(max_slope, slope);
                }
            }

            float carry_capacity = max_slope * length(t.water_velocity) * 100.0f;

            float erosion_amount = carry_capacity - t.sediment;

            t.erosion_amount = erosion_amount;
        }
    }

    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            terrain_tile& t = tiles[i];

            float new_sediment = t.sediment + t.erosion_amount;
            new_sediment = max(0.0f, new_sediment);
            float delta = new_sediment - t.sediment;
            //std::cout << delta << " ";

            t.sediment_temp = t.sediment + delta;
            t.elevation_temp = t.elevation - delta;
            t.sediment = t.sediment_temp;
        }
    }

    // move sediment
    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            terrain_tile& t = tiles[i];

            for(int i = 0; i < t.neighbors.size(); ++i) {
                float outflow = t.outflow[i];
                float ratio = outflow / t.water;

                ivec2 i_n = t.neighbors[i];
                if(is_valid(i_n)) {
                    terrain_tile& t_n = tiles[i_n.x + i_n.y * size.x];

                    t_n.sediment += t.sediment_temp * ratio;
                    t.sediment -= t.sediment_temp * ratio;
                }
            }

            /*
            float x_ratio = float(t.self.x) - t.water_velocity.x * 50.0f;
            float y_ratio = float(t.self.y) - t.water_velocity.y * 50.0f;

            int x_0 = floor(x_ratio);
            int y_0 = floor(y_ratio);

            if(is_valid({x_0, y_0}) && is_valid({x_0 + 1, y_0 + 1})) {
                x_ratio -= x_0;
                y_ratio -= y_0;

                int i0 = (x_0) + (y_0) * size.x;
                int i1 = (x_0 + 1) + (y_0) * size.x;
                int i2 = (x_0) + (y_0 + 1) * size.x;
                int i3 = (x_0 + 1) + (y_0 + 1) * size.x;

                float x0 = tiles[i0].sediment_temp * (1.0f - x_ratio) + tiles[i1].sediment_temp * x_ratio;
                float x1 = tiles[i2].sediment_temp * (1.0f - x_ratio) + tiles[i3].sediment_temp * x_ratio;

                float y0 = x0 * (1.0f - y_ratio) + x1 * y_ratio;

                t.sediment = y0;
            } else {
                t.sediment = t.sediment_temp;
            }
            */
        }
    }
    
    for(int y = 0; y < size.y; ++y) {
        for(int x = 0; x < size.x; ++x) {
            int i = x + y * size.x;
            terrain_tile& t = tiles[i];

            t.water = t.water_temp;
            t.elevation = t.elevation_temp;
            
            for(int i = 0; i < 4; ++i) {
                ivec2 i_n = t.neighbors[i];

                if(is_valid(i_n)) {
                    terrain_tile& t_n = tiles[i_n.x + i_n.y * size.x];
                }
            }

            t.water *= 0.8f;
        }
    }

    update_texture();
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