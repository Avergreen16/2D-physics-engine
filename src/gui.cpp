#include "gui.hpp"
#include "input.hpp"

int italic_factor = 2;

std::string integers = "0123456789\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89";  

std::string message_callback() {
    double elapsed_time = core.prev_time - core.start_time;
    return to_base(elapsed_time, 16, 4);
}
std::string null_callback() {
    return "";
}
void button_callback() {
    std::cout << "button pressed" << "\n";
}
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
    return std::to_string(fps);
}

void insert_char(std::vector<UI_vertex>& vertices, Font& font, char character, vec2 pos, int text_size, bool italic, bool bold, vec4 color) {
    std::array<glm::vec2, 4> offsets = {glm::vec2{0, 0}, glm::vec2{0, 0}, glm::vec2{0, 0}, glm::vec2{0, 0}};
    Glyph_data& g = font.glyph_map[character];

    if(italic) {
        float y0 = g.pos_line[1];
        float y1 = g.pos_line[1] + g.size[1];
        std::array<glm::vec2, 4> add_offsets = {
            glm::vec2{(y0 / 7) * italic_factor, 0},
            glm::vec2{(y0 / 7) * italic_factor, 0},
            glm::vec2{(y1 / 7) * italic_factor, 0},
            glm::vec2{(y1 / 7) * italic_factor, 0}
        };

        for(int i = 0; i < 4; ++i) {
            glm::vec2 v = add_offsets[i];
            offsets[i] += v;
        }
    }
    if(bold) {
        std::array<glm::vec2, 4> add_offsets = {
            glm::vec2{0, 0},
            glm::vec2{1.0, 1.0},
            glm::vec2{0, 0},
            glm::vec2{1.0, 1.0}
        };

        for(int i = 0; i < 4; ++i) {
            glm::vec2 v = add_offsets[i];
            offsets[i] += v;
        }
    }

    for(glm::vec2 o : offsets) {
        o.y = round(o.y * text_size) / text_size;
    }

    UI_vertex v0;
    v0.color = color;
    v0.position = glm::round(vec3(pos.x + (g.pos_line[0] + offsets[0].x) * text_size, pos.y + (g.pos_line[1]) * text_size, 0));
    v0.tex_coord = vec2(g.pos_tex[0] + offsets[0].y, g.pos_tex[1]);

    v0.data = 0x1;
    if(bold) v0.data |= 0x2;
    if(italic) v0.data |= 0x4;


    UI_vertex v1 = v0;
    v1.position = glm::round(glm::vec3{pos.x + (g.pos_line[0] + g.size[0] + offsets[1].x) * text_size, pos.y + (g.pos_line[1]) * text_size, 0});
    v1.tex_coord = vec2(g.pos_tex[0] + g.size[0] + offsets[1].y, g.pos_tex[1]);

    UI_vertex v2 = v0;
    v2.position = glm::round(glm::vec3{pos.x + (g.pos_line[0] + offsets[2].x) * text_size, pos.y + (g.pos_line[1] + g.size[1]) * text_size, 0});
    v2.tex_coord = vec2(g.pos_tex[0] + offsets[2].y, g.pos_tex[1] + g.size[1]);

    UI_vertex v3 = v0;
    v3.position = glm::round(glm::vec3{pos.x + (g.pos_line[0] + g.size[0] + offsets[3].x) * text_size, pos.y + (g.pos_line[1] + g.size[1]) * text_size, 0});
    v3.tex_coord = vec2(g.pos_tex[0] + g.size[0] + offsets[3].y, g.pos_tex[1] + g.size[1]);

    vertices.push_back(v0);
    vertices.push_back(v1);
    vertices.push_back(v2);
    vertices.push_back(v2);
    vertices.push_back(v1);
    vertices.push_back(v3);
}

std::vector<uint8_t> get_bytes_from_file(char* path) {
    std::vector<uint8_t> bytes;

    std::ifstream file;
    file.open(path, std::ios::in | std::ios::binary);

    if(file.is_open()) {
        file.seekg(0, std::ios::end);
        int size = file.tellg();
        file.seekg(0, std::ios::beg);

        bytes = std::vector<uint8_t>(size);
        file.read((char*)bytes.data(), size);
    } else {
        std::cout << "failed to open file " << path << std::endl;
    }

    file.close();

    return bytes;
}

std::string to_base(int32_t num, int base) {
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

std::string to_base(int64_t num, int base) {
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

std::string to_base(float num, int base, int max_float) {
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
    }

    if(neg) ret *= -1;

    return ret;
}

std::vector<UI_vertex> GUI_system::create_mesh(std::string string, uint32_t text_size, ivec2& size, bool size_mode) {
    std::vector<UI_vertex> vertices;

    glm::vec2 p = {-1 * int(text_size), -11 * int(text_size)};

    bool flag_italic = false;
    bool flag_bold = false;
    glm::vec3 color = {1.0, 1.0, 1.0};

    int skip = 0;
    
    for(int i = 0; i < string.size(); ++i) {
        if(skip > 0) {
            --skip;
            continue;
        }

        char c = string[i];
        if(c == '\\' && skip != -1) {
            if(i < string.size() - 1) {
                char c1 = string[i + 1];
                if(c1 == '\\') {
                    skip = -1;
                    continue;
                } else if(c1 == 'i') {
                    flag_italic = true;
                    skip = 1;
                    continue;
                } else if(c1 == 'b') {
                    flag_bold = true;
                    skip = 1;
                    continue;
                } else if(c1 == 'r') {
                    flag_italic = false;
                    flag_bold = false;
                    skip = 1;
                    continue;
                } else if(c1 == 'c') {
                    if(i < string.size() - 4) {
                        char c2 = string[i + 2];
                        char c3 = string[i + 3];
                        char c4 = string[i + 4];

                        if(std::find(integers.begin(), integers.end(), c2) != integers.end() &&
                        std::find(integers.begin(), integers.end(), c3) != integers.end() &&
                        std::find(integers.begin(), integers.end(), c4) != integers.end()) {
                            skip = 4;
                            color = {float(from_base(std::string(1, c2), 16)) / 15, float(from_base(std::string(1, c3), 16)) / 15, float(from_base(std::string(1, c4), 16)) / 15};
                            continue; 
                        }
                    }
                } else if(c1 == 'n') {
                    p.x = 0;
                    p.y -= 13 * text_size;
                    skip = 1;
                    continue;
                }
            }
        } else if(c == '\n') {
            p.x = 0;
            p.y -= 13 * text_size;
            continue;
        }

        if(skip < 0) {
            ++skip;
        }

        Glyph_data& g = font.glyph_map[c];
 
        if(g.visible) {
            insert_char(vertices, font, c, p, text_size, flag_italic, flag_bold, vec4(color, 1.0));
        }

        p.x += round((float(g.stride) + 0.5 * flag_bold) * text_size);
    }

    if(size_mode) {
        ivec2 min_v = ivec2(0x7FFFFFFF);
        ivec2 max_v = ivec2(-0x7FFFFFFF);
        
        for(UI_vertex& v : vertices) {
            ivec2 pos = v.position.xy();

            min_v = glm::min(pos, min_v);
            max_v = glm::max(pos, max_v);
        }
        
        for(UI_vertex& v : vertices) {
            v.position.z = 0.5f;
            v.position.y = v.position.y - min_v.y;
        }

        size = abs(max_v - min_v);
    } else {
        ivec2 max_v = ivec2(0);

        for(UI_vertex& v : vertices) {
            ivec2 pos = v.position.xy();

            max_v.x = glm::max(max_v.x, pos.x);
            max_v.y = glm::min(max_v.y, pos.y);
        }

        for(UI_vertex& v : vertices) {
            v.position.z = 0.5f;
            v.position.y -= max_v.y;
        }

        size = abs(max_v);
    }

    return vertices;
}


void GUI_system::capture_cursor() {
    for(uint32_t entity : collectors[3].entities) {
        Window_widget& w = ecs.get_component<Window_widget>(entity);

        Input_system& input_system = ecs.get_system<Input_system>();
        if(input_system.cursor_pos.x > w.position.x && input_system.cursor_pos.x < w.position.x + w.size.x && input_system.cursor_pos.y > w.position.y && input_system.cursor_pos.y < w.position.y + w.size.y) {
            cursor_captured = true;
            break;
        }
    }
}

ivec2 GUI_system::recursive_position(uint32_t entity, ivec4 window, ivec2 position, bool off) {
    Widget& widget = ecs.get_component<Widget>(entity);

    if(widget.parent != 0xFFFFFFFF) {
        ivec2 new_pos = position + ivec2(widget.border.x, -widget.border.y) + ivec2(0, -widget.size.y);

        if(widget.position != new_pos) remesh = true;
        widget.position = new_pos;

        if(!widget.toggle || off) {
            off = true;
            widget.position = ivec2(-0x7FFFFFFF);
        }

        widget.window = window;
    }

    ivec4 new_window = window;
    new_window = ivec4(max(new_window.x, widget.window.x), max(new_window.y, widget.window.y), min(new_window.z, widget.window.z), min(new_window.w, widget.window.w));

    /*if(widget.window.z != -1) {
        ivec2 min_window = window.xy();
        ivec2 max_window = window.xy() + window.zw();

        min_window = max(min_window, widget.window.xy());
        max_window = min(max_window, widget.window.xy() + widget.window.xy() + widget.window.zw());

        new_window = ivec4(min_window, max_window - min_window);
    }*/
    ivec2 pos;

    if(widget.child_mode == CM_CONTINUE || widget.child_mode == CM_SURROUND) {
        pos = position + widget.child_offset;
    } else {
        if(widget.parent != 0xFFFFFFFF) {
            Widget& parent_widget = ecs.get_component<Widget>(widget.parent);
            pos = ivec2(parent_widget.position.x, position.y + widget.child_offset.y - widget.border.y - widget.border.w);
        }
    }
    
    if(widget.child_mode == CM_SURROUND) {
        ivec2 min_v = ivec2(0x7FFFFFFF);
        ivec2 max_v = ivec2(-0x7FFFFFFF);
        for(uint32_t child : widget.children) {
            ivec2 add_pos = recursive_position(child, new_window, pos);
            pos = add_pos;
            
            Widget& child_widget = ecs.get_component<Widget>(child);

            ivec2 min_pos = child_widget.position - ivec2(child_widget.border.x, child_widget.border.w);
            ivec2 max_pos = child_widget.position + child_widget.size + ivec2(child_widget.border.z, child_widget.border.y);

            min_v = min(min_pos, min_v);
            max_v = max(max_pos, max_v);
        }

        ivec2 new_size = max_v - min_v;
        widget.size = new_size;
    } else {
        for(uint32_t child : widget.children) {
            ivec2 add_pos = recursive_position(child, new_window, pos);
            pos = add_pos;
        }
    }

    ivec2 new_position;

    if(widget.sibling_mode == SM_LEFT) {
        new_position = position + ivec2(widget.size.x + widget.border.z, 0);
    } else if(widget.sibling_mode == SM_DOWN) {
        new_position = position + ivec2(0, -widget.size.y - widget.border.w - widget.border.y);
    } else if(widget.sibling_mode == SM_INCLUDE_CHILDREN) {
        new_position = pos;
    }

    return new_position;
};

void GUI_system::recursive_toggle(uint32_t entity, bool toggle, bool first) {
    Widget& widget = ecs.get_component<Widget>(entity);

    if(!toggle) {
        if(!first) widget.toggle = toggle;
        for(uint32_t child : widget.children) {
            recursive_toggle(child, toggle, false);
        }
    } else {
        if(!first) widget.toggle = toggle;
        if(widget.toggle_parent) {
            if(widget.open) {
                for(uint32_t child : widget.children) {
                    recursive_toggle(child, toggle, false);
                }
            }
        } else {
            for(uint32_t child : widget.children) {
                recursive_toggle(child, toggle, false);
            }
        }
    }
}

bool includes(ivec4 range, ivec2 point) {
    return (point.x > range.x && point.x < range.x + range.z && point.y > range.y && point.y < range.y + range.w);
}

void GUI_system::call() {
    Input_system& input_system = ecs.get_system<Input_system>();
    cursor_captured = false;

    if(!input_system.cursor_disabled) {
        uint32_t hovered_widget = 0xFFFFFFFF;

        if(selected_widget == 0xFFFFFFFF) {
            cursor_mode = CURSOR_CLICK;

            for(uint32_t entity : collectors[3].entities) {
                Window_widget& window = ecs.get_component<Window_widget>(entity); 
                Widget& widget = ecs.get_component<Widget>(entity); 


                int thickness = 6;
                int border = 3;
            
                if(widget.open) {
                    ivec4 range_bottom = ivec4(window.position - border, window.size.x + border * 2, thickness);
                    ivec4 range_top = ivec4(window.position.x - border, window.position.y + window.size.y - thickness + border, window.size.x + border * 2, thickness);
                    ivec4 range_left = ivec4(window.position - border, thickness, window.size.y + border * 2);
                    ivec4 range_right = ivec4(window.position.x + window.size.x - thickness + border, window.position.y - border, thickness, window.size.y + border * 2);

                    bool bottom = includes(range_bottom, input_system.cursor_pos);
                    bool top = includes(range_top, input_system.cursor_pos);
                    bool left = includes(range_left, input_system.cursor_pos);
                    bool right = includes(range_right, input_system.cursor_pos);

                    if(bottom) {
                        if(left) cursor_mode = CURSOR_RESIZE_BL;
                        else if(right) cursor_mode = CURSOR_RESIZE_BR;
                        else cursor_mode = CURSOR_RESIZE_B;
                    } else if(top) {
                        if(left) cursor_mode = CURSOR_RESIZE_TL;
                        else if(right) cursor_mode = CURSOR_RESIZE_TR;
                        else cursor_mode = CURSOR_RESIZE_T;
                    } else if(left) cursor_mode = CURSOR_RESIZE_L;
                    else if(right) cursor_mode = CURSOR_RESIZE_R;

                    if(cursor_mode != CURSOR_CLICK) {
                        hovered_widget = entity;
                        cursor_captured = true;
                    }
                } else {
                    ivec4 range_left = ivec4(window.position - border, thickness, window.size.y + border * 2);
                    ivec4 range_right = ivec4(window.position.x + window.size.x - thickness + border, window.position.y - border, thickness, window.size.y + border * 2);

                    bool left = includes(range_left, input_system.cursor_pos);
                    bool right = includes(range_right, input_system.cursor_pos);

                    if(left) cursor_mode = CURSOR_RESIZE_L;
                    else if(right) cursor_mode = CURSOR_RESIZE_R;

                    if(cursor_mode != CURSOR_CLICK) {
                        hovered_widget = entity;
                        cursor_captured = true;
                    }
                }
            }
        }

        if(selected_widget != 0xFFFFFFFF) {
            if(ecs.has_component<Window_widget>(selected_widget)) {
                Window_widget& w = ecs.get_component<Window_widget>(selected_widget); 
                Widget& widget = ecs.get_component<Widget>(selected_widget); 

                if(resize) {
                    if(cursor_mode == CURSOR_RESIZE_T || cursor_mode == CURSOR_RESIZE_TL || cursor_mode == CURSOR_RESIZE_TR) { // top
                        w.size.y += input_system.cursor_delta.y;
                    } else if(cursor_mode == CURSOR_RESIZE_B || cursor_mode == CURSOR_RESIZE_BL || cursor_mode == CURSOR_RESIZE_BR) { // bottom
                        w.size.y -= input_system.cursor_delta.y;
                        w.position.y += input_system.cursor_delta.y;
                    }
                    if(cursor_mode == CURSOR_RESIZE_L || cursor_mode == CURSOR_RESIZE_BL || cursor_mode == CURSOR_RESIZE_TL) { // left
                        w.size.x -= input_system.cursor_delta.x;
                        w.position.x += input_system.cursor_delta.x;
                    } else if(cursor_mode == CURSOR_RESIZE_R || cursor_mode == CURSOR_RESIZE_BR || cursor_mode == CURSOR_RESIZE_TR) { // right
                        w.size.x += input_system.cursor_delta.x;
                    }
                } else {
                    w.position += ivec2(input_system.cursor_delta);

                    ivec2 min_range = ivec2(20 - w.size.x, 20 - w.size.y);
                    ivec2 max_range = ivec2(core.window.screen_size.x - 20, core.window.screen_size.y - w.size.y);

                    w.position.x = clamp(w.position.x, min_range.x, max_range.x);
                    w.position.y = clamp(w.position.y, min_range.y, max_range.y);
                }
                remesh = true;

                widget.position = w.position;
                widget.size = w.size;
                widget.window = ivec4(w.position, w.size.x + w.position.x, w.size.y - w.bar_size + w.position.y);
                widget.child_offset = ivec2(0, w.size.y - w.bar_size);
            }
        }

        if(input_system.click) {
            bool selected = false;

            for(uint32_t entity : collectors[3].entities) {
                Window_widget& w = ecs.get_component<Window_widget>(entity);
                Widget& widget = ecs.get_component<Widget>(entity);

                if(cursor_mode == CURSOR_CLICK) {
                    bool drop = false;
                    
                    // toggle drop
                    int32_t offset_x = w.bar_size / 2 - 10 / 2;
                    int32_t offset_y = w.bar_size / 2 + 10 / 2;
                    ivec2 pos = w.position + ivec2(offset_x, w.size.y - offset_y);
                    ivec4 new_range = ivec4(pos, 10, 10);

                    if(includes(new_range, input_system.cursor_pos)) {
                        widget.open = !widget.open;
                        recursive_toggle(entity, widget.open);

                        drop = true;
                        remesh = true;
                    }

                    if(!drop) {
                        vec4 range = vec4(w.position.x, w.position.y + w.size.y - 20, w.size.x, 20);
                        if(includes(range, input_system.cursor_pos)) { // top move
                            selected_widget = entity;
                            resize = false;
                            selected = true;
                        }
                    }
                } else {
                    if(hovered_widget != 0xFFFFFFFF) {
                        selected_widget = hovered_widget;
                        selected = true;
                        resize = true;
                    }
                }
            }

            if(!selected) {
                selected_widget = 0xFFFFFFFF;
            }

            uint32_t switch_tab_parent = 0xFFFFFFFF;
            uint32_t switch_tab = 0;

            for(uint32_t entity : collectors[5].entities) {
                Widget& w = ecs.get_component<Widget>(entity);

                vec4 range = vec4(w.position.xy(), w.size.xy());
                if(includes(range, input_system.cursor_pos)) {
                    switch_tab_parent = w.parent;
                    switch_tab = entity;
                    break;
                }
            }

            // toggle tabs

            if(switch_tab_parent != 0xFFFFFFFF) {
                Widget& w = ecs.get_component<Widget>(switch_tab_parent);

                for(uint32_t child : w.children) {
                    if(ecs.has_component<Tab>(child)) {
                        Widget& tab_widget = ecs.get_component<Widget>(child);
                        Tab& t = ecs.get_component<Tab>(child);
                        if(child == switch_tab) {
                            tab_widget.open = true;
                            remesh = true;
                            recursive_toggle(child, true);
                        } else {
                            tab_widget.open = false;
                            remesh = true;
                            recursive_toggle(child, false);
                        }
                    }
                }
            }
        } else if(!input_system.key_map[GLFW_MOUSE_BUTTON_LEFT]) {
            selected_widget = 0xFFFFFFFF;
        }
    } else selected_widget = 0xFFFFFFFF;
    
    for(uint32_t entity : collectors[4].entities) {
        Widget& w = ecs.get_component<Widget>(entity);
        Button& b = ecs.get_component<Button>(entity);

        vec4 range = vec4(w.position, w.size);

        if(input_system.cursor_pos.x > range.x && input_system.cursor_pos.x < range.x + range.z && input_system.cursor_pos.y > range.y && input_system.cursor_pos.y < range.y + range.w){
            if(!b.hovered) {
                b.hovered = true;
                remesh = true;
            }

            if(input_system.click) {
                if(!b.click) {
                    b.click = true;
                    remesh = true;

                    b.callback();
                }
            }
        } else {
            if(b.hovered) {
                b.hovered = false;   
                remesh = true;
            }
        }
        
        if(!input_system.key_map[GLFW_MOUSE_BUTTON_LEFT]) {
            if(b.click) {
                b.click = false;
                remesh = true;
            }
        }
    }

    for(uint32_t entity : collectors[2].entities) {
        Text& t = ecs.get_component<Text>(entity);
        Widget& w = ecs.get_component<Widget>(entity);

        std::string s = t.callback();

        if(s == "") s = t.string;

        if(s != t.string || t.vertices.size() == 0) {
            t.string = s;
            ivec2 size;
            t.vertices = create_mesh(t.string, t.text_size, size, t.size_mode);
            t.size = size;
            
            remesh = true;

            if(t.is_main) w.size = size;
        }
    }

    std::vector<uint32_t> roots;

    for(uint32_t entity : collectors[1].entities) {
        Widget& w = ecs.get_component<Widget>(entity);

        if(w.parent == 0xFFFFFFFF) {
            roots.push_back(entity);
        }
    }

    for(uint32_t entity : roots) {
        Widget& widget = ecs.get_component<Widget>(entity);
        
        recursive_position(entity, widget.window, widget.position);
    }
    
    capture_cursor();
}

void GUI_system::create_mesh() {
    if(!vertices->initialized) vertices->init();

    std::vector<UI_vertex> v;
    ivec4 full_window = ivec4(0, 0, 0x7FFFFFFF, 0x7FFFFFFF);

    std::function<void(vec4, ivec4, ivec4)> insert_flat = [&v](vec4 color, ivec4 range, ivec4 window) {
        UI_vertex v0;
        v0.color = color;
        v0.data = 0x2;
        v0.position = vec3(range.xy(), 0.5f);
        v0.tex_coord = vec2(0.0f, 0.0f);
        v0.range = window;

        UI_vertex v1 = v0;
        v1.position = vec3(range.x + range.z, range.y, 0.5f);

        UI_vertex v2 = v0;
        v2.position = vec3(range.x, range.y + range.w, 0.5f);
        
        UI_vertex v3 = v0;
        v3.position = vec3(range.xy() + range.zw(), 0.5f);

        v.push_back(v0);
        v.push_back(v1);
        v.push_back(v3);
        v.push_back(v0);
        v.push_back(v3);
        v.push_back(v2);
    };

    std::function<void(vec4, ivec4, ivec4)> insert_tex = [&v](vec4 tex, ivec4 range, ivec4 window) {
        UI_vertex v0;
        v0.color = vec4(1.0f);
        v0.data = 0x0;
        v0.position = vec3(range.xy(), 0.5f);
        v0.tex_coord = vec2(tex.xy());
        v0.range = window;

        UI_vertex v1 = v0;
        v1.position = vec3(range.x + range.z, range.y, 0.5f);
        v1.tex_coord = vec2(tex.x + tex.z, tex.y);

        UI_vertex v2 = v0;
        v2.position = vec3(range.x, range.y + range.w, 0.5f);
        v2.tex_coord = vec2(tex.x, tex.y + tex.w);
        
        UI_vertex v3 = v0;
        v3.position = vec3(range.xy() + range.zw(), 0.5f);
        v3.tex_coord = vec2(tex.x + tex.z, tex.y + tex.w);

        v.push_back(v0);
        v.push_back(v1);
        v.push_back(v3);
        v.push_back(v0);
        v.push_back(v3);
        v.push_back(v2);
    };

    for(uint32_t entity : collectors[3].entities) {
        Window_widget& w = ecs.get_component<Window_widget>(entity);
        Widget& widget = ecs.get_component<Widget>(entity);

        vec4 color;
        ivec4 range;
        if(widget.open) {
            // panel
            color = vec4(0.25f, 0.25f, 0.25f, 0.1f);
            range = ivec4(w.position, w.size);
            insert_flat(color, range, full_window);
        }

        // top

        color = vec4(1.0f, 0.25f, 0.25f, 1.0f);
        range = vec4(w.position.x, w.position.y + w.size.y - w.bar_size, w.size.x, w.bar_size);
        insert_flat(color, range, full_window);


        // expand icon
        uint32_t offset_x = w.bar_size / 2 - 10 / 2;
        uint32_t offset_y = w.bar_size / 2 + 10 / 2;
        vec4 texture_range;
        if(widget.open) texture_range = vec4(15, 0, 5, 5);
        else texture_range = vec4(10, 0, 5, 5);

        ivec2 pos = w.position + ivec2(offset_x, w.size.y - offset_y);
        range = ivec4(pos, 10, 10);

        insert_tex(texture_range, range, full_window);


        // text
        if(ecs.has_component<Text>(entity)) {
            Text& t = ecs.get_component<Text>(entity);

            std::vector<UI_vertex> vvv = t.vertices;

            ivec2 pos = w.position + ivec2(offset_x * 2 + 10, w.size.y - w.bar_size / 2 - t.size.y / 2);

            for(UI_vertex& vv : vvv) {
                vv.position += vec3(pos, 0);
                vv.range = vec4(w.position.x, w.position.y + w.size.y - w.bar_size, w.position.x + w.size.x, w.position.y + w.size.y);
            }

            v.insert(v.end(), vvv.begin(), vvv.end());
        }
    }
    
    for(uint32_t entity : collectors[4].entities) {
        Widget& w = ecs.get_component<Widget>(entity);
        Button& b = ecs.get_component<Button>(entity);

        vec3 color = vec3(0.25f, 0.25f, 1.0f);
        if(b.click) color = color * 0.5f + 0.5f;
        else if(b.hovered) color = color * 0.75f + 0.25f;
        
        ivec4 range = ivec4(w.position, w.size);
        vec4 color_a = vec4(color, 1.0f);

        insert_flat(color_a, range, w.window);

        if(ecs.has_component<Text>(entity)) {
            Text& t = ecs.get_component<Text>(entity);

            std::vector<UI_vertex> vvv = t.vertices;

            ivec2 pos = w.position + w.size / 2 - t.size / 2;

            for(UI_vertex& vv : vvv) {
                vv.position += vec3(pos, 0);
                vv.range = w.window;
            }

            v.insert(v.end(), vvv.begin(), vvv.end());
        }
    }

    for(uint32_t entity : collectors[5].entities) {
        Widget& w = ecs.get_component<Widget>(entity);
        Tab& t = ecs.get_component<Tab>(entity);

        vec3 color = vec3(0.25f, 0.25f, 1.0f);

        if(!w.open) color = vec3(0.25f, 0.25f, 0.25f);

        //if(b.click) color = color * 0.5f + 0.5f;
        //else if(b.hovered) color = color * 0.75f + 0.25f;
        
        ivec4 range = ivec4(w.position, w.size);
        uint32_t pinch = 6;

        UI_vertex v0;
        v0.color = vec4(color, 1.0f);
        v0.data = 0x2;
        v0.position = vec3(range.xy(), 0.5f);
        v0.tex_coord = vec2(0.0f, 0.0f);
        v0.range = w.window;

        UI_vertex v1 = v0;
        v1.position = vec3(range.x + range.z, range.y, 0.5f);

        UI_vertex v2 = v0;
        v2.position = vec3(range.x + pinch, range.y + range.w, 0.5f);
        
        UI_vertex v3 = v0;
        v3.position = vec3(range.xy() + range.zw() + ivec2(-pinch, 0), 0.5f);

        v.push_back(v0);
        v.push_back(v1);
        v.push_back(v3);
        v.push_back(v0);
        v.push_back(v3);
        v.push_back(v2);

        if(ecs.has_component<Text>(entity)) {
            Text& t = ecs.get_component<Text>(entity);

            std::vector<UI_vertex> vvv = t.vertices;

            ivec2 pos = w.position + w.size / 2 - t.size / 2;

            for(UI_vertex& vv : vvv) {
                vv.position += vec3(pos, 0);
                vv.range = w.window;
            }

            v.insert(v.end(), vvv.begin(), vvv.end());
        }
    }

    for(uint32_t entity : collectors[6].entities) {
        Widget& w = ecs.get_component<Widget>(entity);
        Panel& p = ecs.get_component<Panel>(entity);

        vec4 color = vec4(1.0f, 0.25f, 1.0f, 1.0f);

        // left
        ivec4 range = ivec4(w.position.x - p.inner_border - p.line_width, w.position.y - p.inner_border - p.line_width, p.line_width, w.size.y + (p.inner_border + p.line_width) * 2);
        insert_flat(color, range, w.window);
        
        // bottom
        range = ivec4(w.position.x - p.inner_border - p.line_width, w.position.y - p.inner_border - p.line_width, w.size.x + (p.inner_border + p.line_width) * 2, p.line_width);
        insert_flat(color, range, w.window);
        
        // right
        range = ivec4(w.position.x + w.size.x + p.inner_border, w.position.y - p.inner_border - p.line_width, p.line_width, w.size.y + (p.inner_border + p.line_width) * 2);
        insert_flat(color, range, w.window);
        
        // top
        range = ivec4(w.position.x - p.inner_border - p.line_width, w.position.y + w.size.y + p.inner_border, w.size.x + (p.inner_border + p.line_width) * 2, p.line_width);
        insert_flat(color, range, w.window);
    }
    
    for(uint32_t entity : collectors[collectors.size() - 1].entities) {
        Text& t = ecs.get_component<Text>(entity);
        Widget& w = ecs.get_component<Widget>(entity);
        
        vec4 color = vec4(1.0f, 0.0f, 1.0f, 0.25f);
        ivec4 range = ivec4(w.position, w.size);
        insert_flat(color, range, w.window);

        std::vector<UI_vertex> vvv = t.vertices;

        for(UI_vertex& vv : vvv) {
            vv.position += vec3(w.position, 0);
            vv.range = w.window;
        }

        v.insert(v.end(), vvv.begin(), vvv.end());
    }
    
    vertices->vertex_buffer_data(v.data(), v.size(), sizeof(UI_vertex), GL_STREAM_DRAW);

    vertices->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(UI_vertex), 0);
    vertices->add_vertex_attribute(1, 2, GL_FLOAT, false, sizeof(UI_vertex), 3 * sizeof(float));
    vertices->add_vertex_attribute(2, 4, GL_FLOAT, false, sizeof(UI_vertex), 5 * sizeof(float));
    vertices->add_vertex_attribute(3, 1, GL_INT, false, sizeof(UI_vertex), 9 * sizeof(float));
    vertices->add_vertex_attribute(4, 4, GL_INT, false, sizeof(UI_vertex), 10 * sizeof(float));
}

void GUI_system::render() {
    if(remesh) { 
        create_mesh();
        remesh = false;
    }

    std::shared_ptr<Shader> ui_shader = core.shaders["gui_shader"];
    std::shared_ptr<Texture> ui_texture = core.textures["gui_texture"];
    std::shared_ptr<Texture> text_texture = core.textures["text_texture"];

    glm::mat3 view_mat;
    glm::mat3 trans_mat;

    glm::ivec2 half_viewport_size = core.window.viewport_size / 2;

    view_mat = glm::scale(glm::translate(glm::identity<glm::mat3>(), {-1, -1}), glm::vec2{1.0 / half_viewport_size.x, 1.0 / half_viewport_size.y});
    trans_mat = glm::identity<glm::mat3>();

    ui_shader->use();
    ui_texture->bind(0);
    text_texture->bind(1);
    vertices->bind();

    glUniformMatrix3fv(0, 1, false, &view_mat[0][0]);
    glUniformMatrix3fv(1, 1, false, &trans_mat[0][0]);

    vertices->draw_vertices(GL_TRIANGLES);
}


void GUI_system::add_window(ivec2 position, ivec2 size, std::string label) {
    uint32_t entity = ecs.insert_entity();

    Widget widget;
    Window_widget window;

    uint32_t bar_size = 20;
    uint32_t resize_size = 16;

    window.bar_size = bar_size;
    window.position = position;
    window.size = size + ivec2(0, 20.0f);
    window.resize_icon_size = resize_size;

    widget.size = window.size;
    widget.position = position;
    widget.window = ivec4(position, size.x + position.x, size.y + position.y);
    widget.child_offset = ivec2(0, size.y);
    widget.toggle_parent = true;
    widget.open = true;

    ecs.insert_component(entity, widget);
    ecs.insert_component(entity, window);

    if(label.size()) {
        Text text;
        text.string = label;
        text.size_mode = true;
        text.callback = null_callback;
        ecs.insert_component(entity, text);
    }

    current_entity = entity;
}

void GUI_system::add_text(std::string text) {
    uint32_t entity = ecs.insert_entity();

    Widget widget;
    Text t;
    t.string = text;
    t.callback = null_callback;
    widget.border = ivec4(2);
    t.is_main = true;

    ecs.insert_component(entity, widget);
    ecs.insert_component(entity, t);

    parent(current_entity, entity);
    
}

void GUI_system::add_text(std::function<std::string()> callback) {
    uint32_t entity = ecs.insert_entity();

    Widget widget;
    Text t;
    t.callback = callback;
    widget.border = ivec4(2);
    t.is_main = true;

    ecs.insert_component(entity, widget);
    ecs.insert_component(entity, t);
    
    parent(current_entity, entity);
    
}

void GUI_system::widget_return(int32_t v = 1) {
    if(v == -1) {
        while(true) {
            Widget& w = ecs.get_component<Widget>(current_entity);

            if(w.parent == 0xFFFFFFFF) break;
            current_entity = w.parent;
        }
    } else if(v == 0) {
        while(true) {
            Widget& w = ecs.get_component<Widget>(current_entity);

            current_entity = w.parent;
            if(current_entity == 0xFFFFFFFF) break;
        }
    } else {
        uint32_t vv = v;

        while(vv != 0) {
            Widget& w = ecs.get_component<Widget>(current_entity);

            if(w.parent == 0xFFFFFFFF) break;
            current_entity = w.parent;
            --vv;
        }
    }
}

void GUI_system::widget_set(uint32_t u) {
    current_entity = u;
}

void GUI_system::parent(uint32_t parent, uint32_t child) {
    Widget& wp = ecs.get_component<Widget>(parent);
    Widget& wc = ecs.get_component<Widget>(child);
    wp.children.push_back(child);
    wc.parent = parent;

    if(!wp.toggle || (wp.toggle_parent && !wp.open)) wc.toggle = false;
}

void GUI_system::add_button(ivec2 size, std::function<void()> callback, std::string label) {
    uint32_t entity = ecs.insert_entity();

    Widget widget;
    Button button;
    button.size = size;
    button.callback = callback;
    widget.size = size;
    widget.border = ivec4(2);
    if(label.size()) {
        Text text;
        text.size_mode = true;
        text.string = label;
        text.callback = null_callback;
        ecs.insert_component(entity, text);
    }

    ecs.insert_component(entity, widget);
    ecs.insert_component(entity, button);
    
    parent(current_entity, entity);
}

void GUI_system::add_tab(ivec2 size, std::string label) {
    uint32_t entity = ecs.insert_entity();

    Tab tab;
    Widget widget;

    tab.size = size;
    widget.size = size;
    widget.sibling_mode = SM_LEFT;
    widget.child_mode = CM_RETURN;
    widget.child_offset = ivec2(0, -size.y);
    widget.toggle_parent = true;
    widget.border = ivec4(2);

    if(num_children<Tab>(current_entity) == 0) {
        widget.open = true;
    }

    if(label.size()) {
        Text text;
        text.size_mode = true;
        text.string = label;
        text.callback = null_callback;
        ecs.insert_component(entity, text);
    }
    
    ecs.insert_component(entity, tab);
    ecs.insert_component(entity, widget);
    
    parent(current_entity, entity);

    current_entity = entity;
}

void GUI_system::add_panel(uint32_t line_width, uint32_t inner_border, uint32_t outer_border) {
    uint32_t entity = ecs.insert_entity();

    Panel panel;
    Widget widget;
    panel.inner_border = inner_border;
    panel.outer_border = outer_border;
    panel.line_width = line_width;
    widget.border = ivec4(inner_border + outer_border + line_width);
    widget.child_mode = CM_SURROUND;
    widget.child_offset = ivec2(widget.border.x, -widget.border.y);
    
    ecs.insert_component(entity, panel);
    ecs.insert_component(entity, widget);

    parent(current_entity, entity);
    current_entity = entity;
}