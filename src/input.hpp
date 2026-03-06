#pragma once

#include "ecs.hpp"
//#include "physics.hpp"

struct Input_system : System {
    bool translate = false;

    vec2 world_cursor_pos = vec2(0.0f);

    uint32_t tethered_object;

    uint32_t pic = 0;

    uint32_t id = NULL_ENTITY;

    uint32_t held_object = NULL_ENTITY;
    uint32_t held_constraint = NULL_ENTITY;

    bool debug_physics = false;
    bool debug_mode = false;

    uint32_t snum = 0;

    Input_system();

    void call();
};