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

#include "wrapper.hpp"

const uint32_t MAX_COMPONENTS = 256;
const uint32_t MAX_ENTITIES = 65536;
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
    std::vector<Type> components;
    
    void insert_component(uint32_t entity, Type component) {
        if(entity_to_component.find(entity) == entity_to_component.end()) {
            uint32_t i = components.size();
            
            entity_to_component.emplace(entity, i);
            component_to_entity.emplace(i, entity);
            components.push_back(component);
        }
    }

    void insert_component_move(uint32_t entity, Type&& component) {
        if(entity_to_component.find(entity) == entity_to_component.end()) {
            uint32_t i = components.size();
            
            entity_to_component.emplace(entity, i);
            component_to_entity.emplace(i, entity);
            components.emplace_back(std::move(component));
        }
    }
    
    // complicated part, needs to be simplified and commented
    void remove_component(uint32_t entity) override {
        uint32_t i = entity_to_component[entity]; // component of entity being deleted
        
        entity_to_component.erase(entity);
        
        uint32_t j = components.size() - 1; // last component index in vector
        
        if(i != j) {
            uint32_t k = component_to_entity[j]; // k is entity id with a component at j
            
            component_to_entity.erase(j);
            
            component_to_entity[i] = k;
            
            entity_to_component[k] = i;
            
            components[i] = std::move(components[j]);
        } else {
            component_to_entity.erase(i);
        }
        
        components.resize(j);
    }
    
    Type& get_component(uint32_t& entity) {
        uint32_t i = entity_to_component[entity];
        
        return components[i];
    }
};

struct Component_manager {
    std::unordered_map<uint32_t, std::string> id_to_name;
    std::unordered_map<std::string, uint32_t> name_to_id;
    std::unordered_map<std::string, std::shared_ptr<Clist>> component_lists;
    
    template<typename Type>
    void register_component() {
        std::string ti = typeid(Type).name();
        uint32_t i = id_to_name.size();
        
        id_to_name.emplace(i, ti);
        name_to_id.emplace(ti, i);
        
        component_lists.emplace(ti, std::shared_ptr<Component_list<Type>>(new Component_list<Type>));
    }
    
    template<typename Type>
    Component_list<Type>& get_component_array() {
        std::string i = typeid(Type).name();
        return *(Component_list<Type>*)(component_lists[i].get());
    }
    
    void delete_components(uint32_t entity, Signature signature) {
        for(int i = 0; i < MAX_COMPONENTS; ++i) {
            std::bitset<MAX_COMPONENTS> a = (Signature)1 << i;
            std::bitset<MAX_COMPONENTS> b = signature;
            
            if((b & a) == a) {
                std::string name = id_to_name[i];
                auto& list = component_lists[name];
                
                list->remove_component(entity);
            }
        }
    }
    
    template<typename Type>
    void insert_component(uint32_t entity, Signature& signature, Type t) {
        std::string name = typeid(Type).name();
        uint32_t i = name_to_id[name];

        Signature s2 = Signature(1) << i;
        signature |= s2;
        
        (*(Component_list<Type>*)(component_lists[name].get())).insert_component(entity, t);
    }

    template<typename Type>
    void insert_component_move(uint32_t entity, Signature& signature, Type&& t) {
        std::string name = typeid(Type).name();
        uint32_t i = name_to_id[name];

        Signature s2 = Signature(1) << i;
        signature |= s2;
        
        (*(Component_list<Type>*)(component_lists[name].get())).insert_component_move(entity, std::move(t));
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
};

struct System_manager {
    std::unordered_map<std::string, std::shared_ptr<System>> systems;
    std::vector<std::string> call_order;
    
    template<typename Type>
    void register_system() {
        std::string name = typeid(Type).name();
        
        std::shared_ptr<Type> ptr = std::shared_ptr<Type>(new Type);
        systems.emplace(name, ptr);
        call_order.push_back(name);
    }
};

struct Coordinator {
    Entity_manager entity_manager;
    Component_manager component_manager;
    System_manager system_manager;
    
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
        std::string name = typeid(Type).name();
        return Signature(1) << component_manager.name_to_id[name];
    }
    
    template<typename Type>
    void update_signature(std::bitset<MAX_COMPONENTS>& a) {
        std::string name = typeid(Type).name();
        a |= Signature(1) << component_manager.name_to_id[name];
    }

    template<typename Type>
    bool has_component(uint32_t entity) {
        return (entity_manager.signatures[entity] & update_signature<Type>()) != Signature(0);
    }

    template<typename Type>
    Type& get_component(uint32_t entity) {
        return component_manager.get_component<Type>(entity);
    }
    
    template<typename Type>
    void insert_component(uint32_t entity, Type component) { 
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
        return *(Type*)system_manager.systems[typeid(Type).name()].get();
    }
};

extern Coordinator ecs;


struct Transform {
    vec2 position = vec3(0.0f);
    mat2 orientation = identity<mat2>();
};

struct Camera {
    float scale = 1.0f;
};
