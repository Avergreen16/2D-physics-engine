#include "input.hpp"
#include "core.hpp"
#include "render.hpp"
#include "physics.hpp"

#include "stb_image.h"
#include "stb_image_write.h"

Input_system::Input_system() {
    Signature s = ecs.update_signature<Camera>();
    ecs.update_signature<Transform>(s);
    collectors.push_back(Collector(s));

    //s = ecs.update_signature<Collider>();
    ecs.update_signature<Transform>(s);
    collectors.push_back(Collector(s, false));
}

void Input_system::call() {
    std::set<GLenum> pressed_buttons;
    std::set<GLenum> released_buttons;
    glm::vec2 cursor_delta = glm::vec2(0.0f);

    scroll_delta = 0.0f;

    uint32_t camera = *collectors[0].entities.begin();
    Camera& cc = ecs.get_component<Camera>(camera);
    Transform& camera_transform = ecs.get_component<Transform>(camera);

    world_cursor_pos = (cursor_pos - (vec2(core.window.screen_size) * 0.5f)) * 2.0f / float(core.window.screen_size.x);
    world_cursor_pos /= cc.scale;
    world_cursor_pos += camera_transform.position;

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

                break;
            }
            case 2: {
                Scroll_event& s = std::get<Scroll_event>(e);
                scroll_delta = s.y;
                std::cout << s.y << "\n";

                vec2 difference = camera_transform.position - world_cursor_pos;

                float factor = pow(1.25f, scroll_delta);

                difference /= factor;

                camera_transform.position = world_cursor_pos + difference;

                cc.scale = cc.scale * factor;

                break;
            }
            case 3: {
                Cursor_event& c = std::get<Cursor_event>(e);
                glm::vec2 new_cursor_pos = {c.xpos, core.window.screen_size.y - c.ypos - 1};
                cursor_delta = new_cursor_pos - cursor_pos;
                cursor_pos = new_cursor_pos;

                if(key_map[GLFW_MOUSE_BUTTON_LEFT]) camera_transform.position -= vec2(cursor_delta.x, cursor_delta.y) * 2.0f / float(core.window.screen_size.x) / cc.scale;

                break;
            }
        }
    }

    Transform& tf = ecs.get_component<Transform>(tethered_object);
    //tf.position = world_cursor_pos;

    for(GLenum key : pressed_buttons) {
        if(key == GLFW_KEY_F11) {
            core.window.fullscreen = !core.window.fullscreen;

            if(core.window.fullscreen) {
                GLFWmonitor* monitor = glfwGetPrimaryMonitor();

                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                core.window.prev_pos.z = core.window.screen_size.x;
                core.window.prev_pos.w = core.window.screen_size.y;

                // switch to full screen
                glfwSetWindowMonitor(core.window.window, monitor, 0, 0, mode->width, mode->height, 0);
            } else {
                // restore last window size and position
                glfwSetWindowMonitor(core.window.window, nullptr,  core.window.prev_pos.x, core.window.prev_pos.y, core.window.prev_pos.z, core.window.prev_pos.w, 0 );
            }
        } else if(key == GLFW_MOUSE_BUTTON_RIGHT) {
            uint32_t entity = ecs.insert_entity();
            
            Transform t;
            t.position = world_cursor_pos;
            t.orientation = mat2(rotate(float(M_PI) * core.random(), vec3(0.0f, 0.0f, 1.0f)));
            Collider c;

            int num_sides = core.random.next() % 7 + 3;
            float radius = core.random() * 0.5f + 0.5f;
            radius = radius * 0.6f + 0.2f;
            float jitter = (2.0f * M_PI) / num_sides * 0.4f;

            c.radius = (core.random() * 0.5f + 0.5f) * radius * 0.4f;
            
            for(int i = 0; i < num_sides; ++i) {
                float angle = float(i) / num_sides * (2 * M_PI) + jitter * core.random();
                vec2 vertex = vec2(cos(angle), sin(angle)) * radius;
                c.vertices.push_back(vertex);
            }
            c.mass = radius * radius;
            vec2 shift = Physics_system::calculate_inertia(c);
            t.position += shift;

            Mesh m;
            create_mesh(m, c.vertices, c.radius);

            
            ecs.insert_component(entity, m);
            ecs.insert_component(entity, t);
            ecs.insert_component(entity, c);
        }
    }
}