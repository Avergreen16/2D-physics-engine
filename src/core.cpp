#include "core.hpp"

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
    prev_time = current_time;
    current_time = get_time();
    double delta_time = current_time - prev_time;

    return delta_time;
}

bool Core::time_step(double step) {
    double d_prev = prev_time / step;
    double d_current = current_time / step;

    return floor(d_prev) != floor(d_current);
}

Core::Core(int num_threads) {};

Core core(10);

void Core::handle_events() {
    cursor_delta = glm::vec2(0.0f);
    scroll_delta = 0.0f;
    char_delta = "";

    pressed_buttons.clear();
    repeat_buttons.clear();
    released_buttons.clear();

    for(Event& e : core.events) {
        switch(e.index()) {
            case 0: {
                Key_event& k = std::get<Key_event>(e);

                if(k.action == GLFW_PRESS) {
                    pressed_buttons.emplace(k.key);
                    key_map[k.key] = true;
                }
                if(k.action == GLFW_RELEASE) {
                    released_buttons.emplace(k.key);
                    key_map[k.key] = false;
                }
                if(k.action == GLFW_REPEAT) {
                    repeat_buttons.emplace(k.key);
                }

                break;
            }
            case 1: {
                Mouse_button_event& m = std::get<Mouse_button_event>(e);
                
                if(m.action == GLFW_PRESS) {
                    pressed_buttons.emplace(m.button);
                    key_map[m.button] = true;
                }
                if(m.action == GLFW_RELEASE) {
                    released_buttons.emplace(m.button);
                    key_map[m.button] = false;
                }
                if(m.action == GLFW_REPEAT) {
                    repeat_buttons.emplace(m.button);
                }

                break;
            }
            case 2: {
                Scroll_event& s = std::get<Scroll_event>(e);
                scroll_delta += s.y;

                break;
            }
            case 3: {
                Cursor_event& c = std::get<Cursor_event>(e);
                glm::vec2 new_cursor_pos = {c.xpos, core.window.screen_size.y - c.ypos - 1};
                cursor_delta += new_cursor_pos - cursor_pos;
                cursor_pos = new_cursor_pos;

                break;
            } case 4: {
                Text_event& t = std::get<Text_event>(e);

                char c = t.codepoint;
                char_delta += c;

                break;
            }
        }
    }
}