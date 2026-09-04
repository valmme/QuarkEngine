#include "entity.h"
#include "component.h"

Entity::Entity() 
    : id(0),
      name(""),
      parent_id(-1),
      is_group(false)
{
    components = std::make_unique<ComponentManager>();
    
    auto transform = std::make_shared<TransformComponent>();
    components->add_component(transform);
    
    auto mesh = std::make_shared<MeshComponent>();
    components->add_component(mesh);
}

Entity::Entity(int _id) 
    : id(_id),
      name(object_type_name(CUBE)),
      parent_id(-1),
      is_group(false)
{
    components = std::make_unique<ComponentManager>();
    
    auto transform = std::make_shared<TransformComponent>();
    components->add_component(transform);
    
    auto mesh = std::make_shared<MeshComponent>();
    components->add_component(mesh);
}

Entity::~Entity() {
}

Entity::Entity(const Entity& other)
    : id(other.id),
      name(other.name),
    tags(other.tags),
      parent_id(other.parent_id),
      is_group(other.is_group)
{
    components = std::make_unique<ComponentManager>();
    if (other.components) {
        for (size_t i = 0; i < other.components->get_component_count(); ++i) {
            auto comp = other.components->get_component(i);
            if (!comp) continue;
            
            std::shared_ptr<Component> cloned;
            if (comp->get_type() == COMPONENT_TRANSFORM) {
                auto src = std::dynamic_pointer_cast<TransformComponent>(comp);
                auto dst = std::make_shared<TransformComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_MESH) {
                auto src = std::dynamic_pointer_cast<MeshComponent>(comp);
                auto dst = std::make_shared<MeshComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_LIGHT) {
                auto src = std::dynamic_pointer_cast<LightComponent>(comp);
                auto dst = std::make_shared<LightComponent>(*src);
                cloned = dst;
            }
            else if(comp->get_type() == COMPONENT_MATERIAL) {
                auto src = std::dynamic_pointer_cast<MaterialComponent>(comp);
                auto dst = std::make_shared<MaterialComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_COLLISION) {
                auto src = std::dynamic_pointer_cast<CollisionComponent>(comp);
                auto dst = std::make_shared<CollisionComponent>(*src);
                cloned = dst;
            }
            
            if (cloned) {
                components->add_component(cloned);
            }
        }
    }
}

Entity& Entity::operator=(const Entity& other) {
    if (this == &other) return *this;
    
    id = other.id;
    name = other.name;
    tags = other.tags;
    parent_id = other.parent_id;
    is_group = other.is_group;
    
    components = std::make_unique<ComponentManager>();
    if (other.components) {
        for (size_t i = 0; i < other.components->get_component_count(); ++i) {
            auto comp = other.components->get_component(i);
            if (!comp) continue;
            
            std::shared_ptr<Component> cloned;
            if (comp->get_type() == COMPONENT_TRANSFORM) {
                auto src = std::dynamic_pointer_cast<TransformComponent>(comp);
                auto dst = std::make_shared<TransformComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_MESH) {
                auto src = std::dynamic_pointer_cast<MeshComponent>(comp);
                auto dst = std::make_shared<MeshComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_LIGHT) {
                auto src = std::dynamic_pointer_cast<LightComponent>(comp);
                auto dst = std::make_shared<LightComponent>(*src);
                cloned = dst;
            }
            else if(comp->get_type() == COMPONENT_MATERIAL) {
                auto src = std::dynamic_pointer_cast<MaterialComponent>(comp);
                auto dst = std::make_shared<MaterialComponent>(*src);
                cloned = dst;
            }
            else if (comp->get_type() == COMPONENT_COLLISION) {
                auto src = std::dynamic_pointer_cast<CollisionComponent>(comp);
                auto dst = std::make_shared<CollisionComponent>(*src);
                cloned = dst;
            }
            
            if (cloned) {
                components->add_component(cloned);
            }
        }
    }
    
    return *this;
}
