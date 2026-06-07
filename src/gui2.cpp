#include "gui.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "render.hpp"
#include "core.hpp"

int italic_factor = 2;
uint32_t base = 16;
int header = 15;
int buffer = 4;

std::string integers = "0123456789\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89";  
std::string integers_letters = "0123456789ABCDEF";

bool includes(ivec2 point, ivec4 range) {
    return (point.x > range.x && point.x < range.z && point.y > range.y && point.y < range.w);
}

ivec4 clip(ivec4 range_a, ivec4 range_b) {
    ivec4 range_c = ivec4(glm::max(range_a.x, range_b.x), glm::max(range_a.y, range_b.y), glm::min(range_a.z, range_b.z), glm::min(range_a.w, range_b.w));

    return range_c;
}

void GUI_system::insert_window(std::string label, Window_state state) {
    state.priority = 0;
    for(auto& [name, ws] : window_state) {
        if(name != "") ++ws.priority;
    }
    window_state.emplace(label, state);
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

    STARTCHAR char0
    ENCODING 0
    SWIDTH 500 0
    DWIDTH 6 0
    BBX 6 12 0 -2
    BITMAP
    00
    00
    D8
    88
    00
    88
    88
    00
    88
    D8
    00
    00
    ENDCHAR

    capture_window = min_name;
}

/*
std::string message_callback() {
    double elapsed_time = core.prev_time - core.start_time;
    return to_base(elapsed_time, 16, 4);
}

std::string position_callback() {
    Render_system& r = ecs.get_system<Render_system>();

    uint32_t camera = *r.collectors[1].entities.begin();

    Transform& t = ecs.get_component<Transform>(camera);

    vec3 facing = -t.orientation[2];

    std::string fx = "";
    std::string fy = "";
    std::string fz = "";
    std::string reset = "\\r\\cFFF";

    if(abs(facing.x) > abs(facing.y) && abs(facing.x) > abs(facing.z)) {
        if(facing.x > 0.0f) fx = "\\b\\cF44";
        else fx = "\\b\\c4DD";
    } else if(abs(facing.y) > abs(facing.x) && abs(facing.y) > abs(facing.z)) {
        if(facing.y > 0.0f) fy = "\\b\\c4F4";
        else fy = "\\b\\cD4D";
    } else if(abs(facing.z) > abs(facing.x) && abs(facing.z) > abs(facing.y)) {
        if(facing.z > 0.0f) fz = "\\b\\c44F";
        else fz = "\\b\\cDD4";
    }

    return "\\bX: \\r" + to_base(t.position.x.sector, base) + " " + to_base(t.position.x.fraction, base, 4)
    + "\n\\bY: \\r" + to_base(t.position.y.sector, base) + " " + to_base(t.position.y.fraction, base, 4)
    + "\n\\bZ: \\r"  + to_base(t.position.z.sector, base) + " " + to_base(t.position.z.fraction, base, 4)
    + "\n\\bFacing: \\r" + fx + to_base(facing.x, base, 3) + reset + " " + fy + to_base(facing.y, base, 3) + reset + " " + fz + to_base(facing.z, base, 3) + reset;
}

std::string physics_callback() {
    Physics_system& physics_system = ecs.get_system<Physics_system>();

    uint32_t constraints = 0;

    for(auto& [k, d] : physics_system.collision_table) {
        constraints += d.size();
    }
    constraints += physics_system.constraints.size();

    return "\\bColliders: \\r" + to_base(int64_t(physics_system.collectors[0].entities.size()), base) + "\n\\b""Constraints: \\r" + to_base(int32_t(constraints), base) + "\n\\b""Gravity: \\r" + to_base(physics_system.gravity, base, 3);
}

std::string mode_callback() {
    uint32_t camera = *core.collectors[0].entities.begin();
    Interface& interface = ecs.get_component<Interface>(camera);

    return "Mode: " + to_base(interface.mode, base);
}
*/

uint32_t frames = 0;
double time_fps = 0;
double fps = 0;
std::string fps_callback() {
    ++frames;
    time_fps += core.delta_time;
    if(time_fps > 1) {
        fps = double(frames) / time_fps;
        time_fps = 0;
        frames = 0;
    }
    return "FPS: " + std::to_string(fps);
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

std::vector<UI_vertex> GUI_system::mesh_text(Font& f, std::string text, uint32_t width, ivec2 select_range, Alignment alignment) {
    GUI_system& gui_system = ecs.get_system<GUI_system>();
    gui_system.text_line_indices.clear();
    gui_system.text_line_origins.clear();

    uint32_t line_start_index = 0;
    uint32_t word_start_index = 0;

    bool accept_index = false;

    int i = 0;

    std::vector<UI_vertex> ret;
    vec2 pos = vec2(0.0f);
    vec4 range = vec4(0.0f);

    vec4 color = vec4(1.0f);
    bool bold = false;
    bool italic = false;

    float italic_factor = 1.0f / 3.5f;
    float bold_factor = 1.0f;

    uint32_t num_lines = 0;

    std::vector<UI_vertex> word_ret;
    vec2 word_pos = vec2(0.0f);
    
    std::vector<UI_vertex> line_ret;

    auto insert_line = [&]() {
        int line_width = pos.x;
        int offset;

        if(alignment == ALIGN_LEFT) offset = widget_sep;
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
        if(word_ret.size() == 0) word_start_index = i;

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

        if(!gd.visible) {
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
            if(word_ret.size() == 0) word_start_index = i;

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
            
            if(i >= select_range.x && i < select_range.y) insert_selection(word_pos, {gd.stride, f.line_height});

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

                                i += 4;
                                continue;
                            }
                        }
                    } else if(next == 'b') {
                        bold = true;

                        i += 1;
                        continue;
                    } else if(next == 'i') {
                        italic = true;

                        i += 1;
                        continue;
                    } else if(next == 'r') {
                        bold = false;
                        italic = false;

                        i += 1;
                        continue;
                    }
                }
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
    
    static bool open_settings = false;
    static bool open_chat = false;
    static bool open_stats = false;
    static int num_links = 0;
    static float object_scale = 1.0f;

    Physics_system& ps = ecs.get_system<Physics_system>();

    int width = core.window.viewport_size.x;
    int height = core.window.viewport_size.y;

    if(open_settings) {
        toggle_button({0.0f, height + (-16 * icon_scale - 8 * icon_scale)}, {24 * icon_scale, 16 * icon_scale}, {54, 54, 64, 64}, open_settings);
    } else {
        toggle_button({0.0f, height + (-16 * icon_scale - 8 * icon_scale)}, {16 * icon_scale, 16 * icon_scale}, {54, 54, 64, 64}, open_settings);
        if(open_settings) {
            Window_state ws;
            ws.size = {384.0f, 384.0f};
            ws.position = {width * 0.5f - ws.size.x * 0.5f, height * 0.5f + ws.size.y * 0.5f};
            ws.label = "settings";

            insert_window("settings_window", ws);
        }
    }

    if(open_stats) {
        toggle_button({0.0f, height + (-16 * icon_scale - 8 * icon_scale) * 2.0f}, {24 * icon_scale, 16 * icon_scale}, {54, 34, 64, 44}, open_stats);
    } else {
        toggle_button({0.0f, height + (-16 * icon_scale - 8 * icon_scale) * 2.0f}, {16 * icon_scale, 16 * icon_scale}, {54, 34, 64, 44}, open_stats);
        if(open_stats) {
            Window_state ws;
            ws.size = {384.0f, 384.0f};
            ws.position = {width * 0.5f - ws.size.x * 0.5f, height * 0.5f + ws.size.y * 0.5f};
            ws.label = "statistics";

            insert_window("stats_window", ws);
        }
    }

    if(open_settings) {
        bool close_window = false;
        window("settings_window", close_window);

        if(close_window) {
            open_settings = false;
        }

        //slider("num_links_slider", "number of links: " + to_base(core.num_links, 16), ivec2(0, 16), core.num_links, vec2(150, 15), 10);
        //slider("scale_slider", "object scale: " + to_base(core.object_scale, 16, 3), vec2(0.0625, 16), core.object_scale, vec2(150, 15), 10);

        /*
        std::string str;
        map_mode next;
        if(es.mode == MAP_MODE_ELEVATION) {
            next = MAP_MODE_SHADE;
            str = "Map Mode: \\b\\cF44ELEVATION";
        } else if(es.mode == MAP_MODE_SHADE) {
            next = MAP_MODE_FLOW;
            str = "Map Mode: \\b\\cF4FSHADE";
        } else if(es.mode == MAP_MODE_FLOW) {
            next = MAP_MODE_BASIN;
            str = "Map Mode: \\b\\c44FFLOW";
        } else if(es.mode == MAP_MODE_BASIN) {
            next = MAP_MODE_ELEVATION;
            str = "Map Mode: \\b\\c4F4BASINS";
        }

        alignment = ALIGN_CENTER;
        static bool toggle_map_mode;
        bool prev = toggle_map_mode;
        button("map_mode", str, vec2(300, 25), toggle_map_mode);
        if(!prev && toggle_map_mode) {
            es.mode = next;
            es.update_texture();
        }
        if(toggle_map_mode && !core.key_map[GLFW_MOUSE_BUTTON_LEFT]) toggle_map_mode = false;
        */

        static int number = 0x7F;
        slider("hex_slider", to_base(number, 16), ivec2(0, 0xFF), number, vec2(300, 16), 6);

        std::string lorem_ipsum_string = 
        R"(Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec vulputate lacinia lectus, pulvinar porta sapien consectetur non. Nunc ligula est, vulputate quis vestibulum id, ornare vitae leo. Etiam hendrerit libero in quam venenatis, sit amet lacinia lectus tincidunt. Vestibulum dictum, odio sit amet egestas placerat, velit odio faucibus nisi, eu congue nunc ligula a mauris. Fusce et velit molestie nibh fermentum luctus. Cras ac rutrum mi, molestie vulputate justo. Quisque ut sem in dolor malesuada varius eu sit amet nulla. Nulla blandit lorem eu ligula porta sagittis. Vivamus auctor justo eget dui accumsan convallis. Donec bibendum justo ac mi viverra rhoncus. Nulla mollis vel sem non suscipit. Sed pellentesque odio ut quam ultrices, vel malesuada lectus pharetra. Donec elementum lobortis sollicitudin. Aliquam molestie tellus eget mi iaculis imperdiet. Etiam in lacus dolor.

Etiam ut neque nisl. Aenean dapibus imperdiet convallis. Etiam quam massa, aliquet at tortor vel, lacinia ultrices justo. Ut tortor diam, eleifend vel quam vel, accumsan malesuada metus. Vivamus dui odio, porta at dolor in, interdum porttitor diam. Curabitur sit amet mi a velit aliquet tincidunt. Suspendisse eu massa nibh. Fusce quam est, lacinia eget tellus id, aliquam scelerisque tortor. Nunc congue lacus in odio aliquam, id sagittis augue condimentum. Donec suscipit ac dui quis finibus. Vivamus lobortis, elit sit amet iaculis porttitor, velit ante pulvinar mauris, eget ultrices sapien mi vel neque. In viverra ex in posuere auctor. Curabitur pretium metus eros, vel sodales eros placerat efficitur. Curabitur nisl lorem, iaculis sed elit tristique, efficitur efficitur tortor. Vivamus eu tellus eros. Maecenas ut mauris ante.

Vivamus vestibulum vehicula mi, eu accumsan justo vestibulum non. Vestibulum dignissim leo est, a mattis orci vehicula malesuada. Nunc eros nunc, pretium in orci ut, tempus ultrices enim. Duis efficitur maximus venenatis. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Sed mi lacus, venenatis in augue at, tempus aliquet urna. Aliquam condimentum aliquam gravida. Phasellus posuere ipsum sed felis mattis sollicitudin. Aenean pretium leo et maximus laoreet. In tempor urna quis mauris lobortis, nec aliquet tortor tristique. Morbi urna urna, auctor non sagittis vel, imperdiet dapibus diam. Nunc placerat diam lectus. Ut consequat eget sem a vehicula.

Proin vel commodo neque. Duis leo purus, egestas vitae pulvinar quis, ultricies nec ex. Donec aliquam purus eget dictum egestas. Proin pharetra nunc dui, et luctus urna dictum auctor. Nam id iaculis nulla. Aenean posuere, enim vitae venenatis finibus, lorem dolor porta leo, eu luctus ipsum nisi non enim. Fusce consectetur mauris placerat dui finibus, at mattis nisi hendrerit. Suspendisse eleifend ex quis lobortis convallis. Morbi commodo ut felis non ornare. Donec dignissim est sodales rutrum lobortis. Integer blandit, nibh luctus sodales porttitor, tellus erat eleifend orci, sit amet vulputate justo metus ac orci. Maecenas mi quam, interdum in condimentum vitae, accumsan in ante. Suspendisse vel justo sapien.

Phasellus ac felis ut velit tempor cursus. Phasellus at leo semper, mattis sem ac, blandit lorem. Pellentesque auctor tellus vel lacus congue tempus. Pellentesque pulvinar tellus et varius varius. Nullam egestas, libero ut varius tristique, nisi neque ullamcorper nisl, id varius dui massa a lorem. Nullam tincidunt leo at velit molestie ullamcorper. Morbi nec tellus laoreet, dictum orci et, varius velit. Vivamus vehicula varius sem, id posuere nisi ullamcorper in. Donec sit amet efficitur dolor, vitae aliquam lorem.

Suspendisse vitae laoreet elit, in sodales risus. Mauris suscipit, nibh ut hendrerit condimentum, quam diam venenatis justo, nec ultricies nibh nulla non dolor. Nam blandit, odio ac mollis mollis, est dui mollis neque, eu scelerisque sem urna in sapien. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis laoreet.)";

        alignment = ALIGN_LEFT;
        text("lorem_ipsum_dolor", lorem_ipsum_string);
    }

    static std::string frame_string = "";

    static float elapsed_time = 0.0f;
    static uint32_t frames = 0;

    elapsed_time += core.delta_time;
    ++frames;

    if(elapsed_time > 1.0f) {
        float fps = float(frames) / elapsed_time;
        frame_string = "FPS: " + to_base(fps, 10, 3);
        elapsed_time = 0.0f;
        frames = 0;
    }

    if(open_stats) {
        bool close_window = false;
        window("stats_window", close_window);
        
        if(close_window) {
            open_stats = false;
        }
        
        alignment = ALIGN_LEFT;
        text("frame_text", frame_string);

        Input_system& input_system = ecs.get_system<Input_system>();
        vec2 pos = input_system.world_cursor_pos;

        std::string elev_text = to_base(pos.x, 16, 3) + " " + to_base(pos.y, 16, 3);

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

    ivec4 scrollbar_range;

    int y_size = (ws.current_pos.y - widget_sep) - (position.y - header);
    int height = abs(y_size - ws.scroll_pos);

    ws.scroll_pos = clamp(ws.scroll_pos, 0, max(0, height - int(ws.size.y)));

    if(height > ws.size.y) {
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
        position.x += core.cursor_delta.x;
        size.x -= core.cursor_delta.x;

        if(core.cursor_pos.x > position.x) {
            float delta_max = (core.cursor_pos.x) - position.x;
            position.x += delta_max;
            size.x -= delta_max;
        }

        delta_min = max(0.0f, min_size.x - size.x);
        size.x += delta_min;
        position.x -= delta_min;
    };

    auto resize_right = [&]() {
        float delta_min;

        size.x += core.cursor_delta.x;

        if(core.cursor_pos.x < position.x + size.x) {
            float delta_max = (position.x + size.x) - (core.cursor_pos.x);
            size.x -= delta_max;
        }

        delta_min = max(0.0f, min_size.x - size.x);
        size.x += delta_min;
    };

    auto resize_top = [&]() {
        float delta_min;
        position.y += core.cursor_delta.y;
        size.y += core.cursor_delta.y;

        if(core.cursor_pos.y < position.y) {
            float delta_max = (core.cursor_pos.y) - position.y;
            position.y += delta_max;
            size.y += delta_max;
        }

        delta_min = max(0.0f, min_size.y - size.y);
        size.y += delta_min;
        position.y += delta_min;  
    };

    auto resize_bottom = [&]() {
        float delta_min;

        size.y -= core.cursor_delta.y;

        if(core.cursor_pos.y > position.y - size.y - header) {
            float delta_max = (position.y - size.y - header) - (core.cursor_pos.y);
            size.y += delta_max;
        }

        delta_min = max(0.0f, min_size.y - size.y);
        size.y += delta_min;
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
        scrollbar_height = ws.size.y * (float(ws.size.y) / height);
        float ratio = 1.0f - ws.scroll_pos / (height - ws.size.y);
        int32_t r = size.y - scrollbar_height;
        float rrr = ratio;
        scrollbar_range = {position + vec2(size.x - ws.scrollbar_width, -size.y - header + r * rrr), position + vec2(size.x, -size.y - header + scrollbar_height + r * rrr)};
    

        ret = {a, b, d, a, d, c};
        range = {position + vec2(size.x - ws.scrollbar_width, -size.y - header), position + vec2(size.x, -header)};
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
    ret = {a, b, d, a, d, c};
    ret[0].color.w = 0.0f;
    ret[2].color.w = 0.0f;
    ret[3].color.w = 0.0f;
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
    ret = {a, b, d, a, d, c};
    ret[0].color.w = 0.0f;
    ret[1].color.w = 0.0f;
    ret[2].color.w = 0.0f;
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

    ws.current_pos = position + vec2(0.0f, -header + ws.scroll_pos) + vec2(0.0f, -widget_sep);

    ws.space = {position + vec2(0.0f, -header - size.y), position + vec2(size.x, -header)};
    if(scrollbar) ws.space.z -= ws.scrollbar_width;
    
    current_range = ws.space;
}

void GUI_system::text(std::string name, std::string text, uint32_t width) {
    if(width = 0xFFFFFFFF) {
        width = (window_state[active_window].space.z - window_state[active_window].space.x) - widget_sep * 2;
    }

    Font& f = fonts["default mono"];

    ivec2 select_range = {-1, -1};

    if(window_state[active_window].select_widget == name) select_range = {min(window_state[active_window].select_position, window_state[active_window].select_anchor), max(window_state[active_window].select_position, window_state[active_window].select_anchor)};

    std::vector<UI_vertex> ret = mesh_text(f, text, width, select_range, alignment);
    
    int boundary = (window_state[active_window].space.z - window_state[active_window].space.x) - width;

    vec2 origin = window_state[active_window].current_pos + vec2(boundary * 0.5f, 0);

    for(UI_vertex& v : ret) {
        v.pos = origin + v.pos * float(text_scale) + vec2(0.0f, -f.line_height * float(text_scale));
        v.range = current_range;
    }
    
    window_state[active_window].current_pos.y -= float(text_lines * text_scale * f.line_height);

    // selection

    if((active_window == capture_window && includes(core.cursor_pos, window_state[active_window].space)) || (capture_widget == name && core.key_map[GLFW_MOUSE_BUTTON_LEFT])) {
        vec2 rel_pos = core.cursor_pos - origin;

        int line = floor(-rel_pos.y / (f.line_height * float(text_scale)));

        if((line < 0 || line >= text_lines) && core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
            window_state[active_window].select_position = -1;
            window_state[active_window].select_anchor = -1;
            window_state[active_window].select_widget = "";
        }
        
        if(capture_widget == name && (core.cursor_pos.x > window_state[active_window].space.z || core.cursor_pos.x < window_state[active_window].space.x) && line != clamp(line, 0, int(text_lines - 1))) {
            if(line < 0) window_state[active_window].select_position = 0;
            else window_state[active_window].select_position = text.size();
        } else if(window_state[active_window].select_widget == name && core.key_map[GLFW_MOUSE_BUTTON_LEFT] && capture_widget == name) line = clamp(line, 0, int(text_lines - 1));
        

        if(line >= 0 && line < text_lines) {
            rel_pos.x -= text_line_origins[line];
            uint32_t start_index = text_line_indices[line];
            uint32_t end_index;
            if(text_line_indices.size() <= line + 1) end_index = text.size();
            else end_index = text_line_indices[line + 1];

            std::string str(text.begin() + start_index, text.begin() + end_index);

            uint32_t pixel = 0;
            int index;

            bool bold = false;
            bool italic = false;
            for(int i = 0; i < str.size(); ++i) {
                char c = str[i];

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
                                    i += 4;
                                    continue;
                                }
                            }
                        } else if(next == 'b') {
                            bold = true;

                            i += 1;
                            continue;
                        } else if(next == 'i') {
                            italic = true;

                            i += 1;
                            continue;
                        } else if(next == 'r') {
                            bold = false;
                            italic = false;

                            i += 1;
                            continue;
                        }
                    }
                } 

                uint32_t new_pixel = pixel;

                if(c != '\n') {
                    Glyph_data& gd = f.at(c);

                    float stride = gd.stride;

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

                if(i == str.size() - 1) index = i + 1;
                pixel = new_pixel;
            }

            
            if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
                capture_widget = name;

                window_state[active_window].select_widget = name;
                window_state[active_window].select_anchor = int(start_index) + index;
            }

            if(core.key_map[GLFW_MOUSE_BUTTON_LEFT] && capture_widget == name) {
                window_state[active_window].select_widget = name;
                window_state[active_window].select_position = int(start_index) + index;
                window_state[active_window].select_position_line = line;

                capture = 5;
                capture_widget = name;
            }

            if(capture_widget == name || capture_widget == "") cursor_mode = CURSOR_TEXT;
        }
    }

    if(window_state[active_window].select_widget == name && window_state[active_window].select_position == window_state[active_window].select_anchor) {
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
                                i += 4;
                                continue;
                            }
                        }
                    } else if(next == 'b') {
                        bold = true;

                        i += 1;
                        continue;
                    } else if(next == 'i') {
                        italic = true;

                        i += 1;
                        continue;
                    } else if(next == 'r') {
                        bold = false;
                        italic = false;

                        i += 1;
                        continue;
                    }
                }
            } 

            if(c != '\n') {
                Glyph_data& gd = f.at(c);

                float stride = gd.stride;

                pixel = pixel + stride * text_scale;
            }
        }

        float yy = (line + 1) * float(f.line_height) * float(text_scale);
        float yyy = line * float(f.line_height) * float(text_scale);
        ivec4 range = ivec4(origin.x + pixel - 2 + text_line_origins[line], origin.y - yy, origin.x + pixel + 2 - 2 + text_line_origins[line], origin.y - yyy);

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

    insert_vertices(ret);
}

void GUI_system::slider(std::string name, std::string text, ivec2 bounds, int& value, vec2 size, float slider_width) {
    vec4 range;

    if(alignment == ALIGN_LEFT) {
        int x_pos = widget_sep;
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_CENTER) {
        int x_pos = ((current_range.z - current_range.x) * 0.5f - size.x * 0.5f);
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_RIGHT) {
        int x_pos = ((current_range.z - current_range.x) - size.x - widget_sep);
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
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

    window_state[active_window].current_pos.y -= size.y + widget_sep;
}


void GUI_system::slider(std::string name, std::string text, vec2 bounds, float& value, vec2 size, float slider_width) {
    vec4 range;

    if(alignment == ALIGN_LEFT) {
        int x_pos = widget_sep;
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_CENTER) {
        int x_pos = ((current_range.z - current_range.x) * 0.5f - size.x * 0.5f);
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_RIGHT) {
        int x_pos = ((current_range.z - current_range.x) - size.x - widget_sep);
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
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
    
    window_state[active_window].current_pos.y -= size.y + widget_sep;
}

void GUI_system::button(std::string name, std::string text, vec2 size, bool& active) {
    vec4 range;
    
    if(alignment == ALIGN_LEFT) {
        int x_pos = widget_sep;
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_CENTER) {
        int x_pos = ((current_range.z - current_range.x) * 0.5f - size.x * 0.5f);
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
    } else if(alignment == ALIGN_RIGHT) {
        int x_pos = ((current_range.z - current_range.x) - size.x - widget_sep);
        range = {window_state[active_window].current_pos + vec2(x_pos, -size.y), window_state[active_window].current_pos + vec2(x_pos + size.x, 0.0f)};
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
    
    window_state[active_window].current_pos.y -= size.y + widget_sep;
}