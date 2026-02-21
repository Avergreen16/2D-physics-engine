#include "render.hpp"
#include "input.hpp"
#include "core.hpp"
#include "physics.hpp"
#include "erosion.hpp"

float ellipsoid_height(vec3 pos, vec3 radii) {
    return sqrt(1.0f / (pos.x * pos.x / (radii.x * radii.x) + pos.y * pos.y / (radii.y * radii.y) + pos.z * pos.z / (radii.z * radii.z)));
}

struct Object_vertex {
    vec3 v;
};

void create_mesh(Mesh& m, std::vector<vec2> v, vec2 radius, bool create_interior) {
    m.v_tris = std::shared_ptr<Vertices>(new Vertices);
    m.v_tris->init();
    
    m.v_lines = std::shared_ptr<Vertices>(new Vertices);
    m.v_lines->init();
    
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

    float min_dist = FLT_MAX;
    std::vector<Object_vertex> vvv;
    for(int i = 0 ; i < vv.size(); ++i) {
        vec2 v0 = vv[i];
        vec2 v1 = vv[(i + 1) % vv.size()];

        vec2 origin_v = -v0;
        float len = length(v1 - v0);
        vec2 dir = (v1 - v0) / len;
        float f = dot(dir, origin_v);
        f = clamp(f, 0.0f, len);
        vec2 closest_point = f * dir + v0;

        min_dist = min(length(closest_point), min_dist);



        Object_vertex ov;
        ov.v = vec3(v0, 0.5);
        vvv.push_back(ov);

        ov.v = vec3(v1, 0.5);
        vvv.push_back(ov);
    }
    
    /*
    Object_vertex ov;
    ov.v = vec3(0.0f, 0.0f, 0.5);
    vvv.push_back(ov);
    ov.v = vec3(vec2(0.0f, 0.75f) * min_dist, 0.5);
    vvv.push_back(ov);
    */

    m.v_lines->vertex_buffer_data(vvv.data(), vvv.size(), sizeof(Object_vertex), GL_STATIC_DRAW);
    m.v_lines->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(float) * 3, 0);

    vvv.clear();

    if(create_interior) {
        for(int i = 0; i < vv.size(); ++i) {
            vec3 v0 = vec3(vv[i], 0.5f);
            vec3 v1 = vec3(vv[(i + 1) % vv.size()], 0.5f);
            vec3 v2 = vec3(0.0f, 0.0f, 0.5f);

            vec3 vc = cross(v0 - v2, v1 - v2);
            if(vc.z < 0.0f) {
                vec3 temp = v1;
                v1 = v2;
                v2 = temp;
            }

            Object_vertex ov;
            ov.v = v0;
            vvv.push_back(ov);

            ov.v = v1;
            vvv.push_back(ov);

            ov.v = v2;
            vvv.push_back(ov);
        }
    }
    
    m.v_tris->vertex_buffer_data(vvv.data(), vvv.size(), sizeof(Object_vertex), GL_STATIC_DRAW);
    m.v_tris->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(float) * 3, 0);
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
    
    color.w = 0.2f;
    
    om.v_tris->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);
    glUniform4fv(3, 1, &color[0]);

    om.v_tris->draw_vertices(GL_TRIANGLES);

    color.w = 1.0f;

    om.v_lines->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);
    glUniform4fv(3, 1, &color[0]);

    glLineWidth(1);
    om.v_lines->draw_vertices(GL_LINES);
}

void Render_system::render_cloud(uint32_t camera) {
    vec2 pos = vec2(0.0f, 128.0f);
    vec2 size = vec2(32.0f, 32.0f);

    std::vector<vec2> square = {
        vec2(-1.0f, -1.0f),
        vec2(1.0f, -1.0f),
        vec2(1.0f, 1.0f),
        vec2(-1.0f, -1.0f),
        vec2(1.0f, 1.0f),
        vec2(-1.0f, 1.0f)
    };

    for(vec2& v : square) {
        v *= size;
    }

    vv->vertex_buffer_data(square.data(), square.size(), sizeof(vec2), GL_STREAM_DRAW);
    vv->add_vertex_attribute(0, 2, GL_FLOAT, false, sizeof(vec2), 0);

    Transform& ct = ecs.get_component<Transform>(camera);
    Camera& cc = ecs.get_component<Camera>(camera);

    Input_system& is = ecs.get_system<Input_system>();

    mat4 inv_rot = mat4(transpose(ct.orientation));

    mat4 view = inv_rot * scale(vec3(cc.scale, cc.scale, 1.0f)) * translate(vec3(-ct.position, 0.0f));
    mat4 model = translate(vec3(pos, 0.0f));

    float aspect_ratio = float(core.window.screen_size.y) / core.window.screen_size.x;
    mat4 proj = scale(vec3(1.0f, 1.0f / aspect_ratio, 1.0f));

    core.shaders["cloud_shader"]->use();

    core.textures["noise_map"]->bind(0);
    
    vv->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);

    vv->draw_vertices(GL_TRIANGLES);
}

struct Map_vertex {
    vec2 position;
    vec2 tex_coord;
};

void Render_system::render_map(uint32_t camera) {
    Erosion_system& es = ecs.get_system<Erosion_system>();

    vec2 pos = vec2(0.0f, 0.0f);
    vec2 size = es.size;

    std::vector<vec2> square = {
        vec2(-1.0f, -1.0f),
        vec2(1.0f, -1.0f),
        vec2(1.0f, 1.0f),
        vec2(-1.0f, -1.0f),
        vec2(1.0f, 1.0f),
        vec2(-1.0f, 1.0f)
    };

    std::vector<Map_vertex> map_vs;

    for(vec2& v : square) {
        map_vs.push_back({v * size, v * 0.5f - 0.5f});
    }

    vec2 map_size = es.size;

    vv->vertex_buffer_data(map_vs.data(), map_vs.size(), sizeof(Map_vertex), GL_STREAM_DRAW);
    vv->add_vertex_attribute(0, 2, GL_FLOAT, false, sizeof(Map_vertex), 0);
    vv->add_vertex_attribute(1, 2, GL_FLOAT, false, sizeof(Map_vertex), sizeof(float) * 2);

    Transform& ct = ecs.get_component<Transform>(camera);
    Camera& cc = ecs.get_component<Camera>(camera);

    Input_system& is = ecs.get_system<Input_system>();

    mat4 inv_rot = mat4(transpose(ct.orientation));

    mat4 view = inv_rot * scale(vec3(cc.scale, cc.scale, 1.0f)) * translate(vec3(-ct.position, 0.0f));
    mat4 model = translate(vec3(pos, 0.0f));

    float aspect_ratio = float(core.window.screen_size.y) / core.window.screen_size.x;
    mat4 proj = scale(vec3(1.0f, 1.0f / aspect_ratio, 1.0f));

    core.shaders["map_shader"]->use();

    es.map_texture->bind(0);
    
    vv->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);

    vv->draw_vertices(GL_TRIANGLES);
}

void Render_system::render_marker(vec2 pos, vec2 normal, uint32_t camera) {
    Transform ct = ecs.get_component<Transform>(camera);
    Camera& cc = ecs.get_component<Camera>(camera);

    vec2 size = {10, 10};
    vec4 tex_range = {5, 0, 5, 5};

    float scale_inv = 1.0f / (float(core.window.viewport_size.x) * 0.5f * cc.scale);

    size *= scale_inv;

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

    core.shaders["texture_shader"]->use();
    texture->bind(0);
    vv->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);

    vv->draw_vertices(GL_TRIANGLES);

    std::vector<vec3> v2 = {
        {0, 0, 0.5f},
        {normal * 20.0f, 0.5f}
    };

    for(vec3& v : v2) {
        v.x *= scale_inv;
        v.y *= scale_inv;
    }

    vv->vertex_buffer_data(v2.data(), v2.size(), sizeof(vec3), GL_STREAM_DRAW);
    vv->add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(vec3), 0);
    
    core.shaders["color_shader"]->use();

    vv->bind();

    vec4 color = vec4(1.0f, 1.0f, 0.25f, 1.0f);

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);
    glUniform4fv(3, 1, &color[0]);

    glLineWidth(1);
    vv->draw_vertices(GL_LINES);
}

void Render_system::call() {
    if(!vv->initialized) vv->init();

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

    /*std::vector<vec2> normals;

    for(auto& [k, d] : ps.collision_table) {
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
            normals.push_back(c.normal);
            normals.push_back(-c.normal);
        }
    }*/

    for(uint32_t camera : collectors[0].entities) {
        Camera& camera_camera = ecs.get_component<Camera>(camera);

        render_background(camera);

        render_map(camera);

        for(uint32_t entity : collectors[1].entities) {
            render_object(entity, camera);
        }

        int i = 0;
        for(vec2 v : marker_points) {
            vec2 normal = normals[i];
            render_marker(v, normal, camera);
            ++i;
        }
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

void Render_system::render_background(uint32_t camera) {
    std::vector<vec2> square = {
        vec2(-1.0f, -1.0f),
        vec2(1.0f, -1.0f),
        vec2(1.0f, 1.0f),
        vec2(-1.0f, -1.0f),
        vec2(1.0f, 1.0f),
        vec2(-1.0f, 1.0f)
    };
    
    vv->vertex_buffer_data(square.data(), square.size(), sizeof(vec2), GL_STREAM_DRAW);
    vv->add_vertex_attribute(0, 2, GL_FLOAT, false, sizeof(vec2), 0);

    Transform& ct = ecs.get_component<Transform>(camera);
    Camera& cc = ecs.get_component<Camera>(camera);

    Input_system& is = ecs.get_system<Input_system>();

    mat4 inv_rot = mat4(transpose(ct.orientation));

    mat4 view = inv_rot * scale(vec3(cc.scale, cc.scale, 1.0f)) * translate(vec3(-ct.position, 0.0f));

    float aspect_ratio = float(core.window.screen_size.y) / core.window.screen_size.x;
    mat4 proj = scale(vec3(1.0f, 1.0f / aspect_ratio, 1.0f));

    core.shaders["background"]->use();
    
    vv->bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);

    vv->draw_vertices(GL_TRIANGLES);
}
