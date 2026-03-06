
#pragma once

#include "core.hpp"
#include "render.hpp"
#include "wrapper.hpp"

vec2 map_project(ivec2 position, ivec2 size);
vec3 wrap(vec2 position);

extern std::vector<ivec2> indices;

extern std::string filepath;

struct voronoi_cell {
    vec3 pos;
    uint32_t plate = 0xFFFFFFFF;
};

struct Blob {
    vec3 origin;
    vec3 size;
    mat3 ori;
};

struct tectonic_plate {
    vec3 movement_dir;
    float elev;
    vec3 position;
    float weight;
    
    float temp_elev = 0.0f;
    
    std::vector<uint32_t> cells;
    std::set<uint32_t> frontier;

    std::vector<Blob> blobs;

    float stored_elev = 0.0f;
};

struct Connection {
    uint32_t a;
    uint32_t b;
    uint32_t c = 0xFFFFFFFF;
    uint32_t d = 0xFFFFFFFF;
    float convergence = 0.0f;
    vec3 vel_a;
    vec3 vel_b;

    float weight_a;
    float weight_b;
};

struct Cell {
    float elevation = 0.0f;
    bool touched = false;
};

extern std::vector<Cell> c2;

vec3 find_circumcenter(vec3 a, vec3 b, vec3 c);

vec3 get_color_elev(float elevation);

vec3 hash_vec3(uint32_t a, uint32_t b);

void generate_planet_texture(uint32_t seed);

std::vector<Cell> simulate_erosion(ivec2 terrain_size);

void create_erosion_mesh(std::vector<Cell> cells, uvec2 terrain_size, Transform transform);

void create_europa();