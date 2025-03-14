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

void create_mesh(Mesh& m, std::vector<vec2> v, float radius) {
    m.vertices = std::shared_ptr<Vertices>(new Vertices);
    m.vertices->init();

    std::vector<vec2> a;
    if(radius == 0.0f) {
        for(int i = 0; i < v.size(); ++i) {
            uint32_t i2 = (i + 1) % v.size();
            a.push_back(v[i]);
            a.push_back(v[i2]);
        }
    } else {
        
    }

    std::vector<Object_vertex> vv;
    for(vec2 aa : a) {
        Object_vertex ov;
        ov.v = vec3(aa, 0.5f);
        vv.push_back(ov);
    }

    m.vertices->vertex_buffer_data(vv.data(), vv.size(), sizeof(Object_vertex), GL_STATIC_DRAW);
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

    Transform ct = ecs.get_component<Transform>(camera);
    Camera& cc = ecs.get_component<Camera>(camera);

    Input_system& is = ecs.get_system<Input_system>();

    mat4 view = scale(vec3(cc.scale, cc.scale, 1.0f)) * translate(vec3(-ct.position, 0.0f));
    mat4 model = translate(vec3(ot.position, 0.0f));

    float aspect_ratio = float(core.window.screen_size.y) / core.window.screen_size.x;
    mat4 proj = scale(vec3(1.0f, 1.0f / aspect_ratio, 1.0f));

    vec4 color = vec4(0.3f, 0.3f, 1.0f, 1.0f);

    if(ecs.has_component<Collider>(object)) {
        Collider& oc = ecs.get_component<Collider>(object);

        if(oc.colliding) color = vec4(1.0f, 0.3f, 0.3f, 1.0f);
        else color = vec4(0.3f, 1.0f, 0.3f, 1.0f);
    }

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

    Vertices vertices;

    std::vector<Texture_vertex> v = {
        Texture_vertex({-size.x * 0.5f, -size.y * 0.5f, 0.5}, tex_range.xy()),
        Texture_vertex({size.x * 0.5f, -size.y * 0.5f, 0.5}, tex_range.xy() + vec2(tex_range.z, 0)),
        Texture_vertex({-size.x * 0.5f, size.y * 0.5f, 0.5}, tex_range.xy() + vec2(0, tex_range.w)),
        Texture_vertex({size.x * 0.5f, size.y * 0.5f, 0.5}, tex_range.xy() + vec2(tex_range.z, tex_range.w)),
    };
    std::shared_ptr<Texture> texture = core.textures["cursor"];

    for(Texture_vertex& vv : v) {
        vv.tex /= vec2(texture->size.xy());
    }

    v = {v[0], v[1], v[3], v[0], v[3], v[2]};

    vertices.init();
    vertices.vertex_buffer_data(v.data(), v.size(), sizeof(Texture_vertex), GL_STREAM_DRAW);

    vertices.add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(Texture_vertex), 0);
    vertices.add_vertex_attribute(1, 2, GL_FLOAT, false, sizeof(Texture_vertex), 3 * sizeof(float));

    Input_system& is = ecs.get_system<Input_system>();

    mat4 view = scale(vec3(cc.scale, cc.scale, 1.0f)) * translate(vec3(-ct.position, 0.0f));
    mat4 model = translate(vec3(pos, 0.0f));

    float aspect_ratio = float(core.window.screen_size.y) / core.window.screen_size.x;
    mat4 proj = scale(vec3(1.0f, 1.0f / aspect_ratio, 1.0f));

    vec4 color = vec4(0.3f, 0.3f, 1.0f, 1.0f);

    core.shaders["texture_shader"]->use();
    texture->bind(0);
    vertices.bind();

    glUniformMatrix4fv(0, 1, false, &view[0][0]);
    glUniformMatrix4fv(1, 1, false, &proj[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);

    vertices.draw_vertices(GL_TRIANGLES);
}

/*void Render_system::render_ring(uint32_t entity, uint32_t camera, pvec3 light_pos) {
    Ring& r = ecs.get_component<Ring>(entity);
    Transform& t = ecs.get_component<Transform>(entity);
    
    Transform& camera_transform = ecs.get_component<Transform>(camera);
    Camera& camera_camera = ecs.get_component<Camera>(camera);

    pvec3 apos;
    glm::vec3 light_direction = (vec3(light_pos - t.position));

    apos = -light_direction;

    mat4 view = mat4(transpose(camera_transform.orientation));

    glm::mat4 model_0 = t.orientation;
    glm::mat4 model_1 = core.get_model_matrix(t.position, camera_transform.position);
    //glm::mat4 model_2 = scale(vec3(2, 2, 2));

    mat4 model = model_1 * model_0;// * model_2;
    

    if(length(light_direction) != 0.0) light_direction = vec3(0.0);

    core.shaders["model_shader"]->use();
    r.texture->bind(0);

    r.buffer.bind();

    vec3 apos2 = apos;

    glUniformMatrix4fv(0, 1, false, &camera_camera.proj[0][0]);
    glUniformMatrix4fv(1, 1, false, &view[0][0]);
    glUniformMatrix4fv(2, 1, false, &model[0][0]);
    glUniform3fv(3, 1, &light_direction[0]);
    glUniform3fv(4, 1, &apos2[0]);
    glUniform1f(5, 0.35f);

    r.buffer.draw_vertices(GL_TRIANGLES);
}*/

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

    for(uint32_t camera : collectors[0].entities) {
        Camera& camera_camera = ecs.get_component<Camera>(camera);

        for(uint32_t entity : collectors[1].entities) {
            render_object(entity, camera);
        }

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

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    Input_system& input_system = ecs.get_system<Input_system>();

    if(!input_system.cursor_disabled) {
        Vertices vertices;

        vec2 size = {10, 16};
        vec4 tex_range = {0, 0, 5, 8};

        std::vector<Texture_vertex> v = {
            Texture_vertex({0, -size.y, 0.5}, tex_range.xy()),
            Texture_vertex({size.x, -size.y, 0.5}, tex_range.xy() + vec2(tex_range.z, 0)),
            Texture_vertex({0, 0, 0.5}, tex_range.xy() + vec2(0, tex_range.w)),
            Texture_vertex({size.x, 0, 0.5}, tex_range.xy() + vec2(tex_range.z, tex_range.w)),
        };
        std::shared_ptr<Texture> texture = core.textures["cursor"];

        for(Texture_vertex& vv : v) {
            vv.tex /= vec2(texture->size.xy());
        }

        v = {v[0], v[1], v[3], v[0], v[3], v[2]};

        vertices.init();
        vertices.vertex_buffer_data(v.data(), v.size(), sizeof(Texture_vertex), GL_STREAM_DRAW);

        vertices.add_vertex_attribute(0, 3, GL_FLOAT, false, sizeof(Texture_vertex), 0);
        vertices.add_vertex_attribute(1, 2, GL_FLOAT, false, sizeof(Texture_vertex), 3 * sizeof(float));
    
        uint32_t camera = *collectors[0].entities.begin();

        Transform ct = ecs.get_component<Transform>(camera);
        Camera& cc = ecs.get_component<Camera>(camera);
        
        std::shared_ptr<Shader> shader = core.shaders["texture_shader"];

        

        glm::vec2 half_viewport_size = vec2(core.window.viewport_size) / 2.0f;

        mat4 view = glm::translate(vec3{-1, -1, 0.0f}) * glm::scale(glm::vec3{1.0 / half_viewport_size.x, 1.0 / half_viewport_size.y, 1.0f});
        mat4 model = glm::translate(glm::vec3(input_system.cursor_pos.x, input_system.cursor_pos.y, 0.0f));
        mat4 proj = identity<mat4>();

        shader->use();
        texture->bind(0);
        vertices.bind();

        glUniformMatrix4fv(0, 1, false, &view[0][0]);
        glUniformMatrix4fv(1, 1, false, &proj[0][0]);
        glUniformMatrix4fv(2, 1, false, &model[0][0]);

        vertices.draw_vertices(GL_TRIANGLES);
    }
    
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);


    glfwSwapBuffers(core.window.window);
}