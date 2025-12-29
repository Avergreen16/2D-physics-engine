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

    int arrow_delta = 0;
    std::string char_delta;
    bool backspace = false;

    vec2 world_cursor_pos = vec2(0.0f);

    uint32_t tethered_object;

    uint32_t pic = 0;

    uint32_t id = NULL_ENTITY;

    uint32_t held_object = NULL_ENTITY;
    uint32_t held_constraint;

    bool debug_physics = false;
    bool debug_mode = false;

    uint32_t snum = 0;

    Input_system();

    void call();
};