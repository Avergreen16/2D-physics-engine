#pragma once
#include "wrapper.hpp"
#include "random.hpp"

template<typename type>
type& get(std::unique_ptr<type>& a) {
    return (*a.get());
}

template<typename type_before, typename type_after>
type_after& convert(type_before& b) {
    return *(type_after*)&b;
}

std::ostream& operator<<(std::ostream& c, glm::vec3 v);

float smoothstep(float a);

double get_time();

double get_absolute_time();

time_t get_time_t();

struct Window {
    GLFWwindow* window;
    glm::ivec2 screen_size = {800, 600};
    glm::ivec2 viewport_size = {800, 600};

    bool fullscreen = false;
    glm::ivec4 prev_pos = {0, 0, 800, 600};

    Window() = default;

    Window(glm::ivec2 size);

    void init_callbacks();
};

enum event_types:uint16_t{KEY, MOUSE_BUTTON, SCROLL, CURSOR, TEXT};

struct Key_event {
    int key;
    int scancode;
    int action;
};

struct Mouse_button_event {
    int button;
    int action;
};

struct Scroll_event {
    double x;
    double y;
};

struct Cursor_event {
    double xpos;
    double ypos;
};

struct Text_event {
    uint32_t codepoint;
};

using Event = std::variant<Key_event, Mouse_button_event, Scroll_event, Cursor_event, Text_event>;

struct Model;
struct Mesh;

struct Core {
    bool game_running = true;
    Window window;
    
    double start_time;
    double prev_time;
    double current_time;

    std::unordered_map<std::string, std::shared_ptr<Shader>> shaders;
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;

    std::vector<Event> events;

    // input handling
    std::unordered_map<int, bool> key_map;
    std::unordered_set<GLenum> pressed_buttons;
    std::unordered_set<GLenum> released_buttons;
    std::unordered_set<GLenum> repeat_buttons;
    
    vec2 cursor_pos = vec2(0.0f);
    vec2 cursor_delta;
    float scroll_delta;

    std::string char_delta;

    // matrices
    glm::mat4 view = glm::identity<mat4>();
    glm::mat4 proj = glm::identity<mat4>();

    double delta_time = 1;

    // components
    Random random = Random(8762340);
    //Thread_pool thread_pool_b;

    Core(int num_threads);

    void init();

    double get_delta_time();

    bool time_step(double step);

    void handle_events();
};

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

void character_callback(GLFWwindow* window, unsigned int codepoint);

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

struct aiMesh;
struct aiNode;
struct aiScene;
struct aiAnimation;

extern Core core;