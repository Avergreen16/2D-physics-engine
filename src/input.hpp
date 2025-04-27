#pragma once

#include "ecs.hpp"
//#include "physics.hpp"

struct Input_system : System {
    std::unordered_map<int, bool> key_map;
    vec2 cursor_pos = {0, 0};
    vec2 cursor_delta = {0, 0};
    float scroll_delta = 0;
    bool cursor_disabled = false;
    bool click = false;
    bool translate = false;

    vec2 world_cursor_pos = vec2(0.0f);

    uint32_t tethered_object;

    uint32_t pic = 0;

    uint32_t id = 0xFFFFFFFF;

    uint32_t held_object = 0xFFFFFFFF;
    uint32_t held_constraint;

    Input_system();

    void call();
};