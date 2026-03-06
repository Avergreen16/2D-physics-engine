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
std::string position_callback();
std::string physics_callback();
std::string mode_callback();

struct UI_vertex {
    vec2 pos;
    vec2 tex_pos;
    vec4 color = vec4(1.0f);
    vec4 range = vec4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX);
    uint data = 0;
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
        if(glyph_map.find(key) != glyph_map.end()) return glyph_map[key];
        return empty_data;
    }

    void init(std::string filepath) {
        std::vector<uint8_t> bytes = get_bytes_from_file(filepath);
        uint32_t pos = 0;

        auto read = [&](void* ptr, uint32_t num_bytes) {
            memcpy(ptr, &bytes[pos], num_bytes);
            pos += num_bytes;
        };

        read(&line_height, 1);

        uint16_t invisible_glyphs;
        read(&invisible_glyphs, 2);

        for(int i = 0; i < invisible_glyphs; ++i) {
            uint8_t id;
            Glyph_data data;
            data.visible = false;

            read(&id, 1);
            read(&data.stride, 1);

            glyph_map.insert({id, data});
        }

        uint16_t visible_glyphs;
        read(&visible_glyphs, 2);

        for(int i = 0; i < visible_glyphs; ++i) {
            uint8_t id;
            Glyph_data data;
            data.visible = true;

            read(&id, 1);
            read(&data.stride, 1);
            read(&data.size[0], 1);
            read(&data.size[1], 1);
            read(&data.pos_tex[0], 2);
            read(&data.pos_tex[1], 2);
            read(&data.pos_line[0], 1);
            read(&data.pos_line[1], 1);

            glyph_map.insert({id, data});
        }

        // missing placeholder
        Glyph_data data;
        data.visible = true;
        read(&data.stride, 1);
        read(&data.size[0], 1);
        read(&data.size[1], 1);
        read(&data.pos_tex[0], 2);
        read(&data.pos_tex[1], 2);
        read(&data.pos_line[0], 1);
        read(&data.pos_line[1], 1);

        empty_data = data;
    }

    Font(std::string filepath) {
        init(filepath);
    }

    Font() = default;
    Font(const Font& f) = default;
    Font(Font&& f) = default;
};

enum cursor_mode{CURSOR_CLICK, CURSOR_RESIZE_T, CURSOR_RESIZE_TR, CURSOR_RESIZE_R, CURSOR_RESIZE_BR, CURSOR_RESIZE_B, CURSOR_RESIZE_BL, CURSOR_RESIZE_L, CURSOR_RESIZE_TL, CURSOR_TEXT};

struct Window_state {
    vec2 position;
    vec2 size;
    std::string label;
    uint32_t priority = 0xFFFFFFFF;
    std::vector<UI_vertex> vertices;
    uint32_t scrollbar_width = 6;
    
    vec2 current_pos = vec2(0.0f);
    int scroll_pos = 0;
    ivec4 space;

    // text select
    std::string select_widget;
    int32_t select_anchor;
    int32_t select_position;
    int32_t select_position_line;
};

enum Alignment{ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT};

struct GUI_system : System {
    std::unordered_map<std::string, Font> fonts;
    std::vector<UI_vertex> vertices;

    std::unordered_map<std::string, Window_state> window_state = {{"", Window_state()}};

    cursor_mode cursor_mode = CURSOR_CLICK;

    uint32_t capture = 0xFFFFFFFF;
    std::string capture_window = "";
    std::string capture_widget = "";
    uint32_t capture_operation;
    float capture_position;

    std::string active_window = "";

    vec4 current_range = vec4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX);
    float widget_sep = 5;

    int icon_scale = 2;
    int text_scale = 1;
    vec4 text_range = vec4(0.0f);
    uint32_t text_lines = 0;
    std::vector<uint32_t> text_line_indices;
    std::vector<int> text_line_origins;

    Alignment alignment = ALIGN_LEFT;

    GUI_system() {
        fonts.emplace("default mono", Font("res/other resources/alter_mono.afont"));

        Signature s = ecs.update_signature<Camera>();
        ecs.update_signature<Transform>(s);
        collectors.push_back(Collector(s));
    }

    void insert_window(std::string window, Window_state state);
    void remove_window(std::string window);
    void make_priority(std::string window);
    void insert_vertices(std::vector<UI_vertex>& vertices);
    void insert_vertices();
    void window_capture();

    std::vector<UI_vertex> mesh_text(Font& f, std::string text, uint32_t width = 0xFFFFFFFF, ivec2 select_range = {-1, -1}, Alignment alignment = ALIGN_LEFT);

    void call();
    
    void toggle_button(vec2 position, vec2 size, ivec4 icon, bool& active);
    void button(std::string name, std::string text, vec2 size, bool& active);
    void window(std::string name, bool& close_window);
    void text(std::string name, std::string text, uint32_t width = 0xFFFFFFFF);
    void slider(std::string name, std::string text, ivec2 bounds, int& value, vec2 size, float slider_width);
    void slider(std::string name, std::string text, vec2 bounds, float& value, vec2 size, float slider_width);
};


std::string to_base(int32_t num, int base, bool use_i2 = false);

std::string to_base(int64_t num, int base, bool use_i2 = false);

std::string to_base(float num, int base, int max_float, bool use_i2 = false);

int from_base(std::string num, int base);