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

    if(scroll_delta != 0.0f) {
        vec2 difference = camera_transform.position - world_cursor_pos;

        float factor = pow(1.25f, scroll_delta);

        difference /= factor;

        camera_transform.position = world_cursor_pos + difference;

        cc.scale = cc.scale * factor;
    }

    // translate
    if(translate) {
        if(key_map[GLFW_MOUSE_BUTTON_LEFT]) camera_transform.position -= camera_transform.orientation * (vec2(cursor_delta.x, cursor_delta.y) * 2.0f / float(core.window.screen_size.x) / cc.scale);
        else translate = false;
    }

    //Transform& tf = ecs.get_component<Transform>(tethered_object);

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
                    std::set<uint32_t> non_colliding;
                    Physics_system& ps = ecs.get_system<Physics_system>();

                    uint32_t iter_base = 2;
                    uint32_t iter_degree = 6;

                    uint32_t num_links = 24;

                    float sep = 0.025f;
                    vec2 size = vec2(0.333f, 1.0f);

                    Transform t;
                    t.position = world_cursor_pos;
                    //t.orientation = identity<mat2>();
                    vec2 up = normalize(t.position);
                    t.orientation = {up, vec2(-up.y, up.x)};
                    
                    Collider c;
                    c.vertices = {vec2(0, -(size.y - size.x) * 0.5f), vec2(0.0f, (size.y - size.x) * 0.5f)};
                    //c.vertices = {vec2(0.0f)};
                    c.radius = vec2(size.x * 0.5f);
                    c.mass = 0x40;
                    vec2 shift = Physics_system::calculate_inertia(c);
                    t.position += t.orientation * shift;
                    t.position += t.orientation * vec2(0, 1.0f);

                    Mesh m;
                    m.color = vec3(0.9f, 0.9f, 0.9f);
                    create_mesh(m, c.vertices, c.radius);

                    std::vector<uint32_t> chain_iter(iter_degree);
                    std::vector<std::vector<Constraint>> constraints(iter_degree + 1);
                    uint32_t prev_entity = NULL_ENTITY;
                    uint32_t first_entity = NULL_ENTITY;

                    for(int i = 0; i < num_links; ++i) {    
                        uint32_t capsule = ecs.insert_entity();
                        non_colliding.emplace(capsule);

                        ecs.insert_component(capsule, m);
                        ecs.insert_component(capsule, t);
                        ecs.insert_component(capsule, c);
                        
                        if(prev_entity != NULL_ENTITY) {
                            Transform& tf = ecs.get_component<Transform>(capsule);
                            Collider& c = ecs.get_component<Collider>(capsule);

                            Constraint constraint;
                            constraint.a = prev_entity;
                            constraint.b = capsule;
                            
                            pos_constraint pc;
                            pc.a = vec2(0, (size.y + sep) * 0.5f);
                            pc.b = vec2(0, -(size.y + sep) * 0.5f);
                            pc.vs = {vec2(1, 0), vec2(0, 1)};

                            constraint.pos.push_back(pc);

                            constraints[0].push_back(constraint);

                            //ps.constraints.push_back(constraint);
                        } else {
                            Constraint constraint;
                            constraint.a = capsule;
                            constraint.b = NULL_ENTITY;
                            
                            pos_constraint pc;
                            pc.a = vec2(0, -(size.y + sep) * 0.5f);
                            pc.b = world_cursor_pos;
                            pc.vs = {vec2(1, 0), vec2(0, 1)};

                            constraint.pos.push_back(pc);

                            //ps.constraints.push_back(constraint);

                            first_entity = capsule;

                            std::fill(chain_iter.begin(), chain_iter.end(), first_entity);
                        }

                        if(i > 0) {
                            for(int j = 0; j < iter_degree; ++j) {
                                uint32_t k = pow(iter_base, j + 1);

                                if(i % k == 0) {
                                    uint32_t& prev_chain = chain_iter[j];

                                    Constraint constraint;
                                    constraint.a = prev_chain;
                                    constraint.b = capsule;
                                    
                                    pos_constraint pc;
                                    pc.a = vec2(0, (size.y + sep) * 0.5f);
                                    pc.b = vec2(0.0f, -(size.y + sep) * 0.5f);
                                    pc.vs = {vec2(1, 0), vec2(0, 1)};
                                    pc.tolerance = (size.y + sep) * (k - 1);

                                    //constraint.weight = (j + 1.0f) * 0.75f + 1.0f;

                                    constraint.pos.push_back(pc);

                                    constraints[j + 1].push_back(constraint);
                                    //ps.constraints.push_back(constraint);

                                    prev_chain = capsule;
                                }
                            }
                        }

                        t.position += t.orientation * vec2(0, (size.y + sep));

                        prev_entity = capsule;
                    }

                    for(uint32_t link : non_colliding) {
                        Collider& c = ecs.get_component<Collider>(link);
                        c.non_colliding = non_colliding;
                    }

                    for(auto it = constraints.begin(); it != constraints.end(); ++it) {
                        for(auto& v : *it) {
                            ps.constraints.push_back(v);
                        }
                    }

                    /*
                    Constraint constraint;
                    constraint.a = first_entity;
                    constraint.b = prev_entity;
                    
                    pos_constraint pc;
                    pc.a = vec2(0, (size.y + sep) * 0.5f);
                    pc.b = vec2(0, -(size.y + sep) * 0.5f);
                    pc.vs = {vec2(1, 0), vec2(0, 1)};
                    pc.tolerance = (size.y + sep) * (num_links - 1);

                    constraint.pos.push_back(pc);

                    ps.constraints.push_back(constraint);*/

                    /*
                    Constraint constraint;
                    constraint.a = first_entity;
                    constraint.b = prev_entity;
                    
                    pos_constraint pc;
                    pc.a = vec2(0, -(size.y + sep) * 0.5f);
                    pc.b = vec2(0, -(size.y + sep) * 0.5f);
                    pc.vs = {vec2(1, 0), vec2(0, 1)};
                    pc.tolerance = (size.y + sep) * num_links;

                    constraint.pos.push_back(pc);

                    ps.constraints.push_back(constraint);
                    */

                    //ball on the end:

                    /**/

                    float weight_size = 2.0f;

                    uint32_t entity = ecs.insert_entity();
                    
                    Collider c2;
                    c2.vertices = {vec3(0.0f)};//{vec2(0, -1.5f), vec2(0, 1.5f)};
                    c2.radius = vec2(weight_size * 0.5f);
                    c2.mass = 0x100;
                    shift = Physics_system::calculate_inertia(c2);
                    t.position += t.orientation * shift;
                    t.position += t.orientation * vec2(0, (weight_size - (size.y + sep)) * 0.5f);

                    Mesh m2;
                    m2.color = vec3(0.9f, 0.9f, 0.9f);
                    create_mesh(m2, c2.vertices, c2.radius);

                    ecs.insert_component(entity, m2);
                    ecs.insert_component(entity, t);
                    ecs.insert_component(entity, c2);
                    if(prev_entity != NULL_ENTITY) {
                        Transform& tf = ecs.get_component<Transform>(entity);
                        Collider& c = ecs.get_component<Collider>(entity);

                        Constraint constraint;
                        constraint.a = entity;
                        constraint.b = prev_entity;
                        
                        pos_constraint pc;
                        pc.a = vec2(0, -(weight_size + sep) * 0.5f);
                        pc.b = vec2(0, (size.y + sep) * 0.5f);
                        pc.vs = {vec2(1, 0), vec2(0, 1)};

                        constraint.pos.push_back(pc);

                        rot_constraint rc;
                        rc.a = vec2(0, 1);
                        rc.b = vec2(0, 1);
                        
                        //constraint.rot.push_back(rc);

                        ps.constraints.push_back(constraint);
                    }

                    Constraint constraint;
                    constraint.a = first_entity;
                    constraint.b = entity;
                    
                    pos_constraint pc;
                    pc.a = vec2(0, (size.y + sep) * 0.5f);
                    pc.b = vec2(0.0f, 0.0f);
                    pc.vs = {vec2(1, 0), vec2(0, 1)};
                    pc.tolerance = (size.y + sep) * (num_links) + weight_size * 0.5f;

                    constraint.pos.push_back(pc);

                    ps.constraints.push_back(constraint);

                    /*
                    uint32_t entity = ecs.insert_entity();
                    
                    Collider c2;
                    c2.vertices = {vec2(0.0f, 1.5f), vec2(0.0f, -1.5f)};
                    c2.radius = vec2(0.5f);
                    c2.mass = 500;
                    c2.allow_gravity = true;
                    vec2 shift = Physics_system::calculate_inertia(c2);
                    Transform t;
                    t.position = world_cursor_pos;
                    t.orientation = identity<mat2>();

                    Mesh m2;
                    m2.color = vec3(0.9f, 0.9f, 0.9f);
                    create_mesh(m2, c2.vertices, c2.radius);

                    ecs.insert_component(entity, m2);
                    ecs.insert_component(entity, t);
                    ecs.insert_component(entity, c2);

                    Transform& tf = ecs.get_component<Transform>(entity);
                    Collider& c = ecs.get_component<Collider>(entity);
                    
                    Constraint constraint;
                    constraint.a = entity;
                    constraint.b = NULL_ENTITY;
                    
                    pos_constraint pc;
                    pc.a = vec2(0, -1.5f);
                    pc.b = world_cursor_pos;
                    pc.vs = {vec2(1, 0), vec2(0, 1)};
                    pc.tolerance = 4.0f;

                    constraint.pos.push_back(pc);

                    ps.constraints.push_back(constraint);

                    Mesh m;
                    uint32_t e = ecs.insert_entity();

                    Transform t2;
                    t2.orientation = identity<mat2>();
                    t2.position = world_cursor_pos;

                    ecs.insert_component(e, t2);

                    m.color = vec3(1.0f);
                    create_mesh(m, {vec2(0.0f)}, vec2(4.0f), false);

                    ecs.insert_component(e, m);

                    /*
                    Constraint constraint;
                    constraint.a = entity;
                    constraint.b = NULL_ENTITY;
                    
                    pos_constraint pc;
                    pc.a = vec2(0, -1.5f);
                    pc.b = world_cursor_pos;
                    pc.vs = {vec2(1, 0), vec2(0, 1)};
                    pc.tolerance = 1.0f;

                    constraint.pos.push_back(pc);

                    ps.constraints.push_back(constraint);
                    */
                } else {

                    auto insert_square = [&](vec2 pos, vec2 size, mat2 ori) {
                        uint32_t entity = ecs.insert_entity();

                        Transform t;
                        t.position = pos;
                        t.orientation = ori;

                        
                        Collider c;
                        c.vertices = {
                            vec2(-1, -1),
                            vec2(1, -1),
                            vec2(1, 1),
                            vec2(-1, 1)
                        };
                        for(vec2& v : c.vertices) v *= size * 0.5f;
                        c.radius = vec2(0.0f);
                        c.mass = size.x * size.y * 25.0f;

                        vec2 shift = Physics_system::calculate_inertia(c);
                        t.position += shift;

                        Mesh m;
                        m.color = vec3(0.35f);//get_color(abs(core.random())) * 0.7f + 0.3f;
                        create_mesh(m, c.vertices, c.radius);
                        
                        ecs.insert_component(entity, m);
                        ecs.insert_component(entity, t);
                        ecs.insert_component(entity, c);
                    };

                    auto insert_ellipse = [&](vec2 pos, vec2 size, mat2 ori) {
                        uint32_t entity = ecs.insert_entity();

                        Transform t;
                        t.position = pos;
                        t.orientation = ori;

                        
                        Collider c;
                        c.vertices = {
                            vec2(0.0f)
                        };
                        c.radius = size * 0.5f;
                        c.mass = size.x * size.y * 25.0f;

                        vec2 shift = Physics_system::calculate_inertia(c);
                        t.position += shift;

                        Mesh m;
                        m.color = vec3(0.35f);//get_color(abs(core.random())) * 0.7f + 0.3f;
                        create_mesh(m, c.vertices, c.radius);
                        
                        ecs.insert_component(entity, m);
                        ecs.insert_component(entity, t);
                        ecs.insert_component(entity, c);
                    };
                    
                    if(key_map[GLFW_KEY_LEFT_ALT]) {
                        mat2 orientation = identity<mat2>();
                        float floor = 64;
                        vec2 size = vec2(1.0f, 1.0f);

                        float elev = 0;
                        uint32_t stack_size = 32;

                        for(int y = stack_size; y >= 1; --y) {
                            for(int x = 0; x < y; ++x) {
                                float width = size.x * (y + 1);

                                float side = -width * 0.5f;

                                vec2 pos = vec2((x + 0.5f) * size.x + side, ((stack_size - y) + 0.5f) * size.y);
                                pos.y += floor;

                                insert_square(pos, size, orientation);
                            }
                        }
                    } else {
                        vec2 origin = world_cursor_pos;

                        mat2 orientation = rotate(core.random(), vec3(0.0f, 0.0f, 1.0f));

                        uint32_t square_size = 8;
                        float separation = 0.0625f;
                        vec2 max_size = vec2(1.0f, 1.0f);
                        vec2 min_size = vec2(0.75f, 0.75f);

                        for(int y = 0; y < square_size; ++y) {
                            for(int x = 0; x < square_size; ++x) {
                                vec2 width = float(square_size) * max_size + (square_size - 1) * separation;
                                vec2 pos = vec2(-width.x * 0.5f + max_size.x * (x + 0.5f) + separation * x, -width.y * 0.5f + max_size.y * (y + 0.5f) + separation * y);

                                insert_square(orientation * pos + origin, min_size + (max_size - min_size) * vec2(abs(core.random()), abs(core.random())), orientation);
                            }
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
                            
                            constraint.is_grab = true;
                            
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

    if(held_object != NULL_ENTITY) {
        if(!key_map[GLFW_MOUSE_BUTTON_LEFT]) {
            held_object = NULL_ENTITY;

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

    ++snum;
}