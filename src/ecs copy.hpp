#pragma once;
#include <iostream>
#include <bitset>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <typeinfo>
#include <queue>
#include <array>

#include "model.hpp"
#include "wrapper.hpp"
#include "pnum.hpp"

struct Mesh_component {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Texture> texture;
    bool cull = true;
};

const uint32_t MAX_COMPONENTS = 0xFF;
const uint32_t MAX_ENTITIES = 0xFFFF;
const uint32_t NULL_ENTITY = 0xFFFFFFFF;
using Signature = std::bitset<MAX_COMPONENTS>;

struct Entity {
    uint32_t id;
    Signature signature = 0;
    
    Entity() {}
    
    Entity(uint32_t i) {
        id = i;
    }
};

std::size_t hash_entity(const Entity& e);

struct Entity_manager {
    std::queue<uint32_t> available_ids;
    std::unordered_set<uint32_t> entities;
    std::array<Signature, MAX_ENTITIES> signatures;
    
    void init();
    
    int insert_entity();
    
    void erase_entity(uint32_t entity);
};

struct Clist {
    virtual void remove_component(uint32_t entity) = 0;
};

template<typename Type> 
struct Component_list : Clist {
    std::unordered_map<uint32_t, uint32_t> entity_to_component;
    std::unordered_map<uint32_t, uint32_t> component_to_entity;
    std::vector<std::shared_ptr<Type>> components;
    
    void insert_component(uint32_t entity, Type component) {
        if(entity_to_component.find(entity) == entity_to_component.end()) {
            uint32_t i = components.size();
            
            entity_to_component.emplace(entity, i);
            component_to_entity.emplace(i, entity);
            components.push_back(std::make_shared<Type>(component));
        }
    }

    void insert_component_move(uint32_t entity, Type&& component) {
        if(entity_to_component.find(entity) == entity_to_component.end()) {
            uint32_t i = components.size();
            
            entity_to_component.emplace(entity, i);
            component_to_entity.emplace(i, entity);
            components.push_back(std::make_shared<Type>(std::move(component)));
        }
    }
    
    // complicated part, needs to be simplified and commented
    void remove_component(uint32_t entity) {
        uint32_t i = entity_to_component[entity]; // component of entity being deleted
        
        entity_to_component.erase(entity);
        
        uint32_t j = components.size() - 1; // last component index in vector
        
        if(i != j) {
            uint32_t k = component_to_entity[j]; // k is entity id with a component at j
            
            component_to_entity.erase(j);
            
            component_to_entity[i] = k;
            
            entity_to_component[k] = i;
            
            components[i] = std::move(components[j]);
            components.erase(components.begin() + j);
        } else {
            component_to_entity.erase(i);
            components.erase(components.begin() + i);
        }
        
        //components.resize(j);
    }
    
    Type& get_component(uint32_t& entity) {
        uint32_t i = entity_to_component[entity];
        
        return *components[i].get();
    }
};

struct Component_manager {
    std::unordered_map<uint32_t, std::size_t> id_to_code;
    std::unordered_map<std::size_t, uint32_t> code_to_id;
    std::unordered_map<std::size_t, std::shared_ptr<Clist>> component_lists;
    
    template<typename Type>
    void register_component() {
        std::size_t code = typeid(Type).hash_code();

        if(!code_to_id.contains(code)) {
            uint32_t i = id_to_code.size();
            
            id_to_code.emplace(i, code);
            code_to_id.emplace(code, i);
            
            component_lists.emplace(code, std::shared_ptr<Component_list<Type>>(new Component_list<Type>));
        }
    }
    
    template<typename Type>
    Component_list<Type>& get_component_array() {
        std::size_t ti = typeid(Type).hash_code();
        return *(Component_list<Type>*)(component_lists[ti].get());
    }
    
    void delete_components(uint32_t entity, Signature signature) {
        for(int i = 0; i < MAX_COMPONENTS; ++i) {
            std::bitset<MAX_COMPONENTS> a = (Signature)1 << i;
            std::bitset<MAX_COMPONENTS> b = signature;
            
            if((b & a) == a) {
                std::size_t ti = id_to_code[i];
                auto& list = component_lists[ti];
                
                list->remove_component(entity);
            }
        }
    }
    
    template<typename Type>
    void insert_component(uint32_t entity, Signature& signature, Type t) {
        std::size_t code = typeid(Type).hash_code();
        uint32_t i = code_to_id[code];

        Signature s2 = Signature(1) << i;
        signature |= s2;
        
        (*(Component_list<Type>*)(component_lists[code].get())).insert_component(entity, t);
    }

    template<typename Type>
    void insert_component_move(uint32_t entity, Signature& signature, Type&& t) {
        std::size_t code = typeid(Type).hash_code();
        uint32_t i = code_to_id[code];

        Signature s2 = Signature(1) << i;
        signature |= s2;
        
        (*(Component_list<Type>*)(component_lists[code].get())).insert_component_move(entity, std::move(t));
    }
    
    template<typename Type>
    Type& get_component(uint32_t entity) {
        auto& cl = get_component_array<Type>();
        
        return cl.get_component(entity);
    }
};

struct Collector {
    std::bitset<MAX_COMPONENTS> signature = 0;
    bool greedy = true;

    std::unordered_set<uint32_t> entities;
};

struct System {
    std::vector<Collector> collectors;
    virtual void call() {};
    virtual void init() {};
};

struct Camera {
    mat4 proj;

    bool orthogonal = false;
    float fov;
    float near_plane;
    float far_plane;

    uint32_t framebuffer = 0;
};

struct System_manager {
    std::unordered_map<std::size_t, std::shared_ptr<System>> systems;
    std::vector<std::size_t> call_order;
    
    template<typename Type>
    void register_system() {
        std::size_t code = typeid(Type).hash_code();
        
        std::shared_ptr<Type> ptr = std::shared_ptr<Type>(new Type);
        systems.emplace(code, ptr);
        call_order.push_back(code);

        ptr->init();
    }
};

struct Coordinator {
    Entity_manager entity_manager;
    Component_manager component_manager;
    System_manager system_manager;

    std::atomic<uint32_t> num_lookups = 0;
    
    template<typename Type>
    void register_component() {
        component_manager.register_component<Type>();
    }
    
    template<typename Type>
    void register_system() {
        system_manager.register_system<Type>();
    }
    
    int insert_entity() {
        return entity_manager.insert_entity();
    }    
    
    template<typename Type>
    std::bitset<MAX_COMPONENTS> update_signature() {
        register_component<Type>();

        std::size_t code = typeid(Type).hash_code();
        return Signature(1) << component_manager.code_to_id[code];
    }
    
    template<typename Type>
    void update_signature(std::bitset<MAX_COMPONENTS>& a) {
        register_component<Type>();

        std::size_t code = typeid(Type).hash_code();
        a |= Signature(1) << component_manager.code_to_id[code];
    }

    template<typename Type>
    bool has_component(uint32_t entity) {
        return (entity_manager.signatures[entity] & update_signature<Type>()) != Signature(0);
    }
    
    template<typename Type>
    bool has_system() {
        return system_manager.systems.contains(typeid(Type).hash_code());
    }

    template<typename Type>
    Type& get_component(uint32_t entity) {
        ++num_lookups;
        return component_manager.get_component<Type>(entity);
    }
    
    template<typename Type>
    void insert_component(uint32_t entity, Type component) {
        register_component<Type>();

        component_manager.insert_component(entity, entity_manager.signatures[entity], component);
        Signature new_signature = entity_manager.signatures[entity];
        
        for(auto& s : system_manager.systems) {
            bool remove = false;
            for(Collector& c : s.second->collectors) {
                if(!remove) {
                    Signature s_signature = c.signature;
                    if((new_signature & s_signature) == s_signature) {
                        if(c.greedy) remove = true;
                        if(c.entities.find(entity) == c.entities.end()) {
                            c.entities.emplace(entity);
                        }
                    }
                } else {
                    Signature s_signature = c.signature;
                    if((new_signature & s_signature) == s_signature) {
                        if(c.entities.find(entity) != c.entities.end()) {
                            c.entities.erase(entity);
                        }
                    }
                }
            }
        }
    }

    template<typename Type>
    void insert_component_move(uint32_t entity, Type&& component) { 
        using RawType = std::remove_reference_t<Type>;
        register_component<RawType>();

        component_manager.insert_component_move(entity, entity_manager.signatures[entity], std::move(component));
        Signature new_signature = entity_manager.signatures[entity];
        
        for(auto& s : system_manager.systems) {
            bool remove = false;
            for(Collector& c : s.second->collectors) {
                if(!remove) {
                    Signature s_signature = c.signature;
                    if((new_signature & s_signature) == s_signature) {
                        if(c.greedy) remove = true;
                        if(c.entities.find(entity) == c.entities.end()) {
                            c.entities.emplace(entity);
                        }
                    }
                } else {
                    Signature s_signature = c.signature;
                    if((new_signature & s_signature) == s_signature) {
                        if(c.entities.find(entity) != c.entities.end()) {
                            c.entities.erase(entity);
                        }
                    }
                }
            }
        }
    }
    
    void erase_entity(uint32_t entity) {
        Signature entity_signature = entity_manager.signatures[entity];
        
        component_manager.delete_components(entity, entity_manager.signatures[entity]);
        
        entity_manager.erase_entity(entity);
        
        for(auto& s : system_manager.systems) {
            for(Collector& c : s.second->collectors) {
                Signature s_signature = c.signature;
                if((entity_signature & s_signature) == s_signature) {
                    if(c.entities.find(entity) != c.entities.end()) {
                        c.entities.erase(entity);
                    }
                }
            }
        }
    }

    template<typename Type> 
    Type& get_system() {
        return *(Type*)system_manager.systems[typeid(Type).hash_code()].get();
    }
};

extern Coordinator ecs;

struct Transform {
    pvec3 position = {0, 0, 0};
    mat3 orientation = identity<mat3>();
};

template<typename Type>
struct Keyframe {
    double time;
    Type value;
};

template<typename Type>
struct K_less {
    bool operator()(const Keyframe<Type>& a, const Keyframe<Type>& b) const {
        return a.time < b.time;
    }
};

struct Color_map {
    std::shared_ptr<Texture> texture;
    vec3 center_offset;
};