#pragma once

#include "ecs.hpp"
#include "wrapper.hpp"

struct terrain_tile {
    ivec2 self;
    std::vector<ivec2> neighbors;
    std::vector<float> distances;

    float elevation;
    float water = 0.0f;
    float sediment = 0.0f;

    bool mark = false;

    float flow = 0.0f;

    ivec2 downstream;
    uint32_t basin = 0xFFFFFFFF;
    std::vector<ivec2> upstream;
};

enum map_mode{MAP_MODE_ELEVATION, MAP_MODE_FLOW, MAP_MODE_BASIN};

struct Erosion_system : System {
    std::vector<terrain_tile> tiles;
    ivec2 size;
    std::shared_ptr<Texture> map_texture;

    map_mode mode = MAP_MODE_ELEVATION;

    Erosion_system();

    void call();

    void sim_step();

    void update_texture();
};