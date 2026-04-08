#include "gui.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "render.hpp"
#include "core.hpp"
#include "erosion.hpp"

const double hexond_ratio = 86400.0 / 65536.0;

float italic_factor = 1.0f / 3.5f;
float bold_factor = 1.0f;
int header = 15;
int buffer = 4;

int hexonds_since_midnight() {
    using namespace std::chrono;

    auto now = system_clock::now();

    auto local = zoned_time{current_zone(), now};

    auto today = floor<days>(local.get_local_time());

    auto since_midnight = local.get_local_time() - today;

    double sec = since_midnight.count() / 1000000000.0;

    return sec / hexond_ratio;
}

int seconds_since_midnight() {
    using namespace std::chrono;

    auto now = system_clock::now();

    auto local = zoned_time{current_zone(), now};

    auto today = floor<days>(local.get_local_time());

    auto since_midnight = local.get_local_time() - today;

    double sec = since_midnight.count() / 1000000000.0;

    return sec;
}

bool includes(ivec2 point, ivec4 range) {
    return (point.x >= range.x && point.x < range.z && point.y >= range.y && point.y < range.w);
}

ivec4 clip(ivec4 range_a, ivec4 range_b) {
    ivec4 range_c = ivec4(glm::max(range_a.x, range_b.x), glm::max(range_a.y, range_b.y), glm::min(range_a.z, range_b.z), glm::min(range_a.w, range_b.w));

    return range_c;
}

bool Window_state::panel_valid(uint32_t id) {
    return id != 0xFFFFFFFF && panels[id].name != "%EMPTY";
}

bool Window_state::panel_active(uint32_t id) {
    return id != 0xFFFFFFFF && panels[id].activated;
}

ivec4 GUI_system::get_space() {
    Window_state& ws = window_state[active_window];

    if(ws.panels.size() > 1) return ws.panels[ws.current_panel].space;
    else return ws.space;
}

ivec2& GUI_system::get_position() {
    Window_state& ws = window_state[active_window];
    
    if(ws.panels.size() > 1) return ws.panels[ws.current_panel].current_pos;
    else return ws.current_pos;
}

void GUI_system::insert_window(std::string label, Window_state state) {
    if(!window_state.contains(label)) {
        state.priority = 0;
        state.display_offsets = vec4(0.0f);

        for(auto& [name, ws] : window_state) {
            if(name != "") ++ws.priority;
        }
        window_state.emplace(label, state);
    }
}

void GUI_system::remove_window(std::string label) {
    Window_state& main_state = window_state[label];
    for(auto& [name, ws] : window_state) {
        if(name != "" && ws.priority > main_state.priority) --ws.priority;
    }
    window_state.erase(label);
}

void GUI_system::make_priority(std::string window) {
    Window_state& main_state = window_state[window];
    for(auto& [name, state] : window_state) {
        if(name != "" && state.priority < main_state.priority) ++state.priority;
    }

    main_state.priority = 0;
}

void GUI_system::insert_vertices(std::vector<UI_vertex>& vertices) {
    Window_state& ws = window_state[active_window];

    ws.vertices.insert(ws.vertices.end(), vertices.begin(), vertices.end());
}

void GUI_system::insert_vertices() {
    std::vector<std::string> names;
    std::vector<std::string> remove;

    for(auto& [name, ws] : window_state) {
        names.push_back(name);
    }

    std::sort(names.begin(), names.end(), 
        [&](std::string a, std::string b) {
            return window_state[a].priority < window_state[b].priority;
        }
    );

    for(std::string s : names) {
        Window_state& ws = window_state[s];

        vertices.insert(vertices.begin(), ws.vertices.begin(), ws.vertices.end());

        if(ws.vertices.size() == 0) remove.push_back(s);
        ws.vertices.clear();
    }
    
    for(std::string s : remove) remove_window(s);
}

void GUI_system::window_capture() {
    std::string min_name = "";
    uint32_t min_priority = 0xFFFFFFFF;

    for(auto& [name, ws] : window_state) {
        ivec4 range = {ivec2(ws.position) + ivec2(0, -ws.size.y - header) - ivec2(buffer), ivec2(ws.position) + ivec2(ws.size.x, 0) + ivec2(buffer)};

        if(name != "" && includes(core.cursor_pos, range)) {
            if(min_priority > ws.priority) {
                min_priority = ws.priority;
                min_name = name;
            }
        }
    }

    capture_window = min_name;
}

std::vector<UI_vertex> create_char(Glyph_data& glyph) {
    std::vector<UI_vertex> ret;

    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    ret.push_back(a);
    ret.push_back(b);
    ret.push_back(d);
    ret.push_back(a);
    ret.push_back(d);
    ret.push_back(c);

    for(UI_vertex& v : ret) {
        v.pos = v.pos * vec2(glyph.size[0], glyph.size[1]);
        v.tex_pos = vec2(glyph.pos_tex[0], glyph.pos_tex[1]) + v.tex_pos * vec2(glyph.size[0], glyph.size[1]);
    }

    return ret;
}

std::string to_base(int32_t num, int base, bool use_i2) {
    std::string ret;

    bool neg = (num < 0);
    num = abs(num);
    while(num > 0) {
        ret += integers[num % base];
        num /= base;
    }

    if(ret.size() == 0) ret = "0";

    if(neg) ret += '-';
    
    std::reverse(ret.begin(), ret.end());

    return ret;
}

std::string to_base(int64_t num, int base, bool use_i2) {
    std::string ret;

    bool neg = (num < 0);
    num = abs(num);
    while(num > 0) {
        if(use_i2) ret += integers_letters[num % base];
        else ret += integers[num % base];
        num /= base;
    }

    if(ret.size() == 0) ret = "0";

    if(neg) ret += '-';
    
    std::reverse(ret.begin(), ret.end());

    return ret;
}

std::string to_base(float num, int base, int max_float, bool use_i2) {
    if(std::isinf(num)) {
        return "INFINITY";
    }

    if(std::isnan(num)) {
        return "NAN";
    }

    std::string integer;
    std::string floating;

    bool neg = (num < 0);
    num = abs(num);

    int i = floor(num);
    float f = num - i;

    while(i > 0) {
        integer += integers[clamp(i % base, 0, 15)];
        i /= base;
    }
    
    if(integer.size() == 0) integer = "0";

    if(neg) integer += '-';

    int f_count = 0;
    while(f != 0.0) {
        f *= base;
        floating += integers[clamp((int)floor(f), 0, 15)];
        f -= floor(f);

        ++f_count;
        if(f_count >= max_float) break;
    }

    if(floating.size() == 0) floating = "0";
    
    std::reverse(integer.begin(), integer.end());

    std::string ret;

    ret = integer + '.' + floating;

    return ret;
}

int from_base(std::string num, int base) {
    int ret = 0;

    bool neg = false;
    if(num[0] == '-') {
        neg = true;
        num.erase(0);
    }

    for(uint8_t c : num) {
        ret *= base;
        if(c >= '0' && c <= '9') ret += c - '0';
        else if(c >= 0x80 && c <= 0x85) ret += c - 0x76;
        else if(c >= 'A' && c <= 'F') ret += c - 'A' + 0xA;
    }

    if(neg) ret *= -1;

    return ret;
}

std::vector<UI_vertex> GUI_system::mesh_text(Font& f, std::string text, uint32_t width, ivec2 select_range, Alignment alignment, bool show_debug) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();
    gui_system.text_line_indices.clear();
    gui_system.text_line_origins.clear();

    uint32_t line_start_index = 0;
    uint32_t word_start_index = 0;

    bool accept_index = false;

    int num_escape_seq = 0;
    int i = 0;

    std::vector<UI_vertex> ret;
    vec2 pos = vec2(0.0f);
    vec4 range = vec4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

    vec4 color = vec4(1.0f);
    bool bold = false;
    bool italic = false;
    bool hex = false;

    uint32_t num_lines = 0;

    std::vector<UI_vertex> word_ret;
    vec2 word_pos = vec2(0.0f);
    
    std::vector<UI_vertex> line_ret;

    auto insert_line = [&]() {
        int line_width = pos.x;
        int offset;

        if(alignment == ALIGN_LEFT) offset = 0.0f;
        else if(alignment == ALIGN_CENTER) offset = round(float(int(width) - line_width) / 2);
        else if(alignment == ALIGN_RIGHT) offset = int(width) - line_width;

        for(UI_vertex& v : line_ret) {
            v.pos.x += offset;
        }
        
        ret.insert(ret.end(), line_ret.begin(), line_ret.end());
        
        line_ret.clear();

        pos.x = 0;
        pos.y -= f.line_height;
        ++num_lines;
        
        gui_system.text_line_indices.push_back(line_start_index);
        gui_system.text_line_origins.push_back(offset);
    };

    auto insert_word = [&]() {
        uint32_t end = pos.x + word_pos.x;

        if(end > width) {
            insert_line();
        }
        
        if(line_ret.size() == 0) line_start_index = word_start_index;

        // insert word
        for(UI_vertex& v : word_ret) {
            v.pos += pos;
        }
        
        line_ret.insert(line_ret.end(), word_ret.begin(), word_ret.end());

        word_ret.clear();
        
        pos.x += word_pos.x;
        word_pos = vec2(0.0f);  
    };

    auto insert_selection = [&](ivec2 pos, ivec2 size) {
        if(word_ret.size() == 0) {
            word_start_index = i;
        }

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        std::vector<UI_vertex> r = {a, b, d, a, d, c};
        for(UI_vertex& v : r) {
            v.pos = vec2(pos) + v.pos * vec2(size);
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(1.0f, 1.0f, 1.0f, 0.35f);
            v.data = 1;
        }
        word_ret.insert(word_ret.end(), r.begin(), r.end());
    };

    auto insert_char = [&](char c) {
        Glyph_data& gd = f.at(c);

        float stride = gd.stride;

        if(word_ret.size() == 0) {
            word_start_index = i;
        }

        if(!gd.visible) {
            UI_vertex v;
            v.pos = word_pos + vec2(stride, 0);
            v.data = 0xFFFFFFFF;
            word_ret.push_back(v);
            word_ret.push_back(v);
            word_ret.push_back(v);

            if(alignment == ALIGN_LEFT) {
                if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.stride, f.line_height});
                word_pos.x += stride;
                
                insert_word();
            } else if(alignment == ALIGN_CENTER) {
                insert_word();

                if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.stride, f.line_height});
                word_pos.x += stride;

                insert_word();
            } else if(alignment == ALIGN_RIGHT) {
                insert_word();

                if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.stride, f.line_height});
                word_pos.x += stride;
            }
        } else {
            std::vector<UI_vertex> vs = create_char(gd);

            for(UI_vertex& v : vs) {
                v.pos += word_pos;
            }

            for(UI_vertex& v : vs) {
                if(italic) {
                    v.pos.x += float(v.pos.y - word_pos.y - f.line_height * 0.5f) * italic_factor;
                }

                v.color = color;
            }

            word_ret.insert(word_ret.end(), vs.begin(), vs.end());

            if(bold) {
                for(UI_vertex& v : vs) {
                    v.pos.x += bold_factor;
                }
                stride += bold_factor;
                
                word_ret.insert(word_ret.end(), vs.begin(), vs.end());
            }
            
            if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.stride + ((bold) ? bold_factor : 0.0f), f.line_height});

            word_pos.x += stride;
        }
    };

    for(i = 0; i < text.size(); ++i) {
        char c = text[i];

        if(c == '\n') {
            insert_word();
            insert_line();
            word_start_index = i;
            line_start_index = i;
            
            if(i >= select_range.x && i < select_range.y && (text[i + 1] == '\n' || i == text.size() - 1)) {
                if(alignment == ALIGN_LEFT) insert_selection(word_pos, {6, f.line_height});
                else if(alignment == ALIGN_CENTER) insert_selection(word_pos - vec2(3, 0), {6, f.line_height});
                else if(alignment == ALIGN_RIGHT) insert_selection(word_pos - vec2(6, 0), {6, f.line_height});
            }

            continue;
        } else {
            if(c == '\\') {
                if(i + 1 < text.size()) {
                    char next = text[i + 1];

                    if(next == 'c') {
                        if(i + 1 + 3 < text.size()) {
                            std::string s(text.begin() + (i + 2), text.begin() + (i + 5));

                            std::size_t i0 = integers_letters.find(s[0]);
                            std::size_t i1 = integers_letters.find(s[1]);
                            std::size_t i2 = integers_letters.find(s[2]);

                            if(i0 != std::string::npos && i1 != std::string::npos && i2 != std::string::npos) {
                                color = vec4(float(i0) / 15.0f, float(i1) / 15.0f, float(i2) / 15.0f, 1.0f);
                                
                                num_escape_seq += 5;
                                if(!show_debug) {    
                                    i += 4;
                                    continue;   
                                }
                            }
                        }
                    } else if(next == 'b') {
                        bold = true;

                        num_escape_seq += 2;
                        if(!show_debug) {
                            i += 1;
                            continue;
                        }
                    } else if(next == 'i') {
                        italic = true;
                        
                        num_escape_seq += 2;
                        if(!show_debug) {
                            i += 1;
                            continue;
                        }

                    } else if(next == 'r') {
                        bold = false;
                        italic = false;
                        
                        num_escape_seq += 2;
                        if(!show_debug) {
                            i += 1;
                            continue;
                        }
                    } else if(next == 'h') {
                        hex = !hex;
                        
                        num_escape_seq += 2;
                        if(!show_debug) {
                            i += 1;
                            continue;
                        }
                    }
                }
            } 
            
            if(show_debug) {
                if(num_escape_seq > 0) {
                    color.w = 0.5f;
                    --num_escape_seq;

                    if(c == 'A') c = '\x80';
                    else if(c == 'B') c = '\x81';
                    else if(c == 'C') c = '\x82';
                    else if(c == 'D') c = '\x83';
                    else if(c == 'E') c = '\x84';
                    else if(c == 'F') c = '\x85';
                } else color.w = 1.0f;
            }

            if(hex) {
                if(c == 'A') c = '\x80';
                else if(c == 'B') c = '\x81';
                else if(c == 'C') c = '\x82';
                else if(c == 'D') c = '\x83';
                else if(c == 'E') c = '\x84';
                else if(c == 'F') c = '\x85';
            }
            
            insert_char(c);
        }
    }

    insert_word();
    insert_line();

    for(UI_vertex& v : ret) {
        range.x = min(range.x, v.pos.x);
        range.y = min(range.y, v.pos.y);
        range.z = max(range.z, v.pos.x);
        range.w = max(range.w, v.pos.y);
    }

    gui_system.text_range = range;
    gui_system.text_lines = num_lines;

    return ret;
}

void GUI_system::call() {
    vertices.clear();

    uint32_t camera = *collectors[0].entities.begin();
    Transform& camera_transform = ecs.get_component<Transform>(camera);

    if(!core.key_map[GLFW_MOUSE_BUTTON_LEFT]) {
        capture = 0xFFFFFFFF;   
        capture_widget = "";
        capture_window = "";
    }
    active_window = "";
    window_capture();

    if(capture == 0xFFFFFFFF) {
        cursor_mode = CURSOR_CLICK;
    }
    
    Physics_system& ps = ecs.get_system<Physics_system>();
    Input_system& input_system = ecs.get_system<Input_system>();
    Erosion_system& erosion_system = ecs.get_system<Erosion_system>();

    int width = core.window.viewport_size.x;
    int height = core.window.viewport_size.y;

    static bool open_settings = false;
    static bool open_stats = false;

    /*
    if(ps.sim_active) {
        toggle_button({width - 8 * icon_scale - 16 * icon_scale, height - 8 * icon_scale - 16 * icon_scale}, {16 * icon_scale, 16 * icon_scale}, {54, 24, 64, 34}, ps.sim_active);
    } else {
        toggle_button({width - 8 * icon_scale - 16 * icon_scale, height - 8 * icon_scale - 16 * icon_scale}, {16 * icon_scale, 16 * icon_scale}, {54, 14, 64, 24}, ps.sim_active);

        static bool step_forward = false;
        bool prev = step_forward;
        toggle_button({width - 8 * icon_scale - 16 * icon_scale, height + (-8 * icon_scale - 16 * icon_scale) * 2.0f}, {16 * icon_scale, 16 * icon_scale}, {44, 54, 54, 64}, step_forward);

        if(!prev && step_forward) {
            Physics_system& ps = ecs.get_system<Physics_system>();
            ps.physics_loop();
        }

        if(!core.key_map[GLFW_MOUSE_BUTTON_LEFT]) step_forward = false;
    }
    */

    if(erosion_system.run_sim) {
        toggle_button({width - 8 * icon_scale - 16 * icon_scale, height - 8 * icon_scale - 16 * icon_scale}, {16 * icon_scale, 16 * icon_scale}, {54, 24, 64, 34}, erosion_system.run_sim);
    } else {
        toggle_button({width - 8 * icon_scale - 16 * icon_scale, height - 8 * icon_scale - 16 * icon_scale}, {16 * icon_scale, 16 * icon_scale}, {54, 14, 64, 24}, erosion_system.run_sim);
    }

    if(open_settings) {
        toggle_button({0.0f, height + (-16 * icon_scale - 8 * icon_scale)}, {24 * icon_scale, 16 * icon_scale}, {54, 54, 64, 64}, open_settings);
    } else {
        toggle_button({0.0f, height + (-16 * icon_scale - 8 * icon_scale)}, {16 * icon_scale, 16 * icon_scale}, {54, 54, 64, 64}, open_settings);
    }
    if(open_settings) {
        Window_state ws;
        ws.size = {384.0f, 384.0f};
        ws.position = {width * 0.5f - ws.size.x * 0.5f, height * 0.5f + ws.size.y * 0.5f};
        ws.label = "settings";

        insert_window("settings_window", ws);
    }

    if(open_stats) {
        toggle_button({0.0f, height + (-16 * icon_scale - 8 * icon_scale) * 2.0f}, {24 * icon_scale, 16 * icon_scale}, {54, 34, 64, 44}, open_stats);
    } else {
        toggle_button({0.0f, height + (-16 * icon_scale - 8 * icon_scale) * 2.0f}, {16 * icon_scale, 16 * icon_scale}, {54, 34, 64, 44}, open_stats);
    }
    if(open_stats) {
        Window_state ws;
        ws.size = {384.0f, 384.0f};
        ws.position = {width * 0.5f - ws.size.x * 0.5f, height * 0.5f + ws.size.y * 0.5f};
        ws.label = "statistics";

        insert_window("stats_window", ws);
    }

    if(open_settings) {
        bool close_window = false;
        window("settings_window", close_window);

        if(close_window) {
            open_settings = false;
        }

        std::string str;
        map_mode next;
        if(erosion_system.mode == MAP_MODE_ELEVATION) {
            next = MAP_MODE_SHADE;
            str = "Map Mode: \\b\\cF44ELEVATION";
        } else if(erosion_system.mode == MAP_MODE_SHADE) {
            next = MAP_MODE_FLOW;
            str = "Map Mode: \\b\\cF4FSHADE";
        } else if(erosion_system.mode == MAP_MODE_FLOW) {
            next = MAP_MODE_BASIN;
            str = "Map Mode: \\b\\c44FFLOW";
        } else if(erosion_system.mode == MAP_MODE_BASIN) {
            next = MAP_MODE_ELEVATION;
            str = "Map Mode: \\b\\c4F4BASINS";
        }

        alignment = ALIGN_LEFT;
        static bool toggle_map_mode;
        bool prev = toggle_map_mode;
        button("map_mode", str, vec2(300, 25), toggle_map_mode);
        if(!prev && toggle_map_mode) {
            erosion_system.mode = next;
            erosion_system.update_texture();
        }
        if(toggle_map_mode && !core.key_map[GLFW_MOUSE_BUTTON_LEFT]) toggle_map_mode = false;
    }

    if(open_stats) {
        bool close_window = false;
        window("stats_window", close_window);

        if(close_window) {
            open_stats = false;
        }

        ivec2 pos = ivec2((floor(input_system.world_cursor_pos) + vec2(erosion_system.size)) / 2.0f);

        std::string elev_text = std::to_string(pos.x) + " " + std::to_string(pos.y);

        std::string total_mass_string;
        float total_mass = 0.0f;
        for(int y = 0; y < erosion_system.size.y; ++y) {
            for(int x = 0; x < erosion_system.size.x; ++x) {
                terrain_tile& tile = erosion_system.tiles[x + y * erosion_system.size.x];

                total_mass += tile.elevation + tile.sediment;
            }
        }
        total_mass_string = to_base(total_mass, 16, 8); 
        
        text("mass_text", total_mass_string);

        if(pos.x >= 0 && pos.x < erosion_system.size.x && pos.y >= 0 && pos.y < erosion_system.size.y) {
            terrain_tile& tile = erosion_system.tiles[pos.x + pos.y * erosion_system.size.x];
            float elevation = tile.elevation;
            elev_text += "\nElevation: " + to_base(elevation, 10, 8) + "\nSediment: " + to_base(tile.sediment, 10, 8) + "\nWater: " + to_base(tile.water, 10, 8) + "\nWater Velocity: " + to_base(tile.water_velocity.x, 10, 8) + " " + to_base(tile.water_velocity.y, 10, 8) + "\nNeighbors: " + to_base(int(tile.upstream.size()), 10) + "\nFlow:" + to_base(tile.flow, 10, 4);
        } else {
            elev_text += "\nOUT OF BOUNDS";
        }

        text("elevation_text", elev_text);
    }

    insert_vertices();
}

void GUI_system::toggle_button(vec2 position, vec2 size, ivec4 icon, bool& active) {
    std::vector<UI_vertex> total_ret;

    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    std::vector<UI_vertex> ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = position + v.pos * size;
        v.tex_pos = vec2(1.0f, 63.0f);
        if(active) v.color = vec4(0.65f);
        else v.color = vec4(0.35f, 0.35f, 0.35f, 0.35f);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    vec2 size_tex = vec2(icon.z - icon.x, icon.w - icon.y);
    float scale_tex = icon_scale;

    float buffer = floor((size.y - size_tex.y * scale_tex) * 0.5f);

    ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = position + vec2(size.x - buffer - size_tex.x * scale_tex, buffer) + v.pos * size_tex * scale_tex;
        v.tex_pos = v.tex_pos * size_tex + vec2(icon.x, icon.y);
        v.color = vec4(1.0f);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    insert_vertices(total_ret);

    ivec4 range = ivec4(position, position + size);
    if(capture == 0xFFFFFFFF) {
        if(includes(core.cursor_pos, range) && core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            capture_operation = 10;
            active = !active;
            capture = 0;
        }
    }
}


void GUI_system::button(vec2 position, vec2 size, ivec4 icon, bool& active) {
    std::vector<UI_vertex> total_ret;

    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    std::vector<UI_vertex> ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = position + v.pos * size;
        v.tex_pos = vec2(1.0f, 63.0f);
        if(active) v.color = vec4(0.65f);
        else v.color = vec4(0.35f, 0.35f, 0.35f, 0.35f);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    vec2 size_tex = vec2(icon.z - icon.x, icon.w - icon.y);
    float scale_tex = icon_scale;

    float buffer = floor((size.y - size_tex.y * scale_tex) * 0.5f);

    ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = position + vec2(size.x - buffer - size_tex.x * scale_tex, buffer) + v.pos * size_tex * scale_tex;
        v.tex_pos = v.tex_pos * size_tex + vec2(icon.x, icon.y);
        v.color = vec4(1.0f);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    insert_vertices(total_ret);

    ivec4 range = ivec4(position, position + size);
    if(capture == 0xFFFFFFFF) {
        if(includes(core.cursor_pos, range) && core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            capture_operation = 10;
            active = true;
            capture = 0;
        }
    }

    if(!core.key_map[GLFW_MOUSE_BUTTON_LEFT]) {
        active = false;
    }
}

ivec4 GUI_system::get_panel_subrange(uint32_t starting_index) {
    Window_state& ws = window_state[active_window];

    ivec4 subrange = ws.space;

    uint32_t id = starting_index;
    Panel_node c_node = ws.panels[id];

    int center = c_node.split_value;
    panel_split starting_split = c_node.split;
    
    while(true) {
        if(c_node.parent == 0xFFFFFFFF) break;
        Panel_node new_node = ws.panels[c_node.parent];

        if(new_node.child_a == id) {
            if(new_node.split == SPLIT_X) {
                subrange.z = min(ws.space.x + new_node.split_value, subrange.z);
            } else if(new_node.split == SPLIT_Y) {
                subrange.w = min(ws.space.y + new_node.split_value, subrange.w);
            }
        } else {
            if(new_node.split == SPLIT_X) {
                subrange.x = max(ws.space.x + new_node.split_value, subrange.x);
            } else if(new_node.split == SPLIT_Y) {
                subrange.y = max(ws.space.y + new_node.split_value, subrange.y);
            }
        }

        id = c_node.parent;
        c_node = new_node;
    }
    
    id = starting_index;
    c_node = ws.panels[id];
    std::unordered_set<uint32_t> visited;

    while(true) {
        if(ws.panel_valid(c_node.child_a) && !visited.contains(c_node.child_a)) {
            id = c_node.child_a;
            visited.insert(c_node.child_a);
            
            c_node = ws.panels[id];
        } else if(ws.panel_valid(c_node.child_b) && !visited.contains(c_node.child_b)) {
            id = c_node.child_b;
            visited.insert(c_node.child_b);

            c_node = ws.panels[id];
        } else {
            visited.insert(id);
            if(id == starting_index) break;
            else id = c_node.parent;

            c_node = ws.panels[id];
        }

        if(c_node.split == starting_split && id != starting_index) {
            if(c_node.split_value < center) {
                if(starting_split == SPLIT_X) {
                    subrange.x = max(c_node.split_value + ws.space.x, subrange.x);
                } else {
                    subrange.y = max(c_node.split_value + ws.space.y, subrange.y);
                }
            } else {
                if(starting_split == SPLIT_X) {
                    subrange.z = min(c_node.split_value + ws.space.x, subrange.z);
                } else {
                    subrange.w = min(c_node.split_value + ws.space.y, subrange.w);
                }
            }
        }
    }

    return subrange;
}

void GUI_system::split_panel(std::string name, int& split, panel_split axis) {
    Window_state& ws = window_state[active_window];

    // split code

    uint32_t current = ws.current_panel;

    Panel_node& current_node = ws.panels[current];

    if(!(current_node.name == "%EMPTY" + std::to_string(current))) {
        if(current_node.name != name && current_node.split != axis) {
            clear_panel(current);
        }
    } else {
        current_node.child_a = ws.panels.size();
        current_node.child_b = ws.panels.size() + 1;

        Panel_node child_a;
        child_a.parent = current;
        child_a.self = current_node.child_a;
        child_a.name = "%EMPTY" + std::to_string(current_node.child_a);
        
        Panel_node child_b;
        child_b.parent = current;
        child_b.self = current_node.child_b;
        child_b.name = "%EMPTY" + std::to_string(current_node.child_b);

        ws.panels.push_back(child_a);
        ws.panels.push_back(child_b);
    }

    ws.panels[current_node.child_a].activated = true;
    ws.panels[current_node.child_b].activated = true;

    uint32_t prev_current = current;

    ws.current_panel = current_node.child_a;

    ws.visited.insert(ws.current_panel);

    //
    
    Panel_node& node = ws.panels[prev_current];
    
    node.split = axis;
    node.name = name;

    if(node.parent == 0xFFFFFFFF) {
        node.space = ws.space;
    }

    ivec4 subrange = node.space;

    std::vector<UI_vertex> total_ret;

    buffer = 4.0f;

    if(axis == SPLIT_X) {
        ivec4 range = ivec4(ivec2(ws.position) + ivec2(split, -ws.size.y - header), ivec2(ws.position) + ivec2(split, -ws.size.y - header) + ivec2(1, ws.size.y));
        range.y = max(range.y, subrange.y);
        range.w = min(range.w, subrange.w);

        ivec4 buffer_range = ivec4(-buffer, 0, buffer, 0);
        
        ivec4 hover_range = ivec4(range.xy(), range.zw()) + buffer_range;

        bool ic = includes(core.cursor_pos, hover_range);

        
        if(ic) {
            cursor_mode = CURSOR_RESIZE_L;

            if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                capture = 2;
                capture_widget = name;
                capture_operation = 0;
                
                make_priority(active_window);
            }
        }

        int min_v = 10;

        if(capture_widget == name && capture != 0xFFFFFFFF) {
            int o_min = subrange.x + min_v;
            int offset_min = core.cursor_pos.x - core.cursor_delta.x - o_min;
            int o_max = subrange.z - min_v;
            int offset_max = core.cursor_pos.x - core.cursor_delta.x - o_max;

            split += core.cursor_delta.x + min(0, offset_min) + max(0, offset_max);
        }
        split = clamp(split, subrange.x - (int)ws.position.x + min_v, subrange.z - (int)ws.position.x - min_v);

        range = ivec4(ivec2(ws.position) + ivec2(split, -ws.size.y - header), ivec2(ws.position) + ivec2(split, -ws.size.y - header) + ivec2(1, ws.size.y));
        range.y = max(range.y, subrange.y);
        range.w = min(range.w, subrange.w);

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        std::vector<UI_vertex> ret = {a, b, d, a, d, c};
        for(UI_vertex& v : ret) {
            v.pos = vec2(range.xy()) + v.pos * vec2(range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.4f, 0.4f, 0.4f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());
    } else {
        ivec4 range = ivec4(ivec2(ws.position) + ivec2(0, split - header - ws.size.y), ivec2(ws.position) + ivec2(0, split - header - ws.size.y) + ivec2(ws.size.x, 1));
        range.x = max(range.x, subrange.x);
        range.z = min(range.z, subrange.z);

        ivec4 buffer_range = ivec4(0, -buffer, 0, buffer);

        ivec4 hover_range = ivec4(range.xy(), range.zw()) + buffer_range;

        bool ic = includes(core.cursor_pos, hover_range);

        if(ic) {
            cursor_mode = CURSOR_RESIZE_T;

            if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                capture = 2;
                capture_widget = name;
                capture_operation = 0;
                
                make_priority(active_window);
            }
        }

        int min_v = 10;

        if(capture_widget == name && capture != 0xFFFFFFFF) {
            int o_min = subrange.y + min_v;
            int offset_min = core.cursor_pos.y - core.cursor_delta.y - o_min;
            int o_max = subrange.w - min_v;
            int offset_max = core.cursor_pos.y - core.cursor_delta.y - o_max;
            split += core.cursor_delta.y + min(0, offset_min) + max(0, offset_max);
        }
        split = clamp(split, subrange.y - int(ws.position.y - ws.size.y - header) + min_v, subrange.w - int(ws.position.y - ws.size.y - header) - min_v);

        range = ivec4(ivec2(ws.position) + ivec2(0, split - header - ws.size.y), ivec2(ws.position) + ivec2(0, split - header - ws.size.y) + ivec2(ws.size.x, 1));
        range.x = max(range.x, subrange.x);
        range.z = min(range.z, subrange.z);

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        std::vector<UI_vertex> ret = {a, b, d, a, d, c};
        for(UI_vertex& v : ret) {
            v.pos = vec2(range.xy()) + v.pos * vec2(range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.4f, 0.4f, 0.4f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());
    }

    node.split_value = split;

    insert_vertices(total_ret);
    
    ivec4 child_a_range = get_panel_subrange(current_node.child_a);
    ws.panels[current_node.child_a].prev_space = ws.panels[current_node.child_a].space;
    ws.panels[current_node.child_a].space = child_a_range;

    if(ws.panels[current_node.child_a].name == "%EMPTY" + std::to_string(current_node.child_a)) scrollbar_panel(current_node.child_a);
    else ws.panels[current_node.child_a].scrollbar = false;

    if(ws.panels[current_node.child_a].scrollbar) {
        ws.panels[current_node.child_a].space.z -= int(ws.scrollbar_width);
    }
    ws.panels[current_node.child_a].current_pos = ivec2(child_a_range.x, child_a_range.w - widget_sep + ws.panels[current_node.child_a].scroll_pos);
    
    ivec4 child_b_range = get_panel_subrange(current_node.child_b);
    ws.panels[current_node.child_b].prev_space = ws.panels[current_node.child_b].space;
    ws.panels[current_node.child_b].space = child_b_range;

    if(ws.panels[current_node.child_b].name == "%EMPTY" + std::to_string(current_node.child_b)) scrollbar_panel(current_node.child_b);
    else ws.panels[current_node.child_b].scrollbar = false;
    
    if(ws.panels[current_node.child_b].scrollbar) {
        ws.panels[current_node.child_b].space.z -= int(ws.scrollbar_width);
    }
    ws.panels[current_node.child_b].current_pos = ivec2(child_b_range.x, child_b_range.w - widget_sep + ws.panels[current_node.child_b].scroll_pos);
    
    current_range = ws.panels[ws.current_panel].space;
}

void GUI_system::clear_panel(uint32_t root) {
    Window_state& ws = window_state[active_window];

    uint32_t id = root;
    Panel_node c_node = ws.panels[id];
    std::unordered_set<uint32_t> visited;

    while(true) {
        if(c_node.child_a != 0xFFFFFFFF && !visited.contains(c_node.child_a)) {
            id = c_node.child_a;
            visited.insert(c_node.child_a);
            
            c_node = ws.panels[id];
        } else if(c_node.child_b != 0xFFFFFFFF && !visited.contains(c_node.child_b)) {
            id = c_node.child_b;
            visited.insert(c_node.child_b);

            c_node = ws.panels[id];
        } else {
            visited.insert(id);
            if(id == root) break;
            else id = c_node.parent;

            c_node = ws.panels[id];
        }
    }

    std::vector<uint32_t> v;
    for(uint32_t vs : visited) {
        v.push_back(vs);
    }

    std::sort(v.begin(), v.end());

    for(int i = v.size() - 1; i >= 0; --i) {
        ws.panels.erase(ws.panels.begin() + v[i]);
    }
}

void GUI_system::step_panel() {
    Window_state& ws = window_state[active_window];

    uint32_t current = ws.current_panel;
    bool v = false;
    
    Panel_node node = ws.panels[current];

    while(true) {
        if(ws.panel_active(node.child_a) && !ws.visited.contains(node.child_a)) {
            current = node.child_a;
            ws.visited.insert(node.child_a);
            
            node = ws.panels[current];
            if(!ws.panel_active(node.child_a) && !ws.panel_active(node.child_b)) break;
        } else if(ws.panel_active(node.child_b) && !ws.visited.contains(node.child_b)) {
            current = node.child_b;
            ws.visited.insert(node.child_b);

            node = ws.panels[current];
            if(!ws.panel_active(node.child_a) && !ws.panel_active(node.child_b)) break;
        } else {
            ws.visited.insert(current);
            if(node.parent == 0xFFFFFFFF) break;
            else current = node.parent;

            node = ws.panels[current];
        }
    }

    ws.current_panel = current;
    
    current_range = ws.panels[ws.current_panel].space;
}

void GUI_system::scrollbar_panel(uint32_t panel_id) {
    Window_state& ws = window_state[active_window];
    Panel_node& panel = ws.panels[panel_id];

    ivec2 size = panel.space.zw() - panel.space.xy();
    int height = panel.prev_space.w - ((panel.current_pos.y - widget_sep) - panel.scroll_pos);

    bool scrollbar = false;

    int scrollbar_height;
    ivec4 scrollbar_range;
    
    if(height > size.y) {
        scrollbar = true;

        scrollbar_height = size.y * (float(size.y) / height);
        float ratio = 1.0 - (float)panel.scroll_pos / (height - size.y);
        int32_t r = size.y - scrollbar_height;
        float rrr = ratio;

        scrollbar_range = {panel.space.xy() + ivec2(size.x - ws.scrollbar_width, r * rrr), panel.space.xy() + ivec2(size.x, scrollbar_height + r * rrr)};
    }

    std::vector<UI_vertex> total_ret;
    
    int scroll_speed = 60;
    if(includes(core.cursor_pos, panel.space) && capture_window == active_window) {
        if(scrollbar) panel.scroll_pos = panel.scroll_pos + core.scroll_delta * scroll_speed * -1.0f;
        if(core.scroll_delta > 0) panel.bottom_lock = false;
    }

    if(panel.scroll_pos + size.y >= height) panel.bottom_lock = true;
    if(height < size.y) {
        panel.bottom_lock = false;
        panel.scroll_pos = 0;
    }

    if(panel.scrollbar) panel.scroll_pos = clamp(panel.scroll_pos, 0, int(height - size.y));
    if(panel.bottom_lock) panel.scroll_pos = height - size.y;

    panel.scrollbar = scrollbar;
    
    if(scrollbar) {
        scrollbar_height = size.y * (float(size.y) / height);
        float ratio = 1.0 - (float)panel.scroll_pos / (height - size.y);
        int32_t r = size.y - scrollbar_height;
        float rrr = ratio;
        scrollbar_range = {panel.space.xy() + ivec2(size.x - ws.scrollbar_width, r * rrr), panel.space.xy() + ivec2(size.x, scrollbar_height + r * rrr)};

        
        if(includes(core.cursor_pos, scrollbar_range)) {
            cursor_mode = CURSOR_CLICK;
            capture_operation = 1;
            
            if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                capture_position = (core.cursor_pos.y - scrollbar_range.y) / size.y;

                capture_widget = panel.name;
                capture_operation = 1;

                make_priority(active_window);
            }
        }
    

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        std::vector<UI_vertex> ret = {a, b, d, a, d, c};
        vec4 range = {panel.space.xy() + ivec2(size.x - ws.scrollbar_width, 0.0f), panel.space.xy() + ivec2(size.x, size.y)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.15f, 0.15f, 0.15f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        ret = {a, b, d, a, d, c};
        range = scrollbar_range;
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.35f, 0.35f, 0.35f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        if(capture_widget == panel.name && capture_operation == 1) {
            ivec2 scroll_range = {0, (height - size.y)};
            ivec2 cursor_range = {panel.space.y, panel.space.w - scrollbar_height};
            int capture_pos = capture_position * size.y;

            int cursor_pos = core.cursor_pos.y - capture_pos;

            float cursor_rel = 1.0f - clamp(float(cursor_pos - cursor_range.x) / (cursor_range.y - cursor_range.x), 0.0f, 1.0f);

            if(cursor_rel != 1.0f) panel.bottom_lock = false;

            panel.scroll_pos = scroll_range.x + cursor_rel * (scroll_range.y - scroll_range.x);
        }
    }

    insert_vertices(total_ret);
}

void GUI_system::window(std::string name, bool& close_window) {
    active_window = name;
    int scroll_speed = 60;

    int tex_size = 1;
    float shadow_width = 6;

    Window_state& ws = window_state[name];
    vec2& position = ws.position;
    vec2& size = ws.size;

    bool scrollbar = false;
    int scrollbar_height = 0;

    vec2 scrollbar_rr = ws.scrollbar_r;
    if(ws.scrollbar_r.x == -1 && ws.scrollbar_r.y == -1) scrollbar_rr = vec2(0, ws.size.y);

    ivec4 scrollbar_range;

    int y_size = (get_position().y - widget_sep) - (position.y - header);
    int height = abs(y_size - ws.scroll_pos);

    ws.scroll_pos = clamp(ws.scroll_pos, 0, max(0, height - int(ws.size.y)));

    if(height > size.y && !(ws.panels.size() > 1)) {
        scrollbar = true;

        scrollbar_height = ws.size.y * (float(ws.size.y) / height);
        float ratio = 1.0f - ws.scroll_pos / (height - ws.size.y);
        int32_t r = size.y - scrollbar_height;
        float rrr = ratio;

        scrollbar_range = {position + vec2(size.x - ws.scrollbar_width, -size.y - header + r * rrr), position + vec2(size.x, -size.y - header + scrollbar_height + r * rrr)};
    }
    
    ivec4 buffer_range = ivec4(-buffer, -buffer, buffer, buffer);

    ivec4 range_move = {position + vec2(0, -header), position + vec2(size.x, 0)};
    range_move += buffer_range;

    buffer = 4.0f;
    buffer_range = ivec4(-buffer, -buffer, buffer, buffer);

    ivec4 range_left = {position + vec2(0, -header - size.y), position + vec2(0, 0)};
    ivec4 range_right = {position + vec2(size.x, -header - size.y), position + vec2(size.x, 0)};
    ivec4 range_top = {position + vec2(0, 0), position + vec2(size.x, 0)};
    ivec4 range_bottom = {position + vec2(0, -header - size.y), position + vec2(size.x, -header - size.y)};
    ivec4 hover_range = {position + vec2(0, -header - size.y), position + vec2(size.x, -header)};

    ivec4 range_close = {position + vec2(size.x - header, -header), position + vec2(size.x, 0.0f)};

    range_left += buffer_range;
    range_right += buffer_range;
    range_top += buffer_range;
    range_bottom += buffer_range;

    vec2 min_size = vec2(192, 192);

    auto resize_left = [&]() {
        float delta_min;
        ws.position.x += core.cursor_delta.x;
        ws.size.x -= core.cursor_delta.x;

        if(core.cursor_pos.x > position.x) {
            float delta_max = (core.cursor_pos.x) - position.x;
            ws.position.x += delta_max;
            ws.size.x -= delta_max;
        }

        delta_min = max(0.0f, min_size.x - size.x);
        ws.size.x += delta_min;
        ws.position.x -= delta_min;
    };

    auto resize_right = [&]() {
        float delta_min;

        ws.size.x += core.cursor_delta.x;

        if(core.cursor_pos.x < position.x + size.x) {
            float delta_max = (position.x + size.x) - (core.cursor_pos.x);
            ws.size.x -= delta_max;
        }

        delta_min = max(0.0f, min_size.x - size.x);
        ws.size.x += delta_min;
    };

    auto resize_top = [&]() {
        float delta_min;
        ws.position.y += core.cursor_delta.y;
        ws.size.y += core.cursor_delta.y;

        if(core.cursor_pos.y < position.y) {
            float delta_max = (core.cursor_pos.y) - position.y;
            ws.position.y += delta_max;
            ws.size.y += delta_max;
        }

        delta_min = max(0.0f, min_size.y - size.y);
        ws.size.y += delta_min;
        ws.position.y += delta_min;  
    };

    auto resize_bottom = [&]() {
        float delta_min;

        ws.size.y -= core.cursor_delta.y;

        if(core.cursor_pos.y > position.y - size.y - header) {
            float delta_max = (position.y - size.y - header) - (core.cursor_pos.y);
            ws.size.y += delta_max;
        }

        delta_min = max(0.0f, min_size.y - size.y);
        ws.size.y += delta_min;
    };

    if(capture != 0xFFFFFFFF && capture_widget == name) {
        switch(capture_operation) {
            case 0:
                position += core.cursor_delta;
                break;
            case 1:
                resize_left();

                break;
            case 2:
                resize_right();

                break;
            case 3:
                resize_bottom();

                break;
            case 4:
                resize_left();
                resize_bottom();

                break;
            case 5:
                resize_right();
                resize_bottom();

                break;
            case 6:
                resize_top();

                break;
            case 7:
                resize_left();
                resize_top();

                break;
            case 8:
                resize_right();
                resize_top();

                break;
        }
    }

    if(capture == 0xFFFFFFFF && capture_window == name) {
        uint32_t operation = 0xFFFFFFFF;

        bool left_cont = includes(core.cursor_pos, range_left);
        bool right_cont = includes(core.cursor_pos, range_right);
        bool top_cont = includes(core.cursor_pos, range_top);
        bool bottom_cont = includes(core.cursor_pos, range_bottom);
        
        if(left_cont && bottom_cont) {
            cursor_mode = CURSOR_RESIZE_BL;
            operation = 4;
        } else if(right_cont && bottom_cont) {
            cursor_mode = CURSOR_RESIZE_BR;
            operation = 5;
        } else if(left_cont && top_cont) {
            cursor_mode = CURSOR_RESIZE_TL;
            operation = 7;
        } else if(right_cont && top_cont) {
            cursor_mode = CURSOR_RESIZE_TR;
            operation = 8;
        } else if(left_cont) {
            cursor_mode = CURSOR_RESIZE_L;
            operation = 1;
        } else if(right_cont) {
            cursor_mode = CURSOR_RESIZE_R;
            operation = 2;
        } else if(bottom_cont) {
            cursor_mode = CURSOR_RESIZE_B;
            operation = 3;
        } else if(top_cont) {
            cursor_mode = CURSOR_RESIZE_T;
            operation = 6;
        }

        if(includes(core.cursor_pos, scrollbar_range)) {
            cursor_mode = CURSOR_CLICK;
            operation = 11;
            capture_position = (core.cursor_pos.y - scrollbar_range.w) / ws.size.y;
        }
        
        if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            if(operation != 0xFFFFFFFF) {
                capture = 1;
                capture_widget = name;
                capture_operation = operation;
                
                make_priority(name);
            } else {
                if(includes(core.cursor_pos, range_move)) {
                    capture = 2;
                    capture_widget = name;
                    capture_operation = 0;
                    
                    make_priority(name);
                }

                if(includes(core.cursor_pos, range_close)) {
                    capture = 3;
                    close_window = true;
                }
            }
        }
    }
    
    if(includes(core.cursor_pos, hover_range) && capture_window == name) {
        if(scrollbar && capture_window == name) {
            ws.scroll_pos = clamp(ws.scroll_pos + core.scroll_delta * scroll_speed * -1.0f, 0.0f, (height - ws.size.y));
        }

        if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            capture = 4;
            make_priority(name);
        }
    }

    vec4 header_range = vec4(position + vec2(0.0f, -header), position + vec2(size.x, 0.0f));
    
    ws.current_pos = position + vec2(0.0f, -header + ws.scroll_pos) + vec2(0.0f, -widget_sep);

    ws.space = {position + vec2(0.0f, -header - size.y), position + vec2(size.x, -header)};
    if(scrollbar) ws.space.z -= ws.scrollbar_width;
    
    current_range = ws.space;

    std::vector<UI_vertex> total_ret;

    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    std::vector<UI_vertex> ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = position + vec2(0.0, -size.y - header) + v.pos * size;
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.25f, 0.25f, 0.25f, 1.0f);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = position + vec2(0.0, -header) + v.pos * vec2(size.x, header);
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(1.0f, 0.35f, 0.35f, 1.0f);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    ret = mesh_text(fonts["default mono"], ws.label);

    for(UI_vertex& v : ret) {
        float s = floor(header * 0.5f - 11.0f * float(text_scale) * 0.5f);
        v.pos = position + v.pos * float(text_scale) + vec2(s, -s - 11.0f * float(text_scale));
        v.range = header_range;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    vec4 range = vec4(59, 9, 64, 14);

    ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        float s = floor(header * 0.5f - 5.0f * float(text_scale) * 0.5f);
        vec2 icon_size = vec2(range.z - range.x, range.w - range.y);

        v.pos = position + v.pos * float(text_scale) * icon_size + vec2(size.x - s - 5.0f * float(text_scale), -s - 5.0f * float(text_scale));
        v.tex_pos = v.tex_pos * icon_size + vec2(range.x, range.y);
        v.color = vec4(1.0f);
        v.data = 1;
        v.range = header_range;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    // scrollbar
    if(scrollbar) {
        scrollbar_height = scrollbar_rr.y * (float(ws.size.y) / height);
        float ratio = 1.0f - ws.scroll_pos / (height - ws.size.y);
        int32_t r = scrollbar_rr.y - scrollbar_height;
        float rrr = ratio;
        scrollbar_range = {position + vec2(size.x - ws.scrollbar_width, -size.y - header + scrollbar_rr.x + r * rrr), position + vec2(size.x, -size.y - header + scrollbar_rr.x + scrollbar_height + r * rrr)};
    

        ret = {a, b, d, a, d, c};
        range = {position + vec2(size.x - ws.scrollbar_width, scrollbar_rr.x - header - size.y), position + vec2(size.x, scrollbar_rr.x + scrollbar_rr.y - header - size.y)};
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.15f, 0.15f, 0.15f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        ret = {a, b, d, a, d, c};
        range = scrollbar_range;
        for(UI_vertex& v : ret) {
            v.pos = range.xy() + v.pos * (range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(0.35f, 0.35f, 0.35f, 1.0f);
            v.data = 1;
        }
        total_ret.insert(total_ret.end(), ret.begin(), ret.end());

        if(capture_widget == name && capture_operation == 11) {
            ivec2 scroll_range = {0, (height - ws.size.y)};
            ivec2 cursor_range = {position.y - header, position.y - header - ws.size.y + (scrollbar_range.w - scrollbar_range.y)};
            int capture_pos = capture_position * ws.size.y;

            int cursor_pos = core.cursor_pos.y - capture_pos;

            float cursor_rel = clamp(float(cursor_pos - cursor_range.x) / (cursor_range.y - cursor_range.x), 0.0f, 1.0f);
            ws.scroll_pos = scroll_range.x + cursor_rel * (scroll_range.y - scroll_range.x);
        }
    } else {

    }
    
    // shadow
    float shadow_w = 0.25f;
    
    // left
    ret = {a, b, d, a, d, c};
    ret[0].color.w = 0.0f;
    ret[3].color.w = 0.0f;
    ret[5].color.w = 0.0f;
    range = {position + vec2(-shadow_width, -size.y - header), position};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * (range.zw() - range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    // top left
    ret = {a, b, c, b, d, c};
    ret[0].color.w = 0.0f;
    ret[2].color.w = 0.0f;
    ret[4].color.w = 0.0f;
    ret[5].color.w = 0.0f;
    range = {position + vec2(-shadow_width, 0.0f), position + vec2(0.0f, shadow_width)};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * (range.zw() - range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());
    
    // bottom left
    ret = {a, b, d, a, d, c};
    ret[0].color.w = 0.0f;
    ret[1].color.w = 0.0f;
    ret[3].color.w = 0.0f;
    ret[5].color.w = 0.0f;
    range = {position + vec2(-shadow_width, -size.y - header - shadow_width), position + vec2(0.0f, -size.y - header)};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * (range.zw() - range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    // right
    ret = {a, b, d, a, d, c};
    ret[1].color.w = 0.0f;
    ret[2].color.w = 0.0f;
    ret[4].color.w = 0.0f;
    range = {position + vec2(size.x, -size.y - header), position + vec2(size.x + shadow_width, 0.0f)};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * (range.zw() - range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    // top right
    ret = {a, b, d, a, d, c};
    ret[1].color.w = 0.0f;
    ret[2].color.w = 0.0f;
    ret[4].color.w = 0.0f;
    ret[5].color.w = 0.0f;
    range = {position + vec2(size.x, 0.0f), position + vec2(size.x + shadow_width, shadow_width)};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * (range.zw() - range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());
    

    //bottom right
    ret = {a, b, c, b, d, c};
    ret[0].color.w = 0.0f;
    ret[1].color.w = 0.0f;
    ret[3].color.w = 0.0f;
    ret[4].color.w = 0.0f;
    range = {position + vec2(size.x, -size.y - header - shadow_width), position + vec2(size.x + shadow_width, -size.y - header)};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * (range.zw() - range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    // top
    ret = {a, b, d, a, d, c};
    ret[2].color.w = 0.0f;
    ret[4].color.w = 0.0f;
    ret[5].color.w = 0.0f;
    range = {position + vec2(0.0f, 0.0f), position + vec2(size.x, shadow_width)};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * (range.zw() - range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    // bottom
    ret = {a, b, d, a, d, c};
    ret[0].color.w = 0.0f;
    ret[1].color.w = 0.0f;
    ret[3].color.w = 0.0f;
    range = {position + vec2(0.0f, -size.y - header - shadow_width), position + vec2(size.x, -size.y - header)};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * (range.zw() - range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, v.color.w * shadow_w);
        v.data = 1;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());
    

    insert_vertices(total_ret);

    ws.current_panel = 0;
    ws.visited.clear();

    if(!ws.panels.size()) ws.panels.resize(1);
    for(auto& panel : ws.panels) panel.activated = false;

    if(window_state[active_window].clear_selection) {
        window_state[active_window].select_position = -1;
        window_state[active_window].select_anchor = -1;
        window_state[active_window].select_widget = "";
    }

    window_state[active_window].clear_selection = false;
    if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) window_state[active_window].clear_selection = true;
}

void GUI_system::text(std::string name, std::string text, uint32_t width) {
    current_widget = name;

    if(width = 0xFFFFFFFF) {
        width = (get_space().z - get_space().x) - widget_sep * 2;
    }

    Font& f = fonts["default mono"];

    ivec2 select_range = {-1, -1};

    if(window_state[active_window].select_widget == name) select_range = {min(window_state[active_window].select_position, window_state[active_window].select_anchor), max(window_state[active_window].select_position, window_state[active_window].select_anchor)};

    std::vector<UI_vertex> ret = mesh_text(f, text, width, select_range, text_alignment);
    
    int boundary = (get_space().z - get_space().x) - width;

    vec2 origin = get_position() + ivec2(boundary * 0.5f, 0);

    for(UI_vertex& v : ret) {
        v.pos = origin + v.pos * float(text_scale) + vec2(0.0f, -f.line_height * float(text_scale));
        v.range = current_range;
    }
    
    get_position().y -= float(text_lines * text_scale * f.line_height);
    
    // handle cursor position
    if((active_window == capture_window && includes(core.cursor_pos, get_space())) || (capture_widget == name && core.key_map[GLFW_MOUSE_BUTTON_LEFT])) insert_cursor(text, origin);

    //
    if(window_state[active_window].select_widget == name && window_state[active_window].select_position == window_state[active_window].select_anchor) {
        ivec2 pixel = get_text_cursor_pos(text, origin);

        ivec4 range = ivec4(pixel.x - 2, pixel.y, pixel.x, pixel.y + f.line_height);

        std::vector<UI_vertex> total_ret;

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        std::vector<UI_vertex> r = {a, b, d, a, d, c};
        for(UI_vertex& v : r) {
            v.pos = vec2(range.xy()) + v.pos * vec2(range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(1.0f);
            v.range = current_range;

            v.data = 1;
        }
        ret.insert(ret.end(), r.begin(), r.end());
    }
    
    if(window_state[active_window].select_widget == name && window_state[active_window].priority == 0) text_navigate(text);

    insert_vertices(ret);
}


void GUI_system::text(std::string name, std::string text, vec2 position, uint32_t width) {
    current_widget = name;

    if(width == 0xFFFFFFFF) {
        width = (get_space().z - get_space().x) - widget_sep * 2;
    }

    Font& f = fonts["default mono"];

    ivec2 select_range = {-1, -1};

    if(window_state[active_window].select_widget == name) select_range = {min(window_state[active_window].select_position, window_state[active_window].select_anchor), max(window_state[active_window].select_position, window_state[active_window].select_anchor)};

    std::vector<UI_vertex> ret = mesh_text(f, text, width, select_range, text_alignment);

    int alignment_offset = 0;
    if(alignment == ALIGN_RIGHT) alignment_offset = width - text_range.z;
    
    int boundary = (get_space().z - get_space().x) - width;

    vec2 origin = vec2(get_position()) + position + vec2(alignment_offset, 0);

    for(UI_vertex& v : ret) {
        v.pos = vec2(origin) + v.pos * float(text_scale) + vec2(0.0f, -f.line_height * float(text_scale));
        v.range = current_range;
    }
    
    get_position().y += position.y - float(text_lines * text_scale * f.line_height);
    
    // handle cursor position
    if((active_window == capture_window && includes(core.cursor_pos, get_space())) || (capture_widget == name && core.key_map[GLFW_MOUSE_BUTTON_LEFT])) insert_cursor(text, origin);

    //
    if(window_state[active_window].select_widget == name && window_state[active_window].select_position == window_state[active_window].select_anchor) {
        ivec2 pixel = get_text_cursor_pos(text, origin);

        ivec4 range = ivec4(pixel.x - 2, pixel.y, pixel.x, pixel.y + f.line_height);

        std::vector<UI_vertex> total_ret;

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        std::vector<UI_vertex> r = {a, b, d, a, d, c};
        for(UI_vertex& v : r) {
            v.pos = vec2(range.xy()) + v.pos * vec2(range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(1.0f);
            v.range = current_range;

            v.data = 1;
        }
        ret.insert(ret.end(), r.begin(), r.end());
    }
    
    if(window_state[active_window].select_widget == name && window_state[active_window].priority == 0) text_navigate(text);

    insert_vertices(ret);
}

void GUI_system::slider(std::string name, std::string text, ivec2 bounds, int& value, vec2 size, float slider_width) {
    vec4 range;

    if(alignment == ALIGN_LEFT) {
        int x_pos = widget_sep;
        range = {get_position() + ivec2(x_pos, -size.y), get_position() + ivec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_CENTER) {
        int x_pos = ((current_range.z - current_range.x) * 0.5f - size.x * 0.5f);
        range = {get_position() + ivec2(x_pos, -size.y), get_position() + ivec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_RIGHT) {
        int x_pos = ((current_range.z - current_range.x) - size.x - widget_sep);
        range = {get_position() + ivec2(x_pos, -size.y), get_position() + ivec2(x_pos + size.x, 0.0f)};
    }

    std::vector<UI_vertex> total_ret;

    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    std::vector<UI_vertex> ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * size;
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(1.0f, 1.0f, 1.0f, 0.35f);
        v.data = 1;
        v.range = current_range;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    float s = (value - bounds.x) / float(bounds.y - bounds.x);
    
    ret = {a, b, d, a, d, c};
    vec4 slider_range = {range.xy() + vec2(s * (size.x - slider_width), 0), range.xy() + vec2(s * (size.x - slider_width) + slider_width, size.y)};
    for(UI_vertex& v : ret) {
        v.pos = slider_range.xy() + v.pos * (slider_range.zw() - slider_range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(1.0f, 1.0f, 1.0f, 0.65f);
        v.data = 1;
        v.range = current_range;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    ret = mesh_text(fonts["default mono"], text);
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * float(text_scale) + round(-vec2((text_range.z - text_range.x) * text_scale, (text_range.w - text_range.y) * text_scale) * 0.5f + size * 0.5f);
        
        v.range = current_range;
    }

    total_ret.insert(total_ret.end(), ret.begin(), ret.end());
    
    insert_vertices(total_ret);

    if(capture != 0xFFFFFFFF && capture_widget == name) {
        float rel_pos = core.cursor_pos.x - range.x - capture_position * (range.z - range.x);

        float i = rel_pos / ((range.z - range.x) - slider_width);

        i *= float(bounds.y - bounds.x);
        i = round(i);

        value = clamp(int(i), bounds.x, bounds.y);
    }

    if(includes(core.cursor_pos, slider_range)) {
        if(capture == 0xFFFFFFFF || capture_widget == name) cursor_mode = CURSOR_RESIZE_L;
        if(capture_widget == "") {
            if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                capture = 6;
                capture_widget = name;
                capture_operation = 0;
                capture_position = (core.cursor_pos.x - slider_range.x) / (range.z - range.x);
            }
        }
    }

    get_position().y -= size.y + widget_sep;
}


void GUI_system::slider(std::string name, std::string text, vec2 bounds, float& value, vec2 size, float slider_width, float precision) {
    vec4 range;

    if(alignment == ALIGN_LEFT) {
        int x_pos = widget_sep;
        range = {get_position() + ivec2(x_pos, -size.y),  get_position() + ivec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_CENTER) {
        int x_pos = ((current_range.z - current_range.x) * 0.5f - size.x * 0.5f);
        range = {get_position() + ivec2(x_pos, -size.y),  get_position() + ivec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_RIGHT) {
        int x_pos = ((current_range.z - current_range.x) - size.x - widget_sep);
        range = {get_position() + ivec2(x_pos, -size.y),  get_position() + ivec2(x_pos + size.x, 0.0f)};
    }

    std::vector<UI_vertex> total_ret;

    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    std::vector<UI_vertex> ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * size;
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(1.0f, 1.0f, 1.0f, 0.35f);
        v.data = 1;
        v.range = current_range;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    float s = (value - bounds.x) / float(bounds.y - bounds.x);
    
    ret = {a, b, d, a, d, c};
    vec4 slider_range = {range.xy() + vec2(s * (size.x - slider_width), 0), range.xy() + vec2(s * (size.x - slider_width) + slider_width, size.y)};
    for(UI_vertex& v : ret) {
        v.pos = slider_range.xy() + v.pos * (slider_range.zw() - slider_range.xy());
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(1.0f, 1.0f, 1.0f, 0.65f);
        v.data = 1;
        v.range = current_range;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    ret = mesh_text(fonts["default mono"], text);
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * float(text_scale) + round(-vec2((text_range.z - text_range.x) * text_scale, (text_range.w - text_range.y) * text_scale) * 0.5f + size * 0.5f);
        
        v.range = current_range;
    }

    total_ret.insert(total_ret.end(), ret.begin(), ret.end());
    
    insert_vertices(total_ret);

    if(capture != 0xFFFFFFFF && capture_widget == name) {
        float rel_pos = core.cursor_pos.x - range.x - capture_position * (range.z - range.x);

        float i = rel_pos / ((range.z - range.x) - slider_width);

        i *= float(bounds.y - bounds.x);

        if(precision != 0.0f) i = round(i / precision) * precision;

        value = clamp(i, bounds.x, bounds.y);
    }

    if(includes(core.cursor_pos, slider_range)) {
        if(capture == 0xFFFFFFFF || capture_widget == name) cursor_mode = CURSOR_RESIZE_L;
        if(capture_widget == "") {
            if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                capture = 7;
                capture_widget = name;
                capture_operation = 0;
                capture_position = (core.cursor_pos.x - slider_range.x) / (range.z - range.x);
            }
        }
    }
    
    get_position().y -= size.y + widget_sep;
}

void GUI_system::button(std::string name, std::string text, vec2 size, bool& active) {
    vec4 range;
    
    if(alignment == ALIGN_LEFT) {
        int x_pos = widget_sep;
        range = {get_position() + ivec2(x_pos, -size.y),  get_position() + ivec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_CENTER) {
        int x_pos = ((current_range.z - current_range.x) * 0.5f - size.x * 0.5f);
        range = {get_position() + ivec2(x_pos, -size.y),  get_position() + ivec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_RIGHT) {
        int x_pos = ((current_range.z - current_range.x) - size.x - widget_sep);
        range = {get_position() + ivec2(x_pos, -size.y),  get_position() + ivec2(x_pos + size.x, 0.0f)};
    }

    std::vector<UI_vertex> total_ret;

    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    std::vector<UI_vertex> ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * size;
        v.tex_pos = vec2(1.0f, 63.0f);
        if(active) v.color = vec4(1.0f, 1.0f, 1.0f, 0.65f);
        else v.color = vec4(1.0f, 1.0f, 1.0f, 0.35f);
        v.data = 1;
        v.range = current_range;
    }
    total_ret.insert(total_ret.end(), ret.begin(), ret.end());

    ret = mesh_text(fonts["default mono"], text);
    for(UI_vertex& v : ret) {
        v.pos = range.xy() + v.pos * float(text_scale) - round(vec2((text_range.z - text_range.x) * text_scale, (text_range.w - text_range.y) * text_scale) * 0.5f - size * 0.5f);
        
        v.range = current_range;
    }

    total_ret.insert(total_ret.end(), ret.begin(), ret.end());
    
    insert_vertices(total_ret);

    if(capture_window == active_window && includes(core.cursor_pos, current_range) && includes(core.cursor_pos, range)) {
        if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            capture = 8;
            capture_widget = name;
            capture_operation = 0;
        }
    }

    if(!core.key_map[GLFW_MOUSE_BUTTON_LEFT]) active = false;
    
    if(capture != 0xFFFFFFFF && capture_widget == name) {
        active = true;
    }
    
    get_position().y -= size.y + widget_sep;
}

void GUI_system::chat_window() {
    Window_state& state = window_state[active_window];
    float shadow_width = 6;
    float shadow_w = 0.25f;

    std::vector<UI_vertex> vs;
    UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
    UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
    UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
    UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

    /*
    std::vector<UI_vertex> ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = state.position + vec2(0.0f, -state.size.y - header) + v.pos * vec2(state.size.x, 20.0f);
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.25f, 0.25f, 0.25f, 1.0f);
        v.data = 1;
    }
    vs.insert(vs.end(), ret.begin(), ret.end());
    
    ret = {a, b, d, a, d, c};
    for(UI_vertex& v : ret) {
        v.pos = state.position + vec2(0.0f, -state.size.y - header) + v.pos * vec2(state.size.x - state.scrollbar_width, shadow_width) + vec2(0.0f, 20.0f);
        v.tex_pos = vec2(1.0f, 63.0f);
        v.color = vec4(0.0f, 0.0f, 0.0f, shadow_w);
        v.data = 1;
    }
    ret[2].color.w = 0.0f;
    ret[4].color.w = 0.0f;
    ret[5].color.w = 0.0f;
    vs.insert(vs.end(), ret.begin(), ret.end());
    
    current_range.y += 20.0f;
    state.scrollbar_r = vec2(20.0f, state.size.y - 20.0f);
    */

    uint i = 0;
    for(Chat_message& message : chat.messages) {
        int width = state.size.x;
        int inset = widget_sep;

        text("%CHAT" + std::to_string(i), message.sender + "\\r\\cFFF >\n" + message.message, vec2(inset, 0.0), width - inset - widget_sep);
        
        if(i + 1 < chat.messages.size()) get_position() += vec2(0.0f, -widget_sep);
        
        ++i;
    }   

    //insert_vertices(vs);
}

void GUI_system::text_input(std::string name, std::string& text, uint32_t width) {
    current_widget = name;

    if(width = 0xFFFFFFFF) {
        width = (get_space().z - get_space().x) - widget_sep * 2;
    }

    Font& f = fonts["default mono"];

    ivec2 select_range = {-1, -1};

    if(window_state[active_window].select_widget == name) select_range = {min(window_state[active_window].select_position, window_state[active_window].select_anchor), max(window_state[active_window].select_position, window_state[active_window].select_anchor)};

    std::vector<UI_vertex> ret = mesh_text(f, text, width, select_range, text_alignment, true);
    
    int boundary = (get_space().z - get_space().x) - width;

    vec2 origin = get_position() + ivec2(boundary * 0.5f, 0);

    for(UI_vertex& v : ret) {
        v.pos = origin + v.pos * float(text_scale) + vec2(0.0f, -f.line_height * float(text_scale));
        v.range = current_range;
    }
    
    get_position().y -= float(text_lines * text_scale * f.line_height);
    
    // handle cursor position
    if((active_window == capture_window && includes(core.cursor_pos, get_space())) || (capture_widget == name && core.key_map[GLFW_MOUSE_BUTTON_LEFT])) {
        insert_cursor(text, origin, true);
    }

    //
    if(window_state[active_window].select_widget == name && window_state[active_window].select_position == window_state[active_window].select_anchor) {
        ivec2 pixel = get_text_cursor_pos(text, origin, true);

        ivec4 range = ivec4(pixel.x - 2, pixel.y, pixel.x, pixel.y + f.line_height);

        std::vector<UI_vertex> total_ret;

        UI_vertex a = {vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), vec4(1.0f)};
        UI_vertex b = {vec2(1.0f, 0.0f), vec2(1.0f, 0.0f), vec4(1.0f)};
        UI_vertex c = {vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec4(1.0f)};
        UI_vertex d = {vec2(1.0f, 1.0f), vec2(1.0f, 1.0f), vec4(1.0f)};

        std::vector<UI_vertex> r = {a, b, d, a, d, c};
        for(UI_vertex& v : r) {
            v.pos = vec2(range.xy()) + v.pos * vec2(range.zw() - range.xy());
            v.tex_pos = vec2(1.0f, 63.0f);
            v.color = vec4(1.0f);
            v.range = current_range;

            v.data = 1;
        }
        ret.insert(ret.end(), r.begin(), r.end());
    }
    
    if(window_state[active_window].select_widget == name && window_state[active_window].priority == 0) text_navigate(text, true, true);

    insert_vertices(ret);
}

void GUI_system::insert_cursor(std::string text, vec2 origin, bool show_debug) {
    Font& f = fonts["default mono"];

    vec2 rel_pos = core.cursor_pos - origin;


    std::string prev = window_state[active_window].select_widget;

    int line = floor(-rel_pos.y / (f.line_height * float(text_scale)));

    if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
        if(capture_window == active_window) {
            if(line >= 0 && line < text_lines) {
                window_state[active_window].clear_selection = false;
            }
        } else {
            window_state[active_window].clear_selection = false;
        }
    }
    
    if(capture_widget == current_widget && (core.cursor_pos.x > get_space().z || core.cursor_pos.x < get_space().x) && line != clamp(line, 0, int(text_lines - 1))) {
        if(line < 0) window_state[active_window].select_position = 0;
        else window_state[active_window].select_position = text.size();
    } else if(window_state[active_window].select_widget == current_widget && core.key_map[GLFW_MOUSE_BUTTON_LEFT] && capture_widget == current_widget) line = clamp(line, 0, int(text_lines - 1));
    
    if(line >= 0 && line < text_lines) {
        rel_pos.x -= text_line_origins[line];
        uint32_t start_index = text_line_indices[line];
        uint32_t end_index;
        if(text_line_indices.size() <= line + 1) end_index = text.size();
        else end_index = text_line_indices[line + 1];
        
        uint32_t pixel = 0;
        int index = 0;
        
        if(text.size()) {
            std::string str(text.begin() + start_index, text.begin() + end_index);

            bool bold = false;
            bool italic = false;
            bool hex = false;

            for(int i = 0; i < start_index; ++i) {
                char c = text[i];

                if(c == '\\') {
                    if(i + 1 < text.size()) {
                        char next = text[i + 1];

                        if(next == 'c') {
                            if(i + 1 + 3 < text.size()) {
                                std::string s(text.begin() + (i + 2), text.begin() + (i + 5));

                                std::size_t i0 = integers_letters.find(s[0]);
                                std::size_t i1 = integers_letters.find(s[1]);
                                std::size_t i2 = integers_letters.find(s[2]);

                                if(i0 != std::string::npos && i1 != std::string::npos && i2 != std::string::npos) {
                                    if(!show_debug) {
                                        i += 4;
                                        continue;
                                    }
                                }
                            }
                        } else if(next == 'b') {
                            bold = true;

                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'i') {
                            italic = true;

                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'r') {
                            bold = false;
                            italic = false;

                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'h') {
                            hex = !hex;

                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        }
                    }
                } 
            }

            int i = 0;
            for(; i < str.size(); ++i) {
                char c = str[i];

                if(c == '\\') {
                    if(i + 1 < str.size()) {
                        char next = str[i + 1];

                        if(next == 'c') {
                            if(i + 1 + 3 < str.size()) {
                                std::string s(str.begin() + (i + 2), str.begin() + (i + 5));

                                std::size_t i0 = integers_letters.find(s[0]);
                                std::size_t i1 = integers_letters.find(s[1]);
                                std::size_t i2 = integers_letters.find(s[2]);

                                if(i0 != std::string::npos && i1 != std::string::npos && i2 != std::string::npos) {
                                    if(!show_debug) {
                                        i += 4;
                                        continue;
                                    }
                                }
                            }
                        } else if(next == 'b') {
                            bold = true;

                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'i') {
                            italic = true;

                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'r') {
                            bold = false;
                            italic = false;

                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'h') {
                            hex = !hex;

                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        }
                    }
                } 

                uint32_t new_pixel = pixel;

                if(hex) {
                    if(c == 'A') c = '\x80';
                    else if(c == 'B') c = '\x81';
                    else if(c == 'C') c = '\x82';
                    else if(c == 'D') c = '\x83';
                    else if(c == 'E') c = '\x84';
                    else if(c == 'F') c = '\x85';
                }

                if(c != '\n') {
                    Glyph_data& gd = f.at(c);

                    float stride = gd.stride;
                    if(bold && gd.visible) stride += bold_factor;

                    new_pixel = pixel + stride * text_scale;
                }

                if(new_pixel > rel_pos.x) {
                    float delta_new = abs(rel_pos.x - float(new_pixel));
                    float delta_old = abs(rel_pos.x - float(pixel));

                    if(delta_new > delta_old) index = i;
                    else {
                        index = i + 1;
                        pixel = new_pixel;
                    }
                    break;
                } 

                if(c == '\n') {
                    index = i;
                    break;
                }

                index = i + 1;

                pixel = new_pixel;
            }
        }
            
        if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            capture_widget = current_widget;

            if(!core.key_map[GLFW_KEY_LEFT_SHIFT]) {
                window_state[active_window].select_anchor = int(start_index) + index;
            }

            if(!(window_state[active_window].select_widget == current_widget)) {
                window_state[active_window].select_anchor = int(start_index) + index;
                window_state[active_window].select_position = int(start_index) + index;
                window_state[active_window].select_position_line = line;
            }
            
            window_state[active_window].select_widget = current_widget;
        }

        if(core.key_map[GLFW_MOUSE_BUTTON_LEFT] && capture_widget == current_widget) {
            window_state[active_window].select_widget = current_widget;
            window_state[active_window].select_position = int(start_index) + index;
            window_state[active_window].select_position_line = line;

            capture = 5;
            capture_widget = current_widget;
        }

        if(capture_widget == current_widget || capture_widget == "") cursor_mode = CURSOR_TEXT;
    }
}

vec2 GUI_system::get_text_cursor_pos(std::string text, vec2 origin, bool show_debug) {
    Font& f = fonts["default mono"];

    int32_t start_pos = 0;
    int32_t line = 0;
    for(int i = 0; i < text_lines; ++i) {
        if(window_state[active_window].select_position < int(text_line_indices[i]) || (window_state[active_window].select_position == text_line_indices[i] && window_state[active_window].select_position_line == i - 1)) break;

        start_pos = text_line_indices[i];
        line = i;
    }

    int32_t pixel = 0;

    bool bold = false;
    bool italic = false;
    bool hex = false;

    for(int i = 0; i < start_pos; ++i) {
        char c = text[i];

        if(c == '\\') {
            if(i + 1 < text.size()) {
                char next = text[i + 1];

                if(next == 'c') {
                    if(i + 1 + 3 < text.size()) {
                        std::string s(text.begin() + (i + 2), text.begin() + (i + 5));

                        std::size_t i0 = integers_letters.find(s[0]);
                        std::size_t i1 = integers_letters.find(s[1]);
                        std::size_t i2 = integers_letters.find(s[2]);

                        if(i0 != std::string::npos && i1 != std::string::npos && i2 != std::string::npos) {
                            if(!show_debug) {
                                i += 4;
                                continue;
                            }
                        }
                    }
                } else if(next == 'b') {
                    bold = true;

                    if(!show_debug) {
                        i += 1;
                        continue;
                    }
                } else if(next == 'i') {
                    italic = true;

                    
                    if(!show_debug) {
                        i += 1;
                        continue;
                    }
                } else if(next == 'r') {
                    bold = false;
                    italic = false;

                    if(!show_debug) {
                        i += 1;
                        continue;
                    }
                } else if(next == 'h') {
                    hex = !hex;

                    if(!show_debug) {
                        i += 1;
                        continue;
                    }
                }
            }
        } 
    }

    for(int i = start_pos; i < window_state[active_window].select_position; ++i) {
        char c = text[i];

        if(c == '\\') {
            if(i + 1 < text.size()) {
                char next = text[i + 1];

                if(next == 'c') {
                    if(i + 1 + 3 < text.size()) {
                        std::string s(text.begin() + (i + 2), text.begin() + (i + 5));

                        std::size_t i0 = integers_letters.find(s[0]);
                        std::size_t i1 = integers_letters.find(s[1]);
                        std::size_t i2 = integers_letters.find(s[2]);

                        if(i0 != std::string::npos && i1 != std::string::npos && i2 != std::string::npos) {
                            if(!show_debug) {
                                i += 4;
                                continue;
                            }
                        }
                    }
                } else if(next == 'b') {
                    bold = true;
                    
                    if(!show_debug) {
                        i += 1;
                        continue;
                    }
                } else if(next == 'i') {
                    italic = true;
                    
                    if(!show_debug) {
                        i += 1;
                        continue;
                    }
                } else if(next == 'r') {
                    bold = false;
                    italic = false;

                    if(!show_debug) {
                        i += 1;
                        continue;
                    }
                } else if(next == 'h') {
                    hex = !hex;

                    if(!show_debug) {
                        i += 1;
                        continue;
                    }
                }
            }
        } 

        if(hex) {
            if(c == 'A') c = '\x80';
            else if(c == 'B') c = '\x81';
            else if(c == 'C') c = '\x82';
            else if(c == 'D') c = '\x83';
            else if(c == 'E') c = '\x84';
            else if(c == 'F') c = '\x85';
        }

        if(c != '\n') {
            Glyph_data& gd = f.at(c);

            float stride = gd.stride;
            if(bold && gd.visible) stride += bold_factor;

            pixel = pixel + stride * text_scale;
        }
    }

    float yy = (line + 1) * float(f.line_height) * float(text_scale);
    float yyy = line * float(f.line_height) * float(text_scale);

    ivec2 pix = ivec2(origin.x + pixel + text_line_origins[line], origin.y - yy);

    return pix;
}

void GUI_system::text_navigate(std::string& text, bool edit, bool show_debug) {
    // navigation
    ivec2 range = ivec2(min(window_state[active_window].select_position, window_state[active_window].select_anchor), max(window_state[active_window].select_position, window_state[active_window].select_anchor));
    
    // get line positions
    int32_t start_pos = 0;
    int32_t line = 0;
    for(int i = 0; i < text_lines; ++i) {
        if(window_state[active_window].select_position < int(text_line_indices[i]) || (window_state[active_window].select_position == text_line_indices[i] && window_state[active_window].select_position_line == i - 1)) break;

        start_pos = text_line_indices[i];
        line = i;
    }


    if(core.pressed_buttons.contains(GLFW_KEY_LEFT) || core.repeat_buttons.contains(GLFW_KEY_LEFT)) {
        int end = text_line_indices[line] - 1;

        if(core.key_map[GLFW_KEY_LEFT_SHIFT]) {
            int r_pos = window_state[active_window].select_position;

            if(window_state[active_window].select_position_line != line) {
                --r_pos;

                while(true) {
                    char c = text[r_pos - 1];
                    if(c == '\n') {
                        --r_pos;
                    } else break;
                }
            } else if(window_state[active_window].select_position == window_state[active_window].select_anchor) {
                while(true) {
                    char c = text[r_pos - 1];
                    if(c == '\n') {
                        --r_pos;
                    } else break;
                }
                
                --r_pos;
            } else --r_pos;

            window_state[active_window].select_position = r_pos;
        } else {
            if(range.x == range.y) {
                if(range.y == end + 1 && window_state[active_window].select_position_line != line - 1) {
                    while(true) {
                        char c = text[range.y - 1];
                        if(c == '\n') {
                            --range.y;
                            --range.x;
                        } else break;
                    }

                    window_state[active_window].select_position_line = line - 1;
                    window_state[active_window].select_position = range.y;
                    window_state[active_window].select_anchor = range.y;
                } else {
                    window_state[active_window].select_position_line = line;
                    window_state[active_window].select_position = range.x - 1;
                    window_state[active_window].select_anchor = range.x - 1;
                }
            } else {
                window_state[active_window].select_position = range.x;
                window_state[active_window].select_anchor = range.x;
            }
        }
    }

    if(core.pressed_buttons.contains(GLFW_KEY_RIGHT) || core.repeat_buttons.contains(GLFW_KEY_RIGHT)) {
        int start = text_line_indices[line];
        int end;
        if(text_line_indices.size() - 1 > line) {
            end = text_line_indices[line + 1];
        } else end = text.size();
        
        if(core.key_map[GLFW_KEY_LEFT_SHIFT]) {
            int r_pos = window_state[active_window].select_position;
        
            if(window_state[active_window].select_position_line != line) {
                ++r_pos;

                while(true) {
                    char c = text[r_pos];
                    if(c == '\n') {
                        ++r_pos;
                    } else break;
                }
            } else if(window_state[active_window].select_position == window_state[active_window].select_anchor) {
                while(true) {
                    char c = text[r_pos];
                    if(c == '\n') {
                        ++r_pos;
                    } else break;
                }

                ++r_pos;
            } else ++r_pos;

            window_state[active_window].select_position = r_pos;
        } else {
            while(true) {
                char c = text[range.y];
                if(c == '\n') {
                    ++range.y;
                    ++range.x;
                } else break;
            }
            
            if(range.x == range.y) {
                if(range.y == end && window_state[active_window].select_position_line != line + 1) {
                    window_state[active_window].select_position_line = line + 1;

                    window_state[active_window].select_position = range.y;
                    window_state[active_window].select_anchor = range.y;
                } else {
                    window_state[active_window].select_position_line = line;

                    window_state[active_window].select_position = range.y + 1;
                    window_state[active_window].select_anchor = range.y + 1;
                }
            } else {
                window_state[active_window].select_position = range.y;
                window_state[active_window].select_anchor = range.y;
            }
        }
    }

    if(core.pressed_buttons.contains(GLFW_KEY_UP) || core.repeat_buttons.contains(GLFW_KEY_UP)) {
        int new_line = line - 1;
        int offset = window_state[active_window].select_position - start_pos;

        if(new_line >= 0) {
            int max_v;
            if(text_line_indices.size() - 1 > new_line) {
                max_v = text_line_indices[new_line + 1];
            } else max_v = text.size();

            window_state[active_window].select_position = min(max_v, int(text_line_indices[new_line] + offset));
            if(!core.key_map[GLFW_KEY_LEFT_SHIFT]) window_state[active_window].select_anchor = min(max_v, int(text_line_indices[new_line] + offset));
            window_state[active_window].select_position_line = new_line;
        }
    }

    if(core.pressed_buttons.contains(GLFW_KEY_DOWN) || core.repeat_buttons.contains(GLFW_KEY_DOWN)) {
        int new_line = line + 1;
        int offset = window_state[active_window].select_position - start_pos;

        if(text_line_indices.size() > new_line) {
            int max_v;
            if(text_line_indices.size() - 1 > new_line) {
                max_v = text_line_indices[new_line + 1];
            } else max_v = text.size();

            window_state[active_window].select_position = min(max_v, int(text_line_indices[new_line] + offset));
            if(!core.key_map[GLFW_KEY_LEFT_SHIFT]) window_state[active_window].select_anchor = min(max_v, int(text_line_indices[new_line] + offset));
            window_state[active_window].select_position_line = new_line;
        }
    }

    window_state[active_window].select_position = clamp(window_state[active_window].select_position, 0, (int)text.size());
    window_state[active_window].select_anchor = clamp(window_state[active_window].select_anchor, 0, (int)text.size());
    
    if(edit) {
        if(core.pressed_buttons.contains(GLFW_KEY_BACKSPACE) || core.repeat_buttons.contains(GLFW_KEY_BACKSPACE)) {
            if(range.x == range.y) {
                if(range.x != 0) text.erase(text.begin() + range.x - 1, text.begin() + range.x);
                
                window_state[active_window].select_position = max(0, range.x - 1);
                window_state[active_window].select_anchor = max(0, range.x - 1);
            } else {
                text.erase(text.begin() + range.x, text.begin() + range.y);
                
                window_state[active_window].select_position = range.x;
                window_state[active_window].select_anchor = range.x;
            }
        }
        
        if(core.char_delta.size()) {
            text.erase(text.begin() + range.x, text.begin() + range.y);
            text.insert(text.begin() + range.x, core.char_delta.begin(), core.char_delta.end());

            window_state[active_window].select_position = range.x + core.char_delta.size();
            window_state[active_window].select_anchor = range.x + core.char_delta.size();
        }

        if(core.key_map[GLFW_KEY_LEFT_SHIFT] && (core.pressed_buttons.contains(GLFW_KEY_ENTER) || core.repeat_buttons.contains(GLFW_KEY_ENTER))) {
            text.erase(text.begin() + range.x, text.begin() + range.y);
            text.insert(text.begin() + range.x, '\n');

            window_state[active_window].select_position = range.x + 1;
            window_state[active_window].select_anchor = range.x + 1;
        }

        if(core.pressed_buttons.contains(GLFW_KEY_V) && core.key_map[GLFW_KEY_LEFT_CONTROL]) {
            std::string paste = paste_from_clipboard();

            text.erase(text.begin() + range.x, text.begin() + range.y);
            text.insert(text.begin() + range.x, paste.begin(), paste.end());

            window_state[active_window].select_position = range.x + paste.size();
            window_state[active_window].select_anchor = range.x + paste.size();
        }
    }

    // copy

    if(window_state[active_window].select_position != window_state[active_window].select_anchor) {
        if(core.pressed_buttons.contains(GLFW_KEY_C) && core.key_map[GLFW_KEY_LEFT_CONTROL]) {
            uint32_t start = min(window_state[active_window].select_position, window_state[active_window].select_anchor);
            uint32_t end = max(window_state[active_window].select_position, window_state[active_window].select_anchor);
            std::string str(text.begin() + start, text.begin() + end);

            std::string new_str;
            for(int i = 0; i < str.size(); ++i) {
                char c = str[i];

                if(c == '\\') {
                    if(i + 1 < str.size()) {
                        char next = str[i + 1];

                        if(next == 'c') {
                            if(i + 1 + 3 < str.size()) {
                                std::string s(str.begin() + (i + 2), str.begin() + (i + 5));

                                std::size_t i0 = integers_letters.find(s[0]);
                                std::size_t i1 = integers_letters.find(s[1]);
                                std::size_t i2 = integers_letters.find(s[2]);

                                if(i0 != std::string::npos && i1 != std::string::npos && i2 != std::string::npos) {
                                    if(!show_debug) {
                                        i += 4;
                                        continue;
                                    }
                                }
                            }
                        } else if(next == 'b') {
                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'i') {
                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        } else if(next == 'r') {
                            if(!show_debug) {
                                i += 1;
                                continue;
                            }
                        }
                    }
                }

                new_str += c;
            }

            str = new_str;
            
            for(char& c : str) {
                if(c == '\x80') c = 'A';
                else if(c == '\x81') c = 'B';
                else if(c == '\x82') c = 'C';
                else if(c == '\x83') c = 'D';
                else if(c == '\x84') c = 'E';
                else if(c == '\x85') c = 'F';
            }

            copy_to_clipboard(str);
        }
    }
}