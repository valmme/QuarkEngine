#ifndef __ENTITY_H__
#define __ENTITY_H__
#include "lighting.h"
#include "component.h"
#include <string>
#include <functional>
#include <vector>
#include <memory>

class ComponentManager;

inline const char* object_type_name(ObjectType type) {
    switch (type) {
        case CUBE: return "Cube";
        case SPHERE: return "Sphere";
        case CONE: return "Cone";
        case CYLINDER: return "Cylinder";
        case HEMISPHERE: return "HemiSphere";
        case TORUS: return "Torus";
        default: return "Object";
    }
}

struct ModelAsset {
    std::string name;
    std::string filepath;
    
    ObjectType type;
    bool is_procedural;
    std::function<Model(int)> generator;
    Model loaded_model;
};

struct Entity {
    int id;
    std::string name;
    std::vector<std::string> tags;
    int parent_id = -1;
    bool is_group = false;
    
    std::unique_ptr<ComponentManager> components;

    Entity();
    Entity(int _id);
    ~Entity();
    
    Entity(const Entity& other);
    Entity& operator=(const Entity& other);
    
    Entity(Entity&& other) noexcept = default;
    Entity& operator=(Entity&& other) noexcept = default;

    ComponentManager* get_components();
    const ComponentManager* get_components() const;
    TransformComponent* get_transform_component();
    const TransformComponent* get_transform_component() const;
    MeshComponent* get_mesh_component();
    const MeshComponent* get_mesh_component() const;
    LightComponent* get_light_component();
    const LightComponent* get_light_component() const;
    MaterialComponent* get_material_component();
    const MaterialComponent* get_material_component() const;
    CollisionComponent* get_collision_component();
    const CollisionComponent* get_collision_component() const;
};

#endif // __ENTITY_H__
