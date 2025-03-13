#pragma once

#include "ecs.hpp"
//#include "physics.hpp"

struct Input_system : System {
    std::unordered_map<int, bool> key_map;
    vec2 cursor_pos = {0, 0};
    float scroll_delta = 0;
    bool cursor_disabled = false;

    vec2 world_cursor_pos = vec2(0.0f);

    uint32_t pic = 0;

    uint32_t id = 0xFFFFFFFF;

    Input_system();

    void call();
};