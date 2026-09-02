#include "component.h"
#define _CRT_SECURE_NO_WARNINGS

#include "entity.h"
#include "nlohmann/json.hpp"

void MeshComponent::serialize(nlohmann::json& json) const {
    json["segments"] = segments;
    json["type"] = static_cast<int>(type);
    
    if (!asset_name.empty()) {
        json["asset_name"] = asset_name;
    }

    json["is_editable_mesh"] = is_editable_mesh;
    json["editable_vertices"] = nlohmann::json::array();

    for (auto& v : editable_mesh.vertices) {
        json["editable_vertices"].push_back({
            v.position.x,
            v.position.y,
            v.position.z,
            v.u,
            v.v
        });
    }

    json["editable_triangles"] = nlohmann::json::array();

    for (auto& t : editable_mesh.triangles) {
        json["editable_triangles"].push_back({
            t.a,
            t.b,
            t.c
        });
    }
}

void MeshComponent::deserialize(const nlohmann::json& json) {
    if (json.contains("segments")) segments = json["segments"];
    if (json.contains("type")) type = static_cast<ObjectType>(json["type"].get<int>());
    
    if (json.contains("asset_name")) {
        asset_name = json["asset_name"];
    }

    if (json.contains("is_editable_mesh")) {
        is_editable_mesh = json["is_editable_mesh"];
    }

    if (json.contains("editable_vertices"))
    {
        editable_mesh.vertices.clear();
        for (auto& v : json["editable_vertices"])
        {
            EditableVertex vert;
            vert.position = {
                v[0],
                v[1],
                v[2]
            };
            if (v.size() >= 5) {
                vert.u = v[3];
                vert.v = v[4];
            }

            editable_mesh.vertices.push_back(vert);
        }
    }

    if (json.contains("editable_triangles"))
    {
        editable_mesh.triangles.clear();
        for (auto& t : json["editable_triangles"])
        {
            EditableTriangle tri;

            tri.a = t[0];
            tri.b = t[1];
            tri.c = t[2];

            editable_mesh.triangles.push_back(tri);
        }
    }
}

void LightComponent::serialize(nlohmann::json& json) const {
    json["light_enabled"] = light.enabled;
    json["light_position"] = {light.position.x, light.position.y, light.position.z};
    json["light_target"] = {light.target.x, light.target.y, light.target.z};
    json["light_rotation"] = {light.rotation.x, light.rotation.y, light.rotation.z};
    
    char color_buf[16];
    snprintf(color_buf, sizeof(color_buf), "%02X%02X%02X%02X", light.color.r, light.color.g, light.color.b, light.color.a);
    json["light_color"] = std::string(color_buf);
    
    json["light_intensity"] = light.intensity;
    json["light_range"] = light.range;
    json["light_spot_angle"] = light.spot_angle;
    json["light_type"] = light.light.type;
}

LightComponent::LightComponent() : Component(COMPONENT_LIGHT, "Light"), created(false) {
    light = create_lighting({0, 0, 0}, WHITE);
}

void LightComponent::deserialize(const nlohmann::json& json) {
    if (json.contains("light_enabled")) light.enabled = json["light_enabled"];
    if (json.contains("light_position")) {
        auto& p = json["light_position"];
        light.position = {p[0], p[1], p[2]};
    }
    if (json.contains("light_target")) {
        auto& t = json["light_target"];
        light.target = {t[0], t[1], t[2]};
    }
    if (json.contains("light_rotation")) {
        auto& r = json["light_rotation"];
        light.rotation = {r[0], r[1], r[2]};
    }
    if (json.contains("light_color")) {
        std::string hex = json["light_color"];
        unsigned int rgba = std::stoul(hex, nullptr, 16);
        light.color = {
            static_cast<unsigned char>((rgba >> 24) & 0xFF),
            static_cast<unsigned char>((rgba >> 16) & 0xFF),
            static_cast<unsigned char>((rgba >> 8) & 0xFF),
            static_cast<unsigned char>(rgba & 0xFF)
        };
    }
    if (json.contains("light_intensity")) light.intensity = json["light_intensity"];
    if (json.contains("light_range")) light.range = json["light_range"];
    if (json.contains("light_spot_angle")) light.spot_angle = json["light_spot_angle"];
    if (json.contains("light_type")) light.light.type = json["light_type"];
}

void LightComponent::on_entity_transform_changed() {}

void CollisionComponent::serialize(nlohmann::json& json) const {
    json["collider_type"] = static_cast<int>(collider_type);
    json["is_trigger"] = is_trigger;

    json["size"] = {size.x, size.y, size.z};
    json["radius"] = radius;
    json["height"] = height;

    json["center"] = {center.x, center.y, center.z};
}

void CollisionComponent::deserialize(const nlohmann::json& json) {
    if (json.contains("collider_type")) collider_type = static_cast<ColliderType>(json["collider_type"].get<int>());
    if (json.contains("is_trigger")) is_trigger = json["is_trigger"];

    if (json.contains("size")) {
        auto& s = json["size"];
        size = {s[0], s[1], s[2]};
    }

    if (json.contains("radius")) radius = json["radius"];
    if (json.contains("height")) height = json["height"];
    
    if (json.contains("center")) {
        auto& c = json["center"];
        center = {c[0], c[1], c[2]};
    }

    dirty = true;
}

void CollisionComponent::on_entity_transform_changed() {
    dirty = true;
}

void ComponentManager::deserialize(const nlohmann::json& json) {
    if (!json.contains("components")) return;
    
    components.clear();
    for (const auto& comp_json : json["components"]) {
        std::string type_name = comp_json["type"];
        bool comp_enabled = comp_json.value("enabled", true);
        
        std::shared_ptr<Component> comp;
        
        if (type_name == "Transform") {
            auto transform = std::make_shared<TransformComponent>();
            if (comp_json.contains("data")) {
                transform->deserialize(comp_json["data"]);
            }
            comp = transform;
        }
        else if (type_name == "Mesh") {
            auto mesh = std::make_shared<MeshComponent>();
            if (comp_json.contains("data")) {
                mesh->deserialize(comp_json["data"]);
            }
            comp = mesh;
        }
        else if (type_name == "Material") {
            auto mat = std::make_shared<MaterialComponent>();
            if (comp_json.contains("data")) {
                mat->deserialize(comp_json["data"]);
            }
            comp = mat;
        }
        else if (type_name == "Light") {
            auto light = std::make_shared<LightComponent>();
            if (comp_json.contains("data")) {
                light->deserialize(comp_json["data"]);
            }
            comp = light;
        }
        else if (type_name == "Text" || type_name == "3D Text") {
            auto text = std::make_shared<Text3DComponent>();
            if (comp_json.contains("data")) {
                text->deserialize(comp_json["data"]);
            }
            comp = text;
        }
        
        if (comp) {
            comp->enabled = comp_enabled;
            components.push_back(comp);
        }
    }
}

ComponentManager* Entity::get_components() {
    return components.get();
}

const ComponentManager* Entity::get_components() const {
    return components.get();
}

TransformComponent* Entity::get_transform_component() { return components ? components->get_transform() : nullptr; }
const TransformComponent* Entity::get_transform_component() const { return components ? components->get_transform() : nullptr; }

MeshComponent* Entity::get_mesh_component() { return components ? components->get_mesh() : nullptr; }
const MeshComponent* Entity::get_mesh_component() const { return components ? components->get_mesh() : nullptr; }

LightComponent* Entity::get_light_component() { return components ? components->get_light() : nullptr; }
const LightComponent* Entity::get_light_component() const { return components ? components->get_light() : nullptr; }

MaterialComponent* Entity::get_material_component() { return components ? components->get_material() : nullptr; }
const MaterialComponent* Entity::get_material_component() const { return components ? components->get_material() : nullptr; }

CollisionComponent* Entity::get_collision_component() { return components ? components->get_collision() : nullptr; }
const CollisionComponent* Entity::get_collision_component() const { return components ? components->get_collision() : nullptr; }