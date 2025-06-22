#include "render.hpp"
#include "input.hpp"
#include "core.hpp"
#include "physics.hpp"

float ellipsoid_height(vec3 pos, vec3 radii) {
    return sqrt(1.0f / (pos.x * pos.x / (radii.x * radii.x) + pos.y * pos.y / (radii.y * radii.y) + pos.z * pos.z / (radii.z * radii.z)));
}

struct Object_vertex {
    vec3 v;
};

void create_mesh(Mesh& m, std::vector<vec2> v, vec2 radius) {
    m.vertices = std::shared_ptr<Vertices>(new Vertices);
    m.vertices->init();
    
    float sphere_segments = max(64, int32_t(8 * max(radius.x, radius.y)));

    std::vector<vec2> vv;
    if(radius.x == 0.0f && radius.y == 0.0f) {
        for(int i = 0; i < v.size(); ++i) {
            vv.push_back(v[i]);
        }
    } else {
        vec2 sum = vec2(0.0f);
        for(int i = 0; i < v.size(); ++i) {
            sum += v[i];
        }
        sum /= v.size();

        std::unordered_set<uint32_t> cc;
        vec2 first_normal = vec2(0.0f);
        vec2 prev_normal = vec2(0.0f);
        for(int i = 0; i < v.size(); ++i) {
            uint16_t a = i;
            uint16_t b = (i + 1) % v.size();
            if(a == b) {
                vec2 v0 = v[a];

                float angle_a = 0.0f;
                float angle_b = 2 * M_PI;

                float angle_per_segment = 2 * M_PI / sphere_segments;

                for(float j = angle_a + angle_per_segment; j < angle_b; j += angle_per_segment) {
                    vec2 vc = {cos(j), sin(j)};
                    vc *= radius;
                    vc = v0 + vc;

                    vv.push_back(vc);
                }
            } else {
                vec2 v0 = v[a];
                vec2 v1 = v[b];
                
                if(a > b) {
                    uint16_t temp = a;
                    a = b;
                    b = temp;
                }

                vec2 direction = normalize(v[b] - v[a]);
                vec2 normal = vec2(direction.y, -direction.x);
                if(dot(normal, sum - v[a]) > 0.0f) {
                    normal = -normal;
                }

                uint32_t c = (uint32_t)a | ((uint32_t)b << 16);
                if(cc.contains(c)) {
                    normal = -normal;
                } else {
                    cc.insert(c);
                }

                // create previous sphere
                if(prev_normal.x != 0.0f || prev_normal.y != 0.0f) {
                    vec2 va = prev_normal * radius;
                    vec2 vb = normal * radius;
                    float angle_a = atan2(va.y, va.x);
                    float angle_b = atan2(vb.y, vb.x);

                    if(angle_b < angle_a) angle_a -= 2 * M_PI;

                    float angle_per_segment = 2 * M_PI / sphere_segments;

                    for(float j = angle_a + angle_per_segment; j < angle_b; j += angle_per_segment) {
                        vec2 vc = {cos(j), sin(j)};
                        vc *= radius;
                        vc = v0 + vc;

                        vv.push_back(vc);
                    }
                }

                vv.push_back(v0 + normal * radius);
                vv.push_back(v1 + normal * radius);

                // create first sphere
                if(i == v.size() - 1) {
                    vec2 va = normal * radius;
                    vec2 vb = first_normal * radius;
                    float angle_a = atan2(va.y, va.x);
                    float angle_b = atan2(vb.y, vb.x);

                    if(angle_b < angle_a) angle_a -= 2 * M_PI;

                    float angle_per_segment = 2 * M_PI / sphere_segments;

                    for(float j = angle_a + angle_per_segment; j < angle_b; j += angle_per_segment) {
                        vec2 vc = {cos(j), sin(j)};
                        vc *= radius;
                        vc = v1 + vc;

                        vv.push_back(vc);
                    }
                }


                prev_normal = normal;
                if(first_normal.x == 0.0f && first_normal.y == 0.0f) first_normal = normal;
            }
        }
    }

    std::vector<Object_vertex> vvv;
    for(int i = 0 ; i < vv.size(); ++i) {
        Object_vertex ov;
        ov.v = vec3(vv[i], 0.5);
        vvv.push_back(ov);

        ov.v = vec3(vv[(i + 1) % vv.size()], 0.5);
        vvv.push_back(ov);
    }

    m.vertices->vertex_buffer_data(vvv.data(), vvv.size(), sizeof(Object_vertex), GL_STATIC_DRAW);
    m.vertices->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(float) * 3, 0);
}


Render_system::Render_system() {
    Signature s = ecs.update_signature<Transform>();
    ecs.update_signature<Camera>(s);
    collectors.push_back(Collector(s, false));
    
    s = ecs.update_signature<Transform>();
    ecs.update_signature<Mesh>(s);
    collectors.push_back(Collector(s, false));

    framebuffers.emplace_back(Framebuffer({800, 600}, {{{GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT}, GL_COLOR_ATTACHMENT0, 0}, {{GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, GL_COLOR_ATTACHMENT1, 1}, {{GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, GL_COLOR_ATTACHMENT2, 2}, {{GL_DEPTH_COMPONENT32F, GL_RED, GL_FLOAT}, GL_DEPTH_ATTACHMENT}}));
    framebuffers.emplace_back(Framebuffer({800, 600}, {{{GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT}, GL_COLOR_ATTACHMENT0, 0}, {{GL_DEPTH_COMPONENT32F, GL_RED, GL_FLOAT}, GL_DEPTH_ATTACHMENT}}));
    framebuffers.emplace_back(Framebuffer({4096, 4096}, {{{GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, GL_COLOR_ATTACHMENT1, 1}, {{GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, GL_COLOR_ATTACHMENT2, 2}, {{GL_DEPTH_COMPONENT32F, GL_RED, GL_FLOAT}, GL_DEPTH_ATTACHMENT}}));

    framebuffer_link = {1, 1, 0};
}

void Render_system::resize_framebuffers() {
    for(int i = 0; i < framebuffers.size(); ++i) {
        float link = framebuffer_link[i];

        if(link > 0) {
            framebuffers[i].resize((vec2)screen_size * link);
        }
    }
}

void Render_system::bind_framebuffer(uint32_t i) {
    framebuffers[i].bind();
    glViewport(0, 0, framebuffers[i].textures[0].size.x, framebuffers[i].textures[0].size.y);
}

void Render_system::bind_default_framebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, core.window.viewport_size.x, core.window.viewport_size.y);
}

void Render_system::render_object(uint32_t object, uint32_t camera) {
    Transform& ot = ecs.get_component<Transform>(object);
    Mesh& om = ecs.get_component<Mesh>(object);

    Transform& ct = ecs.get_component<Transform>(camera);
    Camera& cc = ecs.get_component<Camera>(camera);

    Input_system& is = ecs.get_system<Input_system>();

    mat4 inv_rot = mat4(transpose(ct.orientation));

    mat4 view = inv_rot * scale(vec3(cc.scale, cc.scale, 1.0f)) * translate(vec3(-ct.position, 0.0f));
    mat4 model = translate(vec3(ot.position, 0.0f)) * mat4(ot.orientation);

    float aspect_ratio = float(core.window.screen_size.y) / core.window.screen_size.x;
    mat4 proj = scale(vec3(1.0f, 1.0f / aspect_ratio, 1.0f));

    vec4 color = vec4(om.color, 1.0f);

    /*if(ecs.has_component<Collider>(object)) {
        Collider& oc = ecs.get_component<Collider>(object);

        if(oc.colliding) color = vec4(1.0f, 0.3f, 0.3f, 1.0f);
        else color = vec4(0.3f, 1.0f, 0.3f, 1.0f);
    }*/

    core.shaders["color_shader"]->use();

    om.vertices->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);
    glUniform4fv(3, 1, &color[0]);

    glLineWidth(2);
    om.vertices->draw_vertices(GL_LINES);
}

void Render_system::render_marker(vec2 pos, uint32_t camera) {
    Transform ct = ecs.get_component<Transform>(camera);
    Camera& cc = ecs.get_component<Camera>(camera);

    vec2 size = {10, 10};
    vec4 tex_range = {5, 0, 5, 5};

    size /= float(core.window.viewport_size.x) * 0.5f;
    size /= cc.scale;

    std::vector<Texture_vertex> v = {
        Texture_vertex({-size.x * 0.5f, -size.y * 0.5f, 0.5}, tex_range.xy()),
        Texture_vertex({size.x * 0.5f, -size.y * 0.5f, 0.5}, tex_range.xy() + vec2(tex_range.z, 0)),
        Texture_vertex({-size.x * 0.5f, size.y * 0.5f, 0.5}, tex_range.xy() + vec2(0, tex_range.w)),
        Texture_vertex({size.x * 0.5f, size.y * 0.5f, 0.5}, tex_range.xy() + vec2(tex_range.z, tex_range.w)),
    };
    std::shared_ptr<Texture> texture = core.textures["gui_texture"];

    for(Texture_vertex& vvv : v) {
        vvv.tex /= vec2(texture->size.xy());
    }

    v = {v[0], v[1], v[3], v[0], v[3], v[2]};

    if(!vv->initialized) vv->init();
    vv->vertex_buffer_data(v.data(), v.size(), sizeof(Texture_vertex), GL_STREAM_DRAW);

    vv->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(Texture_vertex), 0);
    vv->add_vertex_attribute(1, 2, GL_FLOAT, false, sizeof(Texture_vertex), 3 * sizeof(float));

    Input_system& is = ecs.get_system<Input_system>();
    
    mat4 inv_rot = mat4(transpose(ct.orientation));

    mat4 view = inv_rot * scale(vec3(cc.scale, cc.scale, 1.0f)) * translate(vec3(-ct.position, 0.0f));
    mat4 model = translate(vec3(pos, 0.0f));

    float aspect_ratio = float(core.window.screen_size.y) / core.window.screen_size.x;
    mat4 proj = scale(vec3(1.0f, 1.0f / aspect_ratio, 1.0f));

    vec4 color = vec4(0.3f, 0.3f, 1.0f, 1.0f);

    core.shaders["texture_shader"]->use();
    texture->bind(0);
    vv->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);

    vv->draw_vertices(GL_TRIANGLES);
}

void Render_system::call() {
    core.proj = scale(vec3(1.0f));

    glDepthFunc(GL_GEQUAL);
    glClearDepth(0.0f);
    glDepthRange(0, 1);
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glDisable(GL_DEPTH_CLAMP);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);

    if(uvec2(core.window.viewport_size) != screen_size) {
        screen_size = core.window.viewport_size;

        resize_framebuffers();
    }

    for(int i = 0; i < framebuffers.size(); ++i) {
        bind_framebuffer(i);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    bind_default_framebuffer();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    bind_framebuffer(0);
    
    Physics_system& ps = ecs.get_system<Physics_system>();

    marker_points.clear();

    /*for(auto& [k, d] : ps.collision_table) {
        for(Collision_data& c : d) {
            Transform& ta = ecs.get_component<Transform>(c.a);
            vec2 point_a = ta.orientation * c.pa + ta.position;
            vec2 point_b;

            if(c.b == 0xFFFFFFFF) {
                point_b = c.pb;
            } else {
                Transform& tb = ecs.get_component<Transform>(c.b);

                point_b = tb.orientation * c.pb + tb.position;
            }

            marker_points.push_back(point_a);
            marker_points.push_back(point_b);
        }
    }*/

    for(uint32_t camera : collectors[0].entities) {
        Camera& camera_camera = ecs.get_component<Camera>(camera);

        for(uint32_t entity : collectors[1].entities) {
            render_object(entity, camera);
        }
        
        render_visualizer(camera);

        for(vec2 v : marker_points) render_marker(v, camera);
    }


    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    bind_default_framebuffer();

    core.shaders["screen_shader"]->use();

    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE); 

    glUniform1f(0, 1.0f);

    framebuffers[0].textures[0].bind(0);

    glDrawArrays(GL_TRIANGLES, 0, 6);  

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    gui_system.render();

    render_cursor();
    
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);


    glfwSwapBuffers(core.window.window);
}


void Render_system::render_cursor() {
    Input_system& input_system = ecs.get_system<Input_system>();
    GUI_system& gui_system = ecs.get_system<GUI_system>();

    ivec2 size;
    ivec4 tex_range;
    ivec2 rel_pos;

    switch(gui_system.cursor_mode) {
        case CURSOR_CLICK: 
            size = {10, 16};
            tex_range = {0, 0, 5, 8};
            rel_pos = {0, 0};
            break;
        case CURSOR_RESIZE_T: 
            size = {10, 18};
            tex_range = {0, 8, 5, 9};
            rel_pos = {-5, 9};
            break;
        case CURSOR_RESIZE_TR: 
            size = {14, 14};
            tex_range = {0, 22, 7, 7};
            rel_pos = {-7, 7};
            break;
        case CURSOR_RESIZE_R: 
            size = {18, 10};
            tex_range = {0, 17, 9, 5};
            rel_pos = {-9, 5};
            break;
        case CURSOR_RESIZE_BR: 
            size = {14, 14};
            tex_range = {0, 29, 7, 7};
            rel_pos = {-7, 7}; 
            break;
        case CURSOR_RESIZE_B: 
            size = {10, 18};
            tex_range = {0, 8, 5, 9};
            rel_pos = {-5, 9};
            break;
        case CURSOR_RESIZE_BL: 
            size = {14, 14};
            tex_range = {0, 22, 7, 7};
            rel_pos = {-7, 7};
            break;
        case CURSOR_RESIZE_L: 
            size = {18, 10};
            tex_range = {0, 17, 9, 5};
            rel_pos = {-9, 5};
            break;
        case CURSOR_RESIZE_TL:
            size = {14, 14};
            tex_range = {0, 29, 7, 7};
            rel_pos = {-7, 7}; 
            break;
        case CURSOR_TEXT:
            size = {6, 14};
            tex_range = {0, 36, 3, 7};
            rel_pos = {-3, 7}; 
            break;
    }
    
    vec2 pos = input_system.cursor_pos;

    std::vector<UI_vertex> v = {
        UI_vertex({0, -size.y, 0.5}, tex_range.xy()),
        UI_vertex({size.x, -size.y, 0.5}, tex_range.xy() + ivec2(tex_range.z, 0)),
        UI_vertex({0, 0, 0.5}, tex_range.xy() + ivec2(0, tex_range.w)),
        UI_vertex({size.x, 0, 0.5}, tex_range.xy() + ivec2(tex_range.z, tex_range.w)),
    };

    for(UI_vertex& vv : v) {
        vv.position += vec3(pos + vec2(rel_pos), 0.0f);
    }

    v = {v[0], v[1], v[3], v[0], v[3], v[2]};

    if(!vv->initialized) vv->init();
    vv->vertex_buffer_data(v.data(), v.size(), sizeof(UI_vertex), GL_STREAM_DRAW);

    vv->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(UI_vertex), 0);
    vv->add_vertex_attribute(1, 2, GL_FLOAT, false, sizeof(UI_vertex), 3 * sizeof(float));
    vv->add_vertex_attribute(2, 4, GL_FLOAT, false, sizeof(UI_vertex), 5 * sizeof(float));
    vv->add_vertex_attribute(3, 1, GL_INT, false, sizeof(UI_vertex), 9 * sizeof(float));
    vv->add_vertex_attribute(4, 4, GL_INT, false, sizeof(UI_vertex), 10 * sizeof(float));

    std::shared_ptr<Shader> ui_shader = core.shaders["gui_shader"];
    std::shared_ptr<Texture> ui_texture = core.textures["gui_texture"];

    glm::mat3 view_mat;
    glm::mat3 trans_mat;

    glm::ivec2 half_viewport_size = core.window.viewport_size / 2;

    view_mat = glm::scale(glm::translate(glm::identity<glm::mat3>(), {-1, -1}), glm::vec2{1.0 / half_viewport_size.x, 1.0 / half_viewport_size.y});
    trans_mat = glm::identity<glm::mat3>();

    ui_shader->use();
    ui_texture->bind(0);
    vv->bind();

    glUniformMatrix3fv(0, 1, false, &view_mat[0][0]);
    glUniformMatrix3fv(1, 1, false, &trans_mat[0][0]);

    vv->draw_vertices(GL_TRIANGLES);
}

void Render_system::render_visualizer(uint32_t camera) {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    Input_system& is = ecs.get_system<Input_system>();

    Transform& ct = ecs.get_component<Transform>(camera);
    Camera& cc = ecs.get_component<Camera>(camera);

    std::shared_ptr<Texture> texture = core.textures["gui_texture"];

    mat4 inv_rot = mat4(transpose(ct.orientation));
    mat4 view = inv_rot * scale(vec3(cc.scale, cc.scale, 1.0f)) * translate(vec3(-ct.position, 0.0f));
    mat4 model = identity<mat3>();
    float aspect_ratio = float(core.window.screen_size.y) / core.window.screen_size.x;
    mat4 proj = scale(vec3(1.0f, 1.0f / aspect_ratio, 1.0f));

    float axis_size = 256.0f;
    vec2 white_coord = vec2(7, 2) / vec2(texture->size.xy());
    
    Collider& ca = ecs.get_component<Collider>(visualizer.a);
    Collider& cb = ecs.get_component<Collider>(visualizer.b);
    std::vector<Texture_vertex> circle;
    float radius = ca.radius.x + cb.radius.x;
    int num_vertices = 16;
    for(int i = 0; i < num_vertices; ++i) {
        float angle = float(i) / num_vertices * M_PI * 2.0f;
        Texture_vertex va;
        va.color = vec4(1.0f);
        va.pos = vec3(vec2(cos(angle), sin(angle)) * radius, 0.5f);
        va.tex = white_coord;

        circle.push_back(va);
    }
    std::vector<Texture_vertex> new_circle;
    for(int i = 0; i < num_vertices; ++i) {
        new_circle.push_back(circle[i]);
        new_circle.push_back(circle[(i + 1) % num_vertices]);
    }
    circle = new_circle;
    
    std::vector<Texture_vertex> vertices_tri;
    std::vector<Texture_vertex> vertices_line = {
        Texture_vertex(vec3(axis_size, 0, 0.5), white_coord, vec4(1.0f, 0.25f, 0.25f, 1.0f)),
        Texture_vertex(vec3(-axis_size, 0, 0.5), white_coord, vec4(1.0f, 0.25f, 0.25f, 1.0f)),
        Texture_vertex(vec3(0, axis_size, 0.5), white_coord, vec4(0.25f, 1.0f, 0.25f, 1.0f)),
        Texture_vertex(vec3(0, -axis_size, 0.5), white_coord, vec4(0.25f, 1.0f, 0.25f, 1.0f)),
    };

    for(auto p : visualizer.lines) {
        for(auto v : p) {
            vertices_line.push_back(Texture_vertex(vec3(v.pos, 0.5), white_coord, v.color));
        }
    }

    for(auto p : visualizer.triangles) {
        for(auto v : p) {
            vertices_tri.push_back(Texture_vertex(vec3(v.pos, 0.5), white_coord, v.color));
        }
    }

    vec2 size_p = {10, 10};
    vec4 tex_range = {5, 0, 5, 5};

    for(auto p : visualizer.points) {
        vec2 size = size_p;
        size /= float(core.window.viewport_size.x) * 0.5f;
        size /= cc.scale;

        std::vector<Texture_vertex> v = {
            Texture_vertex({-size.x * 0.5f, -size.y * 0.5f, 0.5}, tex_range.xy()),
            Texture_vertex({size.x * 0.5f, -size.y * 0.5f, 0.5}, tex_range.xy() + vec2(tex_range.z, 0)),
            Texture_vertex({-size.x * 0.5f, size.y * 0.5f, 0.5}, tex_range.xy() + vec2(0, tex_range.w)),
            Texture_vertex({size.x * 0.5f, size.y * 0.5f, 0.5}, tex_range.xy() + vec2(tex_range.z, tex_range.w)),
        };
        
        for(Texture_vertex& vvv : v) {
            vvv.tex /= vec2(texture->size.xy());
            vvv.pos = vec3(ct.orientation * vvv.pos + p.pos, 0.0f);
            vvv.color = p.color;
        }

        v = {v[0], v[1], v[3], v[0], v[3], v[2]};

        for(Texture_vertex& t : v) {
            vertices_tri.push_back(t);
        }

        if(p.color != vec4(0.25f, 1.0f, 0.25f, 1.0f) && p.color != vec4(0.25f, 0.25f, 1.0f, 1.0f)) {
            for(Texture_vertex v : circle) {
                v.color = p.color;
                v.pos += vec3(p.pos, 0.0f);

                vertices_line.push_back(v);
            }
        }
    }

    // lines

    if(!vv->initialized) vv->init();
    vv->vertex_buffer_data(vertices_line.data(), vertices_line.size(), sizeof(Texture_vertex), GL_STREAM_DRAW);

    vv->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(Texture_vertex), 0);
    vv->add_vertex_attribute(1, 2, GL_FLOAT, false, sizeof(Texture_vertex), 3 * sizeof(float));
    vv->add_vertex_attribute(2, 4, GL_FLOAT, false, sizeof(Texture_vertex), 5 * sizeof(float));

    core.shaders["texture_shader"]->use();
    texture->bind(0);
    vv->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);

    vv->draw_vertices(GL_LINES);

    // triangles

    vv->vertex_buffer_data(vertices_tri.data(), vertices_tri.size(), sizeof(Texture_vertex), GL_STREAM_DRAW);

    vv->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(Texture_vertex), 0);
    vv->add_vertex_attribute(1, 2, GL_FLOAT, false, sizeof(Texture_vertex), 3 * sizeof(float));
    vv->add_vertex_attribute(2, 4, GL_FLOAT, false, sizeof(Texture_vertex), 5 * sizeof(float));

    core.shaders["texture_shader"]->use();
    texture->bind(0);
    vv->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);

    vv->draw_vertices(GL_TRIANGLES);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}