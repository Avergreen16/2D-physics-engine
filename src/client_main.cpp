#include "core.hpp"
#include "render.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "gui.hpp"

#include <chrono>
#include <windows.h>
using namespace std::chrono;

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

std::array<glm::vec2, 49> random_pts;

void Core::init() {
    start_time = get_time();
    prev_time = start_time;

    window = Window({800, 600});
    window.init_callbacks();
    glfwSetWindowUserPointer(window.window, this);

    for(int i = 0; i < 49; ++i) {
        random_pts[i] = glm::vec2(random() - 0.5, random() - 0.5);
    }

    stbi_set_flip_vertically_on_load(true);

    shaders.emplace("color_shader", std::make_shared<Shader>(Shader("src/shaders/color.vert", "src/shaders/color.frag")));
    shaders.emplace("texture_shader", std::make_shared<Shader>(Shader("src/shaders/texture.vert", "src/shaders/texture.frag")));
    shaders.emplace("screen_shader", std::make_shared<Shader>(Shader("src/shaders/screen.vert", "src/shaders/screen.frag")));
    shaders.emplace("gui_shader", std::make_shared<Shader>(Shader("src/shaders/ui.vert", "src/shaders/ui.frag")));
    textures.emplace("gui_texture", std::make_shared<Texture>(Texture("res/textures/gui.png", {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, 4)));
    textures.emplace("text_texture", std::make_shared<Texture>(Texture("res/textures/text.png", {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}, 4)));
}

struct Time {
    std::chrono::steady_clock::time_point last_time;

    void overwrite() {
        last_time = steady_clock::now();
    }

    Time() {
        overwrite();
    }

    double get_elapsed_time(bool overwrite = false) {
        steady_clock::time_point current_time = steady_clock::now();
        steady_clock::duration duration = current_time - last_time;
    
        if(overwrite) {
            last_time = current_time;
        }
    
        return double(duration.count()) * steady_clock::period::num / steady_clock::period::den;
    }
};

int main() {
    if(glfwInit() == GLFW_FALSE) {
        std::cout << "ERROR: GLFW failed to load.\n";
        exit(-1);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    core.init();
    
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);


    ecs.entity_manager.init();

    ecs.register_component<Mesh>();
    ecs.register_component<Transform>();
    ecs.register_component<Camera>();
    ecs.register_component<Collider>();
    ecs.register_component<Widget>();
    ecs.register_component<Window_widget>();
    ecs.register_component<Text>();
    ecs.register_component<Button>();
    ecs.register_component<Tab>();
    ecs.register_component<Panel>();
    ecs.register_component<Text_input>();
    ecs.register_component<Scrollbar>();

    ecs.register_system<Input_system>();
    ecs.register_system<GUI_system>();
    ecs.register_system<Physics_system>();
    ecs.register_system<Render_system>();

    Input_system& input_system = ecs.get_system<Input_system>();
    Physics_system& physics_system = ecs.get_system<Physics_system>();

    vec2 planet_radius = vec2(96.0f, 128.0f);
    physics_system.gravity_aspect = planet_radius;


    uint32_t entity = ecs.insert_entity();

    Transform t;
    t.position = vec2(0.0f);
    t.orientation = identity<mat2>();
    std::vector<vec2> square = {
        vec2(-1, -1),
        vec2(1, -1),
        vec2(1, 1),
        vec2(-1, 1)
    };

    Collider c;
    c.radius = vec2(0.0f);
    c.vertices = square;
    for(vec2& v : c.vertices) v *= vec2(512, 1);
    /*c.radius = planet_radius;
    c.vertices = {
        vec2(0.0f)
    };*/

    c.is_static = true;
    
    Mesh m;
    create_mesh(m, {c.vertices}, c.radius);

    ecs.insert_component(entity, m);
    ecs.insert_component(entity, t);
    ecs.insert_component(entity, c);



    entity = ecs.insert_entity();

    Camera cc;
    cc.scale = 1.0f;
    
    t.position = vec2(0.0f, 0.0f);

    ecs.insert_component(entity, t);
    ecs.insert_component(entity, cc);

    // widgets

    GUI_system& gui_system = ecs.get_system<GUI_system>();

    gui_system.add_window(ivec2(20, 20), ivec2(80, 60), "Test Window");
    gui_system.add_text(fps_callback);
    gui_system.add_text(physics_callback);

    /*gui_system.add_scrollbar(10, 20);
    for(int i = 0; i < 100; ++i) {
        std::string string = std::to_string(i);
        gui_system.add_tab(ivec2(60, 15), "TAB" + string, true);
        gui_system.widget_return();
    }
    gui_system.widget_return();*/
    

    glfwSetInputMode(core.window.window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    Time time;

    while(core.game_running) {
        float t = time.get_elapsed_time(true);
        /*if(t < 0.016) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(int(1000 * (0.016f - t))));
        }*/

        core.random();
        
        core.delta_time = core.get_delta_time();
        
        glfwPollEvents();

        double time = get_time();

        for(std::string& name : ecs.system_manager.call_order) {
            //sstd::cout << name;
            auto& system = ecs.system_manager.systems[name];
            system->call();

            double new_time = get_time();
            double diff_time = new_time - time;

            //std::cout << diff_time << "\n";
            time = new_time;
        }

        core.events.clear();
        
        if(glfwWindowShouldClose(core.window.window)) {
            core.game_running = false;
        }
    }

    glfwTerminate();
    return 0;
}