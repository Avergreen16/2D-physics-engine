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

    uint32_t camera = *collectors[0].entities.begin();
    Camera& cc = ecs.get_component<Camera>(camera);
    Transform& camera_transform = ecs.get_component<Transform>(camera);

    world_cursor_pos = (core.cursor_pos - (vec2(core.window.screen_size) * 0.5f)) * 2.0f / float(core.window.screen_size.x);
    world_cursor_pos /= cc.scale;
    world_cursor_pos = camera_transform.orientation * world_cursor_pos;
    world_cursor_pos += camera_transform.position;

    // scroll

    if(core.scroll_delta != 0.0f) {
        if(gui_system.capture_window == "") {
            vec2 difference = camera_transform.position - world_cursor_pos;

            float factor = pow(1.25f, core.scroll_delta);

            difference /= factor;

            camera_transform.position = world_cursor_pos + difference;

            cc.scale = cc.scale * factor;
        }
    }

    // translate
    if(translate) {
        if(core.key_map[GLFW_MOUSE_BUTTON_LEFT]) camera_transform.position -= camera_transform.orientation * (vec2(core.cursor_delta.x, core.cursor_delta.y) * 2.0f / float(core.window.screen_size.x) / cc.scale);
        else translate = false;
    }

    // pressed buttons

    if(core.pressed_buttons.contains(GLFW_KEY_F11)) {
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
    }

    if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_RIGHT)) {
        if(gui_system.capture == 0xFFFFFFFF) {
            if(core.key_map[GLFW_KEY_LEFT_SHIFT]) {
                std::set<uint32_t> non_colliding;
                Physics_system& ps = ecs.get_system<Physics_system>();

                uint32_t num_links = 8;

                float scale = 1.0f;

                float sep = 0.05f * scale;
                vec2 size = vec2(0.33f, 1.0f) * scale;

                Transform t;
                t.position = world_cursor_pos;
                vec2 up = normalize(vec2(1.0f, 1.0f));
                t.orientation = {up, vec2(-up.y, up.x)};
                
                Collider c;
                Collision_shape cs;
                cs.vertices = {vec2(0, -(size.y - size.x) * 0.5f), vec2(0.0f, (size.y - size.x) * 0.5f)};
                cs.radius = vec2(size.x * 0.5f);
                cs.mass = 0x6 * scale * scale;
                c.shapes.push_back(cs);

                vec2 shift = Physics_system::calculate_inertia(c);
                t.position += t.orientation * shift;
                t.position += t.orientation * vec2(0, size.y * 0.5f);
                //c.allow_rotation = false;

                Mesh m;
                m.color = vec3(0.9f, 0.9f, 0.9f);
                create_mesh(m, c);

                uint32_t prev_entity = NULL_ENTITY;
                uint32_t first_entity;

                for(int i = 0; i < num_links; ++i) {    
                    uint32_t capsule = ecs.insert_entity();
                    non_colliding.emplace(capsule);

                    ecs.insert_component(capsule, m);
                    ecs.insert_component(capsule, t);
                    ecs.insert_component(capsule, c);

                    
                    if(prev_entity != NULL_ENTITY) {
                        Constraint constraint;
                        constraint.a = prev_entity;
                        constraint.b = capsule;

                        pos_constraint pc;
                        pc.a = vec2(0, (size.y + sep) * 0.5f);
                        pc.b = vec2(0, -(size.y + sep) * 0.5f);
                        pc.vs = {vec2(1, 0), vec2(0, 1)};

                        constraint.pos.push_back(pc);

                        ps.constraints.push_back(constraint);
                    } else first_entity = capsule;

                    t.position += t.orientation * vec2(0, (size.y + sep));

                    prev_entity = capsule;
                }

                for(uint32_t link : non_colliding) {
                    Collider& c = ecs.get_component<Collider>(link);
                    c.non_colliding = non_colliding;
                }

                float asteroid_radius = 0.5f * scale;

                Collider c2;
                cs.vertices = {vec2(0, 0)};
                cs.radius = vec2(asteroid_radius);
                cs.mass = 0x30 * scale * scale;
                c2.shapes.push_back(cs);

                shift = Physics_system::calculate_inertia(c2);
                t.position += t.orientation * shift;
                t.position += t.orientation * vec2(0, 1.0f);

                Mesh m2;
                m2.color = vec3(0.9f, 0.9f, 0.9f);
                create_mesh(m2, c2);
                
                t.position = world_cursor_pos + t.orientation * vec2(0, (size.y + sep) * num_links + asteroid_radius);

                uint32_t asteroid = ecs.insert_entity();
                ecs.insert_component(asteroid, m2);
                ecs.insert_component(asteroid, t);
                ecs.insert_component(asteroid, c2);

                {
                    Constraint constraint;
                    constraint.a = prev_entity;
                    constraint.b = asteroid;

                    pos_constraint pc;
                    pc.a = vec2(0, (size.y + sep) * 0.5f);
                    pc.b = vec2(0, -(asteroid_radius + sep * 0.5f));
                    pc.vs = {vec2(1, 0), vec2(0, 1)};

                    constraint.pos.push_back(pc);

                    ps.constraints.push_back(constraint);
                }

                //

                t.position = world_cursor_pos + t.orientation * -vec2(0, asteroid_radius);

                asteroid = ecs.insert_entity();
                ecs.insert_component(asteroid, m2);
                ecs.insert_component(asteroid, t);
                ecs.insert_component(asteroid, c2);

                {
                    Constraint constraint;
                    constraint.a = asteroid;
                    constraint.b = first_entity;

                    pos_constraint pc;
                    pc.a = vec2(0, asteroid_radius + sep * 0.5f);
                    pc.b = vec2(0, -(size.y + sep) * 0.5f);
                    pc.vs = {vec2(1, 0), vec2(0, 1)};

                    constraint.pos.push_back(pc);

                    ps.constraints.push_back(constraint);
                }
            } else if(core.key_map[GLFW_KEY_LEFT_CONTROL]) {
                /*
                Physics_system& ps = ecs.get_system<Physics_system>();

                vec2 size = vec2(1.0f, 24.0f) * 0.125f;

                Transform t;
                t.position = world_cursor_pos;
                vec2 up = normalize(vec2(1.0f, 1.0f));
                t.orientation = {up, vec2(-up.y, up.x)};
                
                Collider c;
                Collision_shape cs;
                cs.vertices = {vec2(0, -(size.y - size.x) * 0.5f), vec2(0.0f, (size.y - size.x) * 0.5f)};
                cs.radius = vec2(size.x * 0.5f);
                cs.mass = 0x6 * size.x * size.y;
                c.shapes.push_back(cs);

                vec2 shift = Physics_system::calculate_inertia(c);
                t.position += t.orientation * shift;
                t.position += t.orientation * vec2(0, size.y * 0.5f);

                Mesh m;
                m.color = vec3(0.9f, 0.9f, 0.9f);
                create_mesh(m, c);

                uint32_t capsule = ecs.insert_entity();

                ecs.insert_component(capsule, m);
                ecs.insert_component(capsule, t);
                ecs.insert_component(capsule, c);
                */

                uint32_t entity = ecs.insert_entity();

                /*
                std::vector<vec2> origins = {
                    vec2(-2, -2),
                    vec2(-1, -2),
                    vec2(0, -2),
                    vec2(1, -2),
                    vec2(2, -2),

                    vec2(-2, -1),
                    vec2(-1, -1),
                    vec2(0, -1),
                    vec2(1, -1),
                    vec2(2, -1),
                    
                    vec2(-2, 0),
                    vec2(-1, 0),
                    vec2(0, 0),
                    vec2(1, 0),
                    vec2(2, 0),

                    vec2(-2, 1),
                    vec2(-1, 1),
                    vec2(0, 1),
                    vec2(1, 1),
                    vec2(2, 1),
                    
                    vec2(-2, 2),
                    vec2(-1, 2),
                    vec2(0, 2),
                    vec2(1, 2),
                    vec2(2, 2),
                };
                */
                vec2 size = vec2(1.5f);
                std::vector<vec2> origins = {
                    vec2(-1, -2),
                    vec2(1, -2),

                    vec2(-1, -1),
                    vec2(0, -1),
                    vec2(1, -1),
                    
                    vec2(-2, 0),
                    vec2(-1, 0),
                    vec2(0, 0),
                    vec2(1, 0),
                    vec2(2, 0),

                    vec2(0, 1),
                    
                    vec2(0, 2),
                };
                
                Transform t;
                t.position = world_cursor_pos;
                t.orientation = identity<mat2>();
                
                Collider c;

                Collision_shape cs;
                cs.vertices = {
                    vec2(-1, -1),
                    vec2(1, -1),
                    vec2(1, 1),
                    vec2(-1, 1)
                };
                for(vec2& v : cs.vertices) v *= size * 0.5f;
                cs.radius = vec2(0.0f);
                cs.mass = 40.0f;

                for(vec2 origin : origins) {
                    cs.position = origin * size;
                    c.shapes.push_back(cs);
                }

                //

                vec2 shift = Physics_system::calculate_inertia(c);
                t.position += shift;

                c.create_BVH();

                Mesh m;
                m.color = vec3(0.35f);
                create_mesh(m, c);
                
                ecs.insert_component(entity, m);
                ecs.insert_component(entity, t);
                ecs.insert_component(entity, c);
            } else {

                auto insert_square = [&](vec2 pos, vec2 size, mat2 ori) {
                    uint32_t entity = ecs.insert_entity();

                    Transform t;
                    t.position = pos;
                    t.orientation = ori;

                    
                    Collider c;
                    Collision_shape cs;
                    cs.vertices = {
                        vec2(-1, -1),
                        vec2(1, -1),
                        vec2(1, 1),
                        vec2(-1, 1)
                    };
                    for(vec2& v : cs.vertices) v *= size * 0.5f;
                    cs.radius = vec2(0.0f);
                    cs.mass = size.x * size.y * 25.0f;
                    c.shapes.push_back(cs);
                    //c.allow_rotation = false;

                    vec2 shift = Physics_system::calculate_inertia(c);
                    t.position += shift;

                    Mesh m;
                    m.color = vec3(0.35f);//get_color(abs(core.random())) * 0.7f + 0.3f;
                    create_mesh(m, c);
                    
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
                    Collision_shape cs;
                    cs.vertices = {
                        vec2(0.0f)
                    };
                    cs.radius = size * 0.5f;
                    cs.mass = size.x * size.y * 25.0f;
                    c.shapes.push_back(cs);

                    vec2 shift = Physics_system::calculate_inertia(c);
                    t.position += shift;

                    Mesh m;
                    m.color = vec3(0.35f);//get_color(abs(core.random())) * 0.7f + 0.3f;
                    create_mesh(m, c);
                    
                    ecs.insert_component(entity, m);
                    ecs.insert_component(entity, t);
                    ecs.insert_component(entity, c);
                };
                
                if(core.key_map[GLFW_KEY_LEFT_ALT]) {
                    mat2 orientation = identity<mat2>();
                    float floor = 8;
                    vec2 size = vec2(0.5f, 0.5f);

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

                    float s = 1.0f;

                    uint32_t square_size = 16;
                    float separation = s * 0.25f;
                    vec2 max_size = vec2(s);
                    vec2 min_size = vec2(s);

                    for(int y = 0; y < square_size; ++y) {
                        for(int x = 0; x < square_size; ++x) {
                            vec2 width = float(square_size) * max_size + (square_size - 1) * separation;
                            vec2 pos = vec2(-width.x * 0.5f + max_size.x * (x + 0.5f) + separation * x, -width.y * 0.5f + max_size.y * (y + 0.5f) + separation * y);

                            if(core.key_map[GLFW_KEY_0]) insert_ellipse(orientation * pos + origin, min_size + (max_size - min_size) * vec2(abs(core.random()), abs(core.random())), orientation);
                            else insert_square(orientation * pos + origin, min_size + (max_size - min_size) * vec2(abs(core.random()), abs(core.random())), orientation);
                        }
                    }
                }
            }
        }
    }

    if(core.pressed_buttons.contains(GLFW_KEY_F6)) {
        std::vector<uint8_t> pixels(core.window.screen_size.x * core.window.screen_size.y * 3);

        glReadPixels(0,0, core.window.screen_size.x, core.window.screen_size.y, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        
        std::string filename = "output/screenshot" + to_base(int64_t(get_absolute_time() * 10), 10, true) + ".png";
        std::cout << "screenshot saved as " << filename << std::endl;

        stbi_flip_vertically_on_write(true);

        stbi_write_png(filename.c_str(), core.window.screen_size.x, core.window.screen_size.y, 3, pixels.data(), 3 * core.window.screen_size.x);
    }

    if(core.pressed_buttons.contains(GLFW_MOUSE_BUTTON_LEFT)) {
        if(gui_system.capture == 0xFFFFFFFF) {
            if(core.key_map[GLFW_KEY_LEFT_SHIFT]) {
                Physics_system& ps = ecs.get_system<Physics_system>();

                for(uint32_t entity : ps.collectors[0].entities) {
                    Transform& tf = ecs.get_component<Transform>(entity);
                    Collider& c = ecs.get_component<Collider>(entity);

                    vec2 rel_point = transpose(tf.orientation) * (world_cursor_pos - tf.position);

                    bool collide = false;

                    for(Collision_shape& cs : c.shapes) {
                        vec2 rel_point2 = transpose(cs.orientation) * (rel_point - cs.position);

                        collide |= Physics_system::collision_point(cs.vertices, cs.radius, rel_point2);

                        if(collide) break;
                    }

                    if(collide) {
                        held_object = entity;
                        held_constraint = ps.constraints.size();

                        Constraint constraint;
                        constraint.a = entity;
                        
                        pos_constraint pc;
                        pc.a = rel_point;
                        pc.b = world_cursor_pos;
                        pc.vs = {vec2(1, 0), vec2(0, 1)};
                        pc.compliance = 0.00033;

                        constraint.pos.push_back(pc);

                        ps.constraints.push_back(constraint);

                        break;
                    }
                }
            } else {
                translate = true;
            }
        }
    }

    if(core.pressed_buttons.contains(GLFW_KEY_F5)) {
        debug_physics = !debug_physics;
    }

    Physics_system& ps = ecs.get_system<Physics_system>();

    if(held_object != NULL_ENTITY) {
        if(!core.key_map[GLFW_MOUSE_BUTTON_LEFT]) {
            held_object = NULL_ENTITY;

            ps.constraints.erase(ps.constraints.begin() + held_constraint);
            held_constraint = NULL_ENTITY;
        } else {
            Constraint& c = ps.constraints[held_constraint];
            c.pos[0].b = world_cursor_pos;
        }
    }

    float rot = 0.0f;

    if(core.key_map[GLFW_KEY_Q]) {
        rot += 1.0f;
    }
    if(core.key_map[GLFW_KEY_E]) {
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