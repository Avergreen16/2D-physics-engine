#pragma once;
#include "wrapper.hpp"
#include "ecs.hpp"
#include "core.hpp"

#include <string>

const std::string integers = "0123456789\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89";  
const std::string integers_letters = "0123456789ABCDEF";

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

enum panel_split{SPLIT_X, SPLIT_Y, SPLIT_LEAF};

struct Panel_node {
    panel_split split = SPLIT_LEAF;
    int split_value;
    ivec2 current_pos;

    uint32_t parent = 0xFFFFFFFF;
    uint32_t self = 0;
    uint32_t child_a = 0xFFFFFFFF;
    uint32_t child_b = 0xFFFFFFFF;

    std::string name = "%EMPTY0";
    ivec4 space;
    ivec4 prev_space;
    int scroll_pos = 0;
    bool activated = false;

    bool scrollbar = false;
    bool bottom_lock = false;
};

struct Window_state {
    vec2 position;
    vec2 size;
    vec2 scrollbar_r = vec2(-1, -1);

    vec4 display_offsets;
    std::string label;
    uint32_t priority = 0xFFFFFFFF;
    std::vector<UI_vertex> vertices;
    uint32_t scrollbar_width = 6;
    
    ivec2 current_pos = ivec2(0);
    int scroll_pos = 0;
    ivec4 space;

    // text select
    std::string select_widget;
    int32_t select_anchor = 0;
    int32_t select_position = 0;
    int32_t select_position_line = 0;
    bool clear_selection = false;

    uint32_t current_panel = 0xFFFFFFFF;
    std::unordered_set<uint32_t> visited;
    std::vector<Panel_node> panels;

    bool panel_active(uint32_t id);
    bool panel_valid(uint32_t id);
};

struct Chat_message {
    std::string sender;
    std::string message;
};

struct Chat {
    std::string self_sender;
    std::string input;
    std::vector<Chat_message> messages;
};

enum Alignment{ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT};

struct GUI_system : System {
    std::unordered_map<std::string, Font> fonts;
    std::vector<UI_vertex> vertices;

    std::unordered_map<std::string, Window_state> window_state = {{"", Window_state()}};
    Chat chat;

    cursor_mode cursor_mode = CURSOR_CLICK;

    uint32_t capture = 0xFFFFFFFF;
    std::string capture_window = "";
    std::string capture_widget = "";
    uint32_t capture_operation;
    float capture_position;

    std::string active_window = "";
    std::string current_widget = "";

    vec4 current_range = vec4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX);
    float widget_sep = 5;

    int icon_scale = 2;
    int text_scale = 1;
    vec4 text_range = vec4(0.0f);
    uint32_t text_lines = 0;
    std::vector<uint32_t> text_line_indices;
    std::vector<int> text_line_origins;

    Alignment alignment = ALIGN_LEFT;
    Alignment text_alignment = ALIGN_LEFT;
    
    bool flag = false;

    bool hex_mode = true;

    GUI_system() {
        fonts.emplace("default mono", Font("res/other resources/alter_mono.afont"));

        Signature s = ecs.update_signature<Camera>();
        ecs.update_signature<Transform>(s);
        collectors.push_back(Collector(s));

        chat.self_sender = "avie";
    }

    void insert_window(std::string window, Window_state state);
    void remove_window(std::string window);
    void make_priority(std::string window);
    void insert_vertices(std::vector<UI_vertex>& vertices);
    void insert_vertices();
    void window_capture();

    std::vector<UI_vertex> mesh_text(Font& f, std::string text, uint32_t width = 0xFFFFFFFF, ivec2 select_range = {-1, -1}, Alignment alignment = ALIGN_LEFT, bool show_debug = false);

    void call();
    
    void toggle_button(vec2 position, vec2 size, ivec4 icon, bool& active);
    void button(vec2 position, vec2 size, ivec4 icon, bool& active);
    void button(std::string name, std::string text, vec2 size, bool& active);
    void window(std::string name, bool& close_window);
    void split_panel(std::string name, int& split, panel_split axis);
    void step_panel();
    void scrollbar_panel(uint32_t panel_id);
    void text(std::string name, std::string text, uint32_t width = 0xFFFFFFFF);
    void text(std::string name, std::string text, vec2 position, uint32_t width = 0xFFFFFFFF);
    void slider(std::string name, std::string text, ivec2 bounds, int& value, vec2 size, float slider_width);
    void slider(std::string name, std::string text, vec2 bounds, float& value, vec2 size, float slider_width, float precision = 0.0f);
    void chat_window();
    void text_input(std::string name, std::string& text, uint32_t width = 0xFFFFFFFF);
    
    ivec4 get_panel_subrange(uint32_t starting_index);
    void clear_panel(uint32_t root);

    void insert_cursor(std::string text, vec2 origin, bool show_debug = false);
    vec2 get_text_cursor_pos(std::string text, vec2 origin, bool show_debug = false);
    void text_navigate(std::string& text, bool edit = false, bool show_debug = false);

    ivec4 get_space();
    ivec2& get_position();
};


std::string to_base(int32_t num, int base, bool use_i2 = false);

std::string to_base(int64_t num, int base, bool use_i2 = false);

std::string to_base(float num, int base, int max_float, bool use_i2 = false);

int from_base(std::string num, int base);