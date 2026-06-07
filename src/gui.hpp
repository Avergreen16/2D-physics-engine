#pragma once;
#include "wrapper.hpp"
#include "ecs.hpp"
#include "core.hpp"
#include "utility.hpp"

#include <string>

enum cursor_mode{CURSOR_CLICK, CURSOR_RESIZE_T, CURSOR_RESIZE_TR, CURSOR_RESIZE_R, CURSOR_RESIZE_BR, CURSOR_RESIZE_B, CURSOR_RESIZE_BL, CURSOR_RESIZE_L, CURSOR_RESIZE_TL, CURSOR_TEXT};
enum panel_split{SPLIT_X, SPLIT_Y, SPLIT_LEAF};
const uint64_t NULL_WIDGET = 0xFFFFFFFFFFFFFFFF;

struct UI_vertex {
    vec2 pos;
    vec2 tex_pos;
    vec4 color = vec4(1.0f);
    vec4 range = vec4(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX);
    uint data = 0;
};

// font

struct Glyph_data {
    std::vector<uint8_t> bitmap;
    bool visible = true;

    ivec2 size;
    ivec2 offset;
    int advance;
    
    ivec2 pos_tex;
};

struct Font {
    int line_height;
    Glyph_data empty_data = {{}, false, {0, 0}, {0, 0}, 0, {0, 0}};
    std::map<uint32_t, Glyph_data> glyph_map;

    Glyph_data& at(uint32_t key);

    void init(std::string filepath);

    Font(std::string filepath);

    Font() = default;
    Font(const Font& f) = default;
    Font(Font&& f) = default;
};

enum ALIGNMENT{ALIGNMENT_LEFT, ALIGNMENT_CENTER, ALIGNMENT_RIGHT};
std::vector<UI_vertex> mesh_text(Font& f, std::string text, uint32_t text_size, uint32_t width = 0xFFFFFFFF, ivec2 select_range = {-1, -1}, ALIGNMENT alignment = ALIGNMENT_LEFT, bool show_debug = false);

// 
enum LAYOUT_MODE{LM_VOID, LM_ROW, LM_COLUMN, LM_GRID};
enum POSITION_MODE{PM_STATIC, PM_TOP_LEFT, PM_TOP_RIGHT, PM_BOTTOM_LEFT, PM_BOTTOM_RIGHT, PM_TOP_CENTER, PM_BOTTOM_CENTER, PM_CENTER_LEFT, PM_CENTER_RIGHT, PM_CENTER, PM_VOID};
enum SIZE_MODE{SM_STATIC, SM_FILL, SM_SURROUND};

struct Widget {
    uint64_t self;
    bool flag = false;

    vec2 position;
    vec2 size;
    vec2 buffer = vec2(0.0f);
    
    vec4 child_region = vec4(0.0f);
    vec2 child_offset = vec2(0.0f);
    vec4 range;
    std::vector<UI_vertex> vertices_before;
    std::vector<UI_vertex> vertices_after;

    std::function<float(std::unique_ptr<Widget>&)> get_height = [](std::unique_ptr<Widget>& w) {
        return w->size.y;
    };

    //

    LAYOUT_MODE layout_mode = LM_VOID;
    POSITION_MODE position_mode = PM_STATIC;
    SIZE_MODE size_mode = SM_STATIC;
    SIZE_MODE size_mode_y = SM_STATIC;

    //

    vec2 rel_position = vec2(0.0f);
    float min_width;
    float max_width;
    float weight_width = 1.0f;
    float min_height;
    float max_height;
    float weight_height = 1.0f;

    vec4 available_space;
    bool dirty = true;

    uint64_t parent = NULL_WIDGET;
    std::vector<uint64_t> children;
    vec2 sep = vec2(0.0f);

    virtual void handle_inputs() {};
    virtual void mesh() {};
    virtual void get_y() {};
    
    virtual void on_measure() {};
    virtual void on_transform() {};
    virtual void on_solve_x() {};
    virtual void on_solve_y() {};
    virtual void on_place() {};
};

struct GUI_system : System {
    std::unordered_map<std::string, Font> fonts;
    cursor_mode cursor_mode = CURSOR_CLICK;
    std::vector<UI_vertex> vertices;
    bool hex_mode = true;

    uint64_t next_id = 0;
    std::map<uint64_t, std::unique_ptr<Widget>> widgets;
    
    uint64_t capture_id = NULL_WIDGET;
    uint64_t capture_operation;

    uint64_t current_widget = NULL_WIDGET;

    POSITION_MODE active_position = PM_TOP_LEFT;

    GUI_system() = default;
    void init();

    template<typename Type>
    void insert_widget(Type widget, bool step = false);
    void step();
    void position(POSITION_MODE mode);
    void make_dirty(uint64_t root);

    float get_width_x(float x, uint64_t id, float weight);
    float assign_width_x(float x, uint64_t id, float weight, std::vector<std::pair<uint64_t, float>>& shadowed_children);
    
    float get_width_y(float x, uint64_t id, float weight);
    float assign_width_y(float x, uint64_t id, float weight, std::vector<std::pair<uint64_t, float>>& shadowed_children);

    void do_layout();
    vec4 get_range(uint64_t v);
    void call();

    void propagate_up(uint64_t start, std::function<void(std::unique_ptr<Widget>&)> func);
    void propagate_down(uint64_t start, std::function<bool(std::unique_ptr<Widget>&)> func);
};

template<typename Type>
void GUI_system::insert_widget(Type widget, bool step) {
    widget.parent = current_widget;
    widget.self = next_id;

    if(current_widget != NULL_WIDGET) widgets[current_widget]->children.push_back(next_id);

    if(step) current_widget = next_id;

    widgets.emplace(next_id, std::make_unique<Type>(widget));

    ++next_id;
}

//

struct Window_Widget : Widget {
    void handle_inputs();
    void mesh();
    //void set_child_offset();

    static void insert(vec2 size, vec2 position);
};

struct Debug_Widget : Widget {
    vec3 color;
    
    void mesh();

    static void insert(vec2 size, float max_width, vec3 color);
};

struct Row_Widget : Widget {
    static void insert(vec2 border);
};

struct Column_Widget : Widget {
    static void insert(vec2 border);
};

struct Grid_Widget : Widget {
    uint32_t columns;
    static void insert(uint32_t num_columns, vec2 border);
};

struct Panel_Constraint {
    float value;
    bool fill = false;
};

struct Split_Widget : Widget {
    std::vector<Panel_Constraint> constraints;

    void handle_inputs();
    void on_measure();
    void on_transform();

    static void insert(LAYOUT_MODE layout, std::vector<Panel_Constraint> constraints);
};

struct Panel_Widget : Widget {
    float total_scrollable = 0.0f;

    void handle_inputs();
    void mesh();
    void on_transform();
    void on_place();

    static void insert();
};

struct Text_Widget : Widget {
    std::string text;
    uint32_t text_size = 1;
    float text_width = 0.0f;
    ALIGNMENT alignment = ALIGNMENT_LEFT;

    std::function<void(std::string&)> callback = [](std::string& str) {};

    void handle_inputs();
    void mesh();
    void get_y();
    void set_str(std::string str);

    static void insert(std::string str, ALIGNMENT alg, std::function<void(std::string&)> callback_ = [](std::string& str) {});
};
