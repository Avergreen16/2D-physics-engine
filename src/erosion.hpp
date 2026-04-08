#pragma once

#include "ecs.hpp"
#include "wrapper.hpp"

struct terrain_tile {
    ivec2 self;
    std::vector<float> distances;
    bool mark = false;

    float flow = 0.0f;
    float flow_water = 0.0f;
    ivec2 downstream = ivec2(-1, -1);
    uint32_t basin = 0xFFFFFFFF;
    std::vector<ivec2> upstream;

    std::vector<ivec2> neighbors;
    float elevation = 0.0f;
    float water = 0.0f;
    float sediment = 0.0f;

    float water_temp = 0.0f;
    float elevation_temp = 0.0f;
    float sediment_temp = 0.0f;

    float delta_flow = 0.0f;
    float erosion_amount = 0.0f;

    vec2 water_velocity = vec2(0.0f);

    float carry_capacity = 0.0f;

    std::vector<float> outflow;
};

enum map_mode{MAP_MODE_ELEVATION, MAP_MODE_SHADE, MAP_MODE_FLOW, MAP_MODE_BASIN};

struct Erosion_system : System {
    bool run_sim = false;
    
    std::vector<terrain_tile> tiles;
    ivec2 size;
    std::shared_ptr<Texture> map_texture;
    std::vector<uint8_t> texture;

    std::vector<ivec2> path;

    map_mode mode = MAP_MODE_ELEVATION;

    std::string prev_filepath;

    Erosion_system();

    void call();

    void sim_step();

    void update_texture();

    void init_map(ivec2 size);

    void write_heightmap();

    void read_heightmap();
};