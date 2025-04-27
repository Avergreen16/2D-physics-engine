#pragma once;
#include "wrapper.hpp"
#include "ecs.hpp"
#include "core.hpp"

#include <string>

extern std::string integers;

enum widget_constraint{WC_BOTTOM, WC_CENTER, WC_TOP, WC_NONE};

std::string message_callback();
std::string null_callback();
void button_callback();

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

            file.close();
        } else {
            std::cout << "error" << std::endl;
        }
    }
};

enum Sibling_mode{SM_DOWN, SM_LEFT, SM_INCLUDE_CHILDREN};
enum Child_mode{CM_CONTINUE, CM_RETURN, CM_SURROUND};

struct Widget {
    uint32_t parent = 0xFFFFFFFF;
    std::vector<uint32_t> children;
    Sibling_mode sibling_mode = SM_DOWN;
    Child_mode child_mode = CM_CONTINUE;

    ivec2 position;
    ivec2 size;
    
    ivec2 child_offset = ivec2(0);
    ivec4 border = ivec4(0);

    ivec4 window = ivec4(0, 0, 0x7FFFFFFF, 0x7FFFFFFF);

    uint32_t weight;

    bool toggle = true;
    bool toggle_parent = false;
};

struct Text {
    std::string string;
    uint32_t text_size = 1;
    bool is_main = false;

    ivec2 size = ivec2(0.0f);
    bool size_mode = false;

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
    bool selected = false;
};

struct Panel {
    uint32_t line_width;
    uint32_t inner_border;
    uint32_t outer_border;
};

struct GUI_system : System {
    Font font; 
    
    bool remesh = true;
    uint32_t selected_widget = 0xFFFFFFFF;
    bool resize = false;
    bool cursor_captured = false;

    std::shared_ptr<Vertices> vertices = std::shared_ptr<Vertices>(new Vertices);

    // input settings;
    uint32_t current_entity = 0xFFFFFFFF;

    GUI_system() {
        font.init("res/other resources/pixelfont.atxt");

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
        
        s = ecs.update_signature<Text>();
        collectors.push_back(Collector(s));
    }

    std::vector<UI_vertex> create_mesh(std::string s, uint32_t text_size, ivec2& size, bool size_mode = false);
    
    void create_mesh();

    void capture_cursor();

    void call();

    void render();

    // input functions

    ivec2 recursive_position(uint32_t entity, ivec4 window, ivec2 position, bool off = false);
    void recursive_toggle(uint32_t entity, bool toggle, bool first = true);

    void add_window(ivec2 position, ivec2 size, std::string label);
    void add_text(std::function<std::string()> callback);
    void add_text(std::string text);
    void add_button(ivec2 size, std::function<void()> callback, std::string label);
    void add_tab(ivec2 size, std::string label);
    void add_panel(uint32_t line_width, uint32_t inner_border, uint32_t outer_border);

    void parent(uint32_t parent, uint32_t child);
    void widget_return(int32_t v = 1);
    void widget_set(uint32_t u);
};


std::string to_base(int32_t num, int base);

std::string to_base(int64_t num, int base);

std::string to_base(float num, int base, int max_float);

int from_base(std::string num, int base);