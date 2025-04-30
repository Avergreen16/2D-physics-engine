#include "core.hpp"

double get_time() {
    return (double)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1000000;
}

time_t get_time_t() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    core.events.push_back(Key_event{key, scancode, action});
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    core.events.push_back(Cursor_event{xpos, ypos});
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    core.events.push_back(Mouse_button_event{button, action});
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    core.events.push_back(Scroll_event{xoffset, yoffset});
}

void character_callback(GLFWwindow* window, unsigned int codepoint) {
    core.events.push_back(Text_event{codepoint});
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    core.window.screen_size.x = width;
    core.window.screen_size.y = height;
    core.window.viewport_size.x = width + 1 * (width & 1);
    core.window.viewport_size.y = height + 1 * (height & 1);

    glViewport(0, 0, core.window.viewport_size.x, core.window.viewport_size.y);

    //core.render();
}


Window::Window(glm::ivec2 size) {
    screen_size = size;
    viewport_size = {size.x + 1 * (size.x & 1), size.y + 1 * (size.y & 1)};
    
    window = glfwCreateWindow(size.x, size.y, "Infinity pre-alpha", NULL, NULL);
    glfwMakeContextCurrent(window);

    if(!gladLoadGL()) {
        std::cout << "ERROR: GLAD failed to load.\n";
        glfwTerminate();
    }

    // init glad and set viewport
    
    glViewport(0, 0, size.x, size.y);
}

void Window::init_callbacks() {
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCharCallback(window, character_callback);
}

double Core::get_delta_time() {
    double current_time = get_time();
    double delta_time = current_time - prev_time;
    prev_time = current_time;

    return delta_time;
}

std::ostream& operator<<(std::ostream& c, glm::vec3 v) {
    c << v.x << " " << v.y << " " << v.z;

    return c;
}

Core::Core(int num_threads) {};

Core core(10);