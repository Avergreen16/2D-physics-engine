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
                if(key_map[GLFW_KEY_LEFT_SHIFT]) {
                    /*uint32_t num_links = 1;
                    float len = 3.5f;
                    float sep = 0.1f;
                    float radius = 0.5f;

                    std::vector<vec2> vertices = {vec2(0, -(len * 0.5f - radius)), vec2(0, len * 0.5f - radius)};

                    Physics_system& ps = ecs.get_system<Physics_system>();

                    uint32_t prev_shape = 0xFFFFFFFF;

                    vec2 dir = normalize(world_cursor_pos);
                    vec2 dir_2 = vec2(dir.y, -dir.x);
                    mat2 ori = mat2(dir, dir_2);

                    for(int i = 0; i < num_links; ++i) {
                        uint32_t entity = ecs.insert_entity();
                        
                        Transform t;
                        t.position = world_cursor_pos + dir_2 * float(len * 0.5 + (len + sep) * i);
                        t.orientation = ori;
                        
                        Collider c;
                        c.vertices = vertices;
                        c.radius = radius;
                        c.mass = len * radius * 2.0f;
                        vec2 shift = Physics_system::calculate_inertia(c);
                        t.position += ori * shift;

                        Mesh m;
                        m.color = get_color(abs(core.random())) * 0.7f + 0.3f;
                        create_mesh(m, c.vertices, c.radius);
                        
                        ecs.insert_component(entity, m);
                        ecs.insert_component(entity, t);
                        ecs.insert_component(entity, c);

                        if(prev_shape == 0xFFFFFFFF) {
                        } else {
                            Position_constraint constraint;
                            constraint.a = prev_shape;
                            constraint.pa = vec2(0, (len + sep) * 0.5f);
                            constraint.b = entity;
                            constraint.pb = vec2(0, -(len + sep) * 0.5f);

                            constraint.dir = vec2(1, 0);
                            ps.position_constraints.push_back(constraint);
                            
                            constraint.dir = vec2(0, 1);
                            ps.position_constraints.push_back(constraint);
                        }

                        prev_shape = entity;
                    }*/

                    uint32_t num_links = 12;

                    float sep = 0.125f;

                    Physics_system& ps = ecs.get_system<Physics_system>();

                    Transform t;
                    t.position = world_cursor_pos;
                    //t.orientation = identity<mat2>();
                    vec2 up = normalize(t.position);
                    t.orientation = {up, vec2(up.y, -up.x)};
                    
                    Collider c;
                    c.vertices = {vec2(0, -1.5f), vec2(0, 1.5f)};
                    c.radius = vec2(0.5f);
                    c.mass = 100;
                    vec2 shift = Physics_system::calculate_inertia(c);
                    t.position += t.orientation * shift;
                    t.position += t.orientation * vec2(0, 2.0f);

                    Mesh m;
                    m.color = vec3(0.9f, 0.9f, 0.9f);
                    create_mesh(m, c.vertices, c.radius);

                    uint32_t prev_entity = 0xFFFFFFFF;

                    for(int i = 0; i < num_links; ++i) {    
                        uint32_t capsule = ecs.insert_entity();

                        ecs.insert_component(capsule, m);
                        ecs.insert_component(capsule, t);
                        ecs.insert_component(capsule, c);
                        
                        if(prev_entity != 0xFFFFFFFF) {
                            Transform& tf = ecs.get_component<Transform>(capsule);
                            Collider& c = ecs.get_component<Collider>(capsule);

                            Constraint constraint;
                            constraint.a = capsule;
                            constraint.b = prev_entity;
                            
                            pos_constraint pc;
                            pc.a = vec2(0, -2.0f - sep * 0.5f);
                            pc.b = vec2(0, 2.0f + sep * 0.5f);
                            pc.vs = {vec2(1, 0), vec2(0, 1)};

                            constraint.pos.push_back(pc);

                            rot_constraint rc;
                            rc.a = vec2(0, 1);
                            rc.b = vec2(0, 1);
                            
                            //constraint.rot.push_back(rc);

                            ps.constraints.push_back(constraint);
                        }

                        t.position += t.orientation * vec2(0, 4 + sep);

                        prev_entity = capsule;
                    }

                    // base

                    /*uint32_t base = ecs.insert_entity();

                    t.position = world_cursor_pos;
                    t.orientation = identity<mat2>();
                    
                    Collider c2;
                    c2.vertices = {vec2(-1.0f, -1.0f), vec2(1.0f, -1.0f), vec2(1.0f, 1.0f), vec2(-1.0f, 1.0f)};
                    for(vec2& v : c2.vertices) v *= vec2(0.75f, 0.5f);

                    c2.radius = 0.0f;
                    c2.mass = 200;
                    shift = Physics_system::calculate_inertia(c2);
                    t.position += t.orientation * shift;
                    t.position += t.orientation * vec2(0, -0.5f);

                    Mesh m2;
                    m2.color = vec3(0.9f, 0.9f, 0.9f);
                    create_mesh(m2, c2.vertices, c2.radius);
                    
                    ecs.insert_component(base, m2);
                    ecs.insert_component(base, t);
                    ecs.insert_component(base, c2);*/
                } else {
                    ivec2 start_pos = world_cursor_pos;
                    ivec2 shape_matrix = ivec2(16);
                    float separation = 1.25f;
                    vec2 max_dim = vec2(1.0f);
                    vec2 min_dim = vec2(1.0f);

                    if(key_map[GLFW_KEY_M]){
                        shape_matrix = ivec2(3);
                        separation *= 8.0f;
                        max_dim *= 8.0f;
                        min_dim *= 8.0f;
                    }

                    for(int x = 0; x < shape_matrix.x; ++x) {
                        for(int y = 0; y < shape_matrix.y; ++y) {
                            vec2 position = world_cursor_pos + (-(vec2(shape_matrix - 1) / 2.0f) + vec2(x, y)) * separation;
                            
                            uint32_t entity = ecs.insert_entity();
                            
                            Transform t;
                            t.position = position;
                            t.orientation = identity<mat2>();//mat2(rotate(float(M_PI) * (core.random() * 1.0f), vec3(0.0f, 0.0f, 1.0f)));
                            Collider c;
                            vec2 size = vec2(core.random() * 0.5f + 0.5f, core.random() * 0.5f + 0.5f) * (max_dim - min_dim) + min_dim;
                            //c.allow_gravity = false;

                            std::vector<vec2> square = {
                                vec2(-1, -1),
                                vec2(1, -1),
                                vec2(1, 1),
                                vec2(-1, 1)
                            };

                            if(core.random() < 0.0f || true) {
                                c.vertices = square;
                                for(vec2& v : c.vertices) v *= size * 0.5f;
                                c.radius = vec2(0.0f);
                                c.mass = size.x * size.y * 25.0f;
                            } else {
                                c.vertices = {vec2(0.0f)};
                                c.radius = size * 0.5f;
                                c.mass = size.x * size.y * 25.0f;
                            }

                            vec2 shift = Physics_system::calculate_inertia(c);
                            //t.position += shift;

                            /*int num_sides = core.random.next() % 5 + 3;
                            float radius = abs(core.random());
                            radius = radius * 0.3f + 0.2f;
                            if(key_map[GLFW_KEY_LEFT_CONTROL]) radius *= 10.0f;
                            float jitter = (2.0f * M_PI) / num_sides * 0.4f;

                            c.radius = 0.0f;//(core.random() * 0.5f + 0.5f) * radius * 0.4f;
                            
                            for(int i = 0; i < num_sides; ++i) {
                                float angle = float(i) / num_sides * (2 * M_PI) + jitter * core.random();
                                vec2 vertex = vec2(cos(angle), sin(angle)) * radius;
                                c.vertices.push_back(vertex);
                            }
                            c.mass = pow(radius * 2, 2);
                            vec2 shift = Physics_system::calculate_inertia(c);
                            t.position += shift;*/

                            Mesh m;
                            m.color = get_color(abs(core.random())) * 0.7f + 0.3f;
                            create_mesh(m, c.vertices, c.radius);

                            
                            ecs.insert_component(entity, m);
                            ecs.insert_component(entity, t);
                            ecs.insert_component(entity, c);
                        }
                    }
                }
            }
        } else if(key == GLFW_MOUSE_BUTTON_LEFT) {
            click = true;

            if(!gui_system.cursor_captured) {
                if(key_map[GLFW_KEY_LEFT_SHIFT]) {
                    Physics_system& ps = ecs.get_system<Physics_system>();

                    for(uint32_t entity : ps.collectors[0].entities) {
                        Transform& tf = ecs.get_component<Transform>(entity);
                        Collider& c = ecs.get_component<Collider>(entity);

                        vec2 rel_point = transpose(tf.orientation) * (world_cursor_pos - tf.position);

                        if(ps.collision_point(c, rel_point)) {
                            held_object = entity;
                            held_constraint = ps.constraints.size();

                            Constraint constraint;
                            constraint.a = entity;
                            
                            pos_constraint pc;
                            pc.a = rel_point;
                            pc.b = world_cursor_pos;
                            pc.vs = {vec2(1, 0), vec2(0, 1)};

                            constraint.pos.push_back(pc);

                            ps.constraints.push_back(constraint);
                        }
                    }
                } else {
                    translate = true;
                }
            }
        } else if(key == GLFW_KEY_BACKSPACE) {
            backspace = true;
        } else if(key == GLFW_KEY_LEFT) {
            --arrow_delta;
        } else if(key == GLFW_KEY_RIGHT) {
            ++arrow_delta;
        } else if(key == GLFW_KEY_0) {
            profiler.output();
            profiler.restart();
            profiler2.output();
            profiler2.restart();
        } else if(key == GLFW_KEY_P) {
            int b[N] = {1, 0, 0, 0, 1, 0, 1, 1};

            xsimd::batch<int> bi;
            bi = xsimd::load_unaligned(b);

            auto bbi = bi != 0;

            auto bbf = bint_to_bfloat(bbi);

            xsimd::batch<float> tmp_batch = static_cast<xsimd::batch<float>>(bbf);
            float b2[N];
            tmp_batch.store_unaligned(b2);

            for(int i = 0; i < N; ++i) {
                std::cout << b[i] << " " << b2[i] << "\n";
            }
        } else if(key == GLFW_KEY_F5) {
            debug_physics = !debug_physics;
        } else if(key == GLFW_KEY_F3) {
            debug_mode = !debug_mode;
        } else if(key == GLFW_KEY_F4) {
            use_skin = !use_skin;
        } else if(key == GLFW_KEY_EQUAL) {
            if(debug_physics) {
                Physics_system& ps = ecs.get_system<Physics_system>();

                ps.physics_loop();
            }
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

    if(held_object != 0xFFFFFFFF) {
        if(!key_map[GLFW_MOUSE_BUTTON_LEFT]) {
            held_object = 0xFFFFFFFF;

            ps.constraints.erase(ps.constraints.begin() + held_constraint);
        } else {
            Constraint& c = ps.constraints[held_constraint];
            c.pos[0].b = world_cursor_pos;
        }
    }

    float rot = 0.0f;

    if(key_map[GLFW_KEY_Q]) {
        rot += 1.0f;
    }
    if(key_map[GLFW_KEY_E]) {
        rot -= 1.0f;
    }

    if(rot != 0.0f) {
        mat2 rot_mat = mat2(glm::rotate(identity<mat3>(), float(rot * 2.0f * M_PI / 4.0f * float(core.delta_time))));

        vec2 difference = camera_transform.position - world_cursor_pos;
        difference = rot_mat * difference;
        camera_transform.position = difference + world_cursor_pos;

        camera_transform.orientation = rot_mat * camera_transform.orientation;
    }
}