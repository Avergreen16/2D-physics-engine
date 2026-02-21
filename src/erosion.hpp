#pragma once

#include "ecs.hpp"
#include "wrapper.hpp"

struct Erosion_system : System {
    std::vector<float> elevation;
    ivec2 size;
    std::shared_ptr<Texture> map_texture;

    Erosion_system();

    void call();

    void update_texture();
};