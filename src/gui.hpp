#pragma once;
#include "wrapper.hpp"
#include "ecs.hpp"
#include "core.hpp"

#include <string>

extern std::string integers;

std::string message_callback();
std::string null_callback();
void button_callback();
std::string fps_callback();
std::string physics_callback();

struct UI_vertex {
    vec3 position;
    vec2 tex_coord = vec2(0.0f);
    vec4 color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    uint32_t data = 0;
    ivec4 range = ivec4(0, 0, 0x7FFFFFF, 0x7FFFFFF);
};

// font

struct Glyph_data {
    bool visible;
    uint8_t stride;

    std::array<uint8_t, 2> size;
    std::array<uint16_t, 2> pos_tex;
    std::array<int8_t, 2> pos_line;
};

struct Font {
    uint8_t line_height;
    Glyph_data empty_data = {false, 0, 0, 0};
    std::map<char, Glyph_data> glyph_map;

    Glyph_data& at(char key) {
        if(glyph_map.contains(key)) return glyph_map[key];
        return empty_data;
    }

    void init(std::string filepath) {
        std::ifstream file;
        file.open(filepath, std::ios::in | std::ios::binary);

        if(file.is_open()) {
            file.read((char*)&line_height, 1);

            uint16_t invisible_glyphs;
            file.read((char*)&invisible_glyphs, 2);

            for(int i = 0; i < invisible_glyphs; ++i) {
                uint8_t id;
                Glyph_data data;
                data.visible = false;

                file.read((char*)&id, 1);
                file.read((char*)&data.stride, 1);

                glyph_map.insert({id, data});
            }

            uint16_t visible_glyphs;
            file.read((char*)&visible_glyphs, 2);

            for(int i = 0; i < visible_glyphs; ++i) {
                uint8_t id;
                Glyph_data data;
                data.visible = true;

                file.read((char*)&id, 1);
                file.read((char*)&data.stride, 1);
                file.read((char*)&data.size[0], 1);
                file.read((char*)&data.size[1], 1);
                file.read((char*)&data.pos_tex[0], 2);
                file.read((char*)&data.pos_tex[1], 2);
                file.read((char*)&data.pos_line[0], 1);
                file.read((char*)&data.pos_line[1], 1);

                glyph_map.insert({id, data});
            }
            
            // missing placeholder
            Glyph_data data;
            data.visible = true;
            file.read((char*)&data.stride, 1);
            file.read((char*)&data.size[0], 1);
            file.read((char*)&data.size[1], 1);
            file.read((char*)&data.pos_tex[0], 2);
            file.read((char*)&data.pos_tex[1], 2);
            file.read((char*)&data.pos_line[0], 1);
            file.read((char*)&data.pos_line[1], 1);

            empty_data = data;

            file.close();
        } else {
            std::cout << "error" << std::endl;
        }
    }
};

struct Widget;

void default_func_a(Widget&);
void empty_func(Widget&);
int get_scroll_func(Widget&);

struct Widget {
    uint32_t parent = NULL_ENTITY;
    std::vector<uint32_t> children;
    std::function<void(Widget&)> func_a = default_func_a;
    std::function<void(Widget&)> func_b = empty_func;

    ivec2 position;
    ivec2 size;
    
    ivec2 parent_offset = ivec2(0);
    ivec2 child_offset = ivec2(0);
    ivec4 border = ivec4(0);

    ivec4 window = ivec4(0, 0, 0x7FFFFFFF, 0x7FFFFFFF);

    uint32_t weight;

    bool toggle = true;
    bool toggle_parent = false;
    bool open = false;

    Widget& get_parent() {
        return ecs.get_component<Widget>(parent);
    }

    Widget& get_prev_sibling(bool& has_prev_sibling) {
        Widget& parent = get_parent();

        uint32_t prev_sibling = NULL_ENTITY;
        for(uint32_t sibling : parent.children) {
            Widget& sibling_widget = ecs.get_component<Widget>(sibling);
            if(&sibling_widget == this) {
                break;
            }
            prev_sibling = sibling;
        }

        if(prev_sibling == NULL_ENTITY) {
            has_prev_sibling = false;
            return parent;
        } else {
            has_prev_sibling = true;
            return ecs.get_component<Widget>(prev_sibling);
        }
    }
};

struct Text {
    std::string string;
    uint32_t text_size = 1;
    bool is_main = false;

    ivec2 size = ivec2(0.0f);
    bool size_mode = false;
    vec3 start_color = vec3(1.0f);

    bool remesh = false;

    std::vector<UI_vertex> vertices;

    std::function<std::string()> callback; 
};

struct Button {
    ivec2 size;
    bool hovered = false;
    bool click = false;
    
    std::function<void()> callback;
};

struct Window_widget {
    ivec2 position;
    ivec2 size;

    int bar_size = 20;
    int resize_icon_size = 16;
};

struct Tab {
    ivec2 size;
};

struct Panel {
    uint32_t line_width;
    uint32_t inner_border;
    uint32_t outer_border;
};

struct Text_input {
    bool selected = false;
    uint32_t width = 0;
    int cursor_pos = -1;
    int offset = 0;
};

struct Scrollbar {
    uint32_t width;
    uint32_t scroll_pixels;
    uint32_t bar_width;
    uint32_t bar_offset = 0.0;
    float scroll = 0;
    uint32_t scroll_child;
    std::function<int(Widget&)> scroll_func = get_scroll_func;
};

enum cursor_mode{CURSOR_CLICK, CURSOR_RESIZE_T, CURSOR_RESIZE_TR, CURSOR_RESIZE_R, CURSOR_RESIZE_BR, CURSOR_RESIZE_B, CURSOR_RESIZE_BL, CURSOR_RESIZE_L, CURSOR_RESIZE_TL, CURSOR_TEXT};

struct GUI_system : System {
    Font font; 
    
    bool remesh = true;
    uint32_t selected_widget = NULL_ENTITY;
    uint32_t text_input_widget = NULL_ENTITY;
    bool selected = false;
    bool resize = false;
    bool cursor_captured = false;
    bool keys_captured = false;
    cursor_mode cursor_mode = CURSOR_CLICK;

    std::shared_ptr<Vertices> vertices = std::shared_ptr<Vertices>(new Vertices);

    // input settings;
    uint32_t current_entity = NULL_ENTITY;

    GUI_system() {
        font.init("res/other resources/alter_mono.afont");

        Signature s = ecs.update_signature<Camera>();
        ecs.update_signature<Transform>(s);
        collectors.push_back(Collector(s));
        
        s = ecs.update_signature<Widget>();
        collectors.push_back(Collector(s, false));
        
        s = ecs.update_signature<Text>();
        collectors.push_back(Collector(s, false));
        
        s = ecs.update_signature<Window_widget>();
        collectors.push_back(Collector(s));
        
        s = ecs.update_signature<Button>();
        collectors.push_back(Collector(s));
        
        s = ecs.update_signature<Tab>();
        collectors.push_back(Collector(s));
        
        s = ecs.update_signature<Panel>();
        collectors.push_back(Collector(s));
        
        s = ecs.update_signature<Text_input>();
        collectors.push_back(Collector(s));
        
        s = ecs.update_signature<Scrollbar>();
        collectors.push_back(Collector(s));
        
        s = ecs.update_signature<Text>();
        collectors.push_back(Collector(s));
    }

    std::vector<UI_vertex> create_mesh(std::string s, uint32_t text_size, ivec2& size, vec3 start_color, bool size_mode = false);
    
    void create_mesh();

    void capture_cursor();

    void call();

    void render();

    // input functions

    void recursive_position(uint32_t entity, ivec4 window);
    void recursive_toggle(uint32_t entity, bool toggle, bool first = true);

    void add_window(ivec2 position, ivec2 size, std::string label);
    void add_text(std::function<std::string()> callback);
    void add_text(std::string text);
    void add_button(ivec2 size, std::function<void()> callback, std::string label);
    void add_tab(ivec2 size, std::string label, bool side_tab = false);
    void add_panel(uint32_t line_width, uint32_t inner_border, uint32_t outer_border);
    void add_input(uint32_t width, std::string start_text);
    void add_scrollbar(uint32_t width, uint32_t bar_width);

    template<typename Type>
    uint32_t num_children(uint32_t parent) {
        uint32_t num = 0;

        Widget& parent_widget = ecs.get_component<Widget>(parent);

        for(uint32_t child : parent_widget.children) {
            if(ecs.has_component<Type>(child)) {
                ++num;
            }
        }

        return num;
    }

    void parent(uint32_t parent, uint32_t child);
    void widget_return(int32_t v = 1);
    void widget_set(uint32_t u);
};


std::string to_base(int32_t num, int base);

std::string to_base(int64_t num, int base);

std::string to_base(float num, int base, int max_float);

int from_base(std::string num, int base);