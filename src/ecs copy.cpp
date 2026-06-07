#include "ecs.hpp"

Coordinator ecs;

std::size_t hash_entity(const Entity& e) {
    return std::hash<uint32_t>()(e.id);
}

void Entity_manager::init() {
    for(int i = 0; i < MAX_ENTITIES; ++i) {
        available_ids.push(i);
    }
    
    std::fill(signatures.begin(), signatures.end(), 0);
}

int Entity_manager::insert_entity() {
    if(available_ids.size()) {
        uint32_t entity_id = available_ids.front();
        available_ids.pop();
        
        entities.emplace(entity_id);
        return entity_id;
    }
    
    return -1;
}

void Entity_manager::erase_entity(uint32_t entity) {
    entities.erase(entity);
    available_ids.push(entity);
}

