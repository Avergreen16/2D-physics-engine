#pragma once
#include "ecs.hpp"
#include "random.hpp"
#include "gui.hpp"
//#include "physics.hpp"

struct Mesh {
    std::vector<vec2> v;
    std::vector<uint32_t> i;

    vec3 color = vec3(1.0f, 0.3f, 0.3f);

    std::shared_ptr<Vertices> vertices;
};

struct Texture_vertex {
    vec3 pos;
    vec2 tex;
};

void create_mesh(Mesh& m, std::vector<vec2> v, vec2 radius);

struct Render_system : System {
    std::vector<Framebuffer> framebuffers;
    std::vector<float> framebuffer_link;
    uvec2 screen_size = {800, 600};
    
    std::vector<vec2> marker_points;

    std::shared_ptr<Vertices> vv = std::shared_ptr<Vertices>(new Vertices);

    Render_system();

    void resize_framebuffers();

    void bind_framebuffer(uint32_t i);

    void bind_default_framebuffer();

    void render_object(uint32_t object, uint32_t camera);
    
    void render_marker(vec2 pos, uint32_t camera);

    void render_cursor();

    void call();
};