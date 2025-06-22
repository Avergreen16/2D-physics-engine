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

vec3 get_color(float a) {
    float aa = a * 6;
    float c = fract(aa);

    vec3 color;
    if(aa < 1) {
        color = vec3(1.0f, c, 0.0f);
    } else if(aa < 2) {
        color = vec3(1.0f - c, 1.0f, 0.0f);
    } else if(aa < 3) {
        color = vec3(0.0f, 1.0f, c);
    } else if(aa < 4) {
        color = vec3(0.0f, 1.0f - c, 1.0f);
    } else if(aa < 5) {
        color = vec3(c, 0.0f, 1.0f);
    } else {
        color = vec3(1.0f, 0.0f, 1.0f - c);   
    }

    return color;
}

void Input_system::call() {
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    std::set<GLenum> pressed_buttons;
    std::set<GLenum> released_buttons;
    std::set<GLenum> repeat_buttons;
    cursor_delta = glm::vec2(0.0f);
    scroll_delta = 0.0f;
    click = false;
    char_delta = "";
    backspace = false;
    arrow_delta = 0;

    uint32_t camera = *collectors[0].entities.begin();
    Camera& cc = ecs.get_component<Camera>(camera);
    Transform& camera_transform = ecs.get_component<Transform>(camera);

    world_cursor_pos = (cursor_pos - (vec2(core.window.screen_size) * 0.5f)) * 2.0f / float(core.window.screen_size.x);
    world_cursor_pos /= cc.scale;
    world_cursor_pos = camera_transform.orientation * world_cursor_pos;
    world_cursor_pos += camera_transform.position;

    vec2 world_cursor_delta;

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
    
    vec2 new_world_cursor_pos = (cursor_pos - (vec2(core.window.screen_size) * 0.5f)) * 2.0f / float(core.window.screen_size.x);
    new_world_cursor_pos /= cc.scale;
    new_world_cursor_pos = camera_transform.orientation * new_world_cursor_pos;
    new_world_cursor_pos += camera_transform.position;

    world_cursor_delta = new_world_cursor_pos - world_cursor_pos;

    // scroll

    vec2 difference = camera_transform.position - world_cursor_pos;

    float factor = pow(1.25f, scroll_delta);

    difference /= factor;

    camera_transform.position = world_cursor_pos + difference;

    cc.scale = cc.scale * factor;

    // translate
    if(translate) {
        if(key_map[GLFW_MOUSE_BUTTON_LEFT]) camera_transform.position -= camera_transform.orientation * (vec2(cursor_delta.x, cursor_delta.y) * 2.0f / float(core.window.screen_size.x) / cc.scale);
        else translate = false;
    }
    //tf.position = world_cursor_pos;
    Transform& tf = ecs.get_component<Transform>(tethered_object);

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
            if(!gui_system.cursor_captured) {
                Physics_system& ps = ecs.get_system<Physics_system>();

                for(uint32_t entity : ps.collectors[0].entities) {
                    Transform& tf = ecs.get_component<Transform>(entity);
                    Collider& c = ecs.get_component<Collider>(entity);

                    vec2 rel_point = transpose(tf.orientation) * (world_cursor_pos - tf.position);

                    if(ps.collision_point(c, rel_point)) {
                        held_object = entity;
                    }
                }
            }
        } else if(key == GLFW_MOUSE_BUTTON_LEFT) {
            click = true;

            if(!gui_system.cursor_captured) {
                translate = true;
            }
        } else if(key == GLFW_KEY_BACKSPACE) {
            backspace = true;
        } else if(key == GLFW_KEY_LEFT) {
            --arrow_delta;
        } else if(key == GLFW_KEY_RIGHT) {
            ++arrow_delta;
        } else if(key == GLFW_KEY_EQUAL) {
            ++visualizer.steps;
        } else if(key == GLFW_KEY_MINUS) {
            --visualizer.steps;
        }
    }
    
    for(GLenum key : repeat_buttons) {
        if(key == GLFW_KEY_BACKSPACE) {
            backspace = true;
        } else if(key == GLFW_KEY_LEFT) {
            --arrow_delta;
        } else if(key == GLFW_KEY_RIGHT) {
            ++arrow_delta;
        }
    }

    Physics_system& ps = ecs.get_system<Physics_system>();
    
    float rot = 0.0f;

    if(key_map[GLFW_KEY_Q]) {
        rot += 1.0f;
    }
    if(key_map[GLFW_KEY_E]) {
        rot -= 1.0f;
    }

    bool capture_rot = false;

    if(held_object != 0xFFFFFFFF) {
        if(!key_map[GLFW_MOUSE_BUTTON_RIGHT]) {
            held_object = 0xFFFFFFFF;
        } else {
            capture_rot = true;
            Transform& t = ecs.get_component<Transform>(held_object);

            mat2 rot_mat = mat2(glm::rotate(identity<mat3>(), float(rot * 2.0f * M_PI / 4.0f * float(core.delta_time))));
            vec2 diff = t.position - world_cursor_pos;
            vec2 new_diff = rot_mat * diff;
            t.position = new_diff + world_cursor_pos;
            t.orientation = rot_mat * t.orientation;

            t.position += world_cursor_delta;
        }
    }

    if(!capture_rot && rot != 0.0f) {
        mat2 rot_mat = mat2(glm::rotate(identity<mat3>(), float(rot * 2.0f * M_PI / 4.0f * float(core.delta_time))));

        vec2 difference = camera_transform.position - world_cursor_pos;
        difference = rot_mat * difference;
        camera_transform.position = difference + world_cursor_pos;

        camera_transform.orientation = rot_mat * camera_transform.orientation;
    }

    visualizer.step_collisions();
}