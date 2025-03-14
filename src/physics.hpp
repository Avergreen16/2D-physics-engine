#pragma once

#include "wrapper.hpp"
#include "ecs.hpp"

struct Collider {
    std::vector<vec2> vertices;
    float radius = 0.0f;

    bool colliding = false;
};

struct Collision_data {
    uint32_t a;
    uint32_t b;

    vec2 pa;
    vec2 pb;
};

struct Physics_system : System {
    Physics_system();

    std::optional<Collision_data> collision(Collider& ca, Transform& ta, Collider& cb, Transform& tb);

    void transform_vertices(Transform& t, Collider& c, std::vector<vec2>& vertices, vec2 origin);

    vec2 support_func(std::vector<vec2>& vertices, float radius, vec2 direction);

    void call();
};