#ifndef __COMPONENT_H__
#define __COMPONENT_H__
#include "QuarkCore/QuarkCore.hpp"
using namespace qc;
#include "lighting.h"
#include "nlohmann/json.hpp"
#include "editable_mesh.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <typeinfo>

struct Entity;
struct ModelAsset;

enum ObjectType { CUBE, SPHERE, CONE, CYLINDER, HEMISPHERE, TORUS };

enum TextureSource {
    TEXTURE_NONE,
    TEXTURE_EXTERNAL,
    TEXTURE_MODEL
};

enum ComponentType {
    COMPONENT_TRANSFORM,
    COMPONENT_MESH,
    COMPONENT_MATERIAL,
    COMPONENT_LIGHT,
    COMPONENT_COLLISION,
    COMPONENT_CUSTOM
};

enum ColliderType {
    COLLIDER_BOX,
    COLLIDER_SPHERE,
    COLLIDER_CAPSULE,
    COLLIDER_MESH
};

class Component {
public:
    std::string name;
    bool enabled = true;
    ComponentType type;

    Component() = default;
    Component(ComponentType type_val, const std::string& name_val)
        : name(name_val), type(type_val) {}

    virtual ~Component() = default;
    virtual void serialize(nlohmann::json& json) const {}
    virtual void deserialize(const nlohmann::json& json) {}
    virtual ComponentType get_type() const { return COMPONENT_CUSTOM; }
    virtual std::string get_type_name() const { return "Custom"; }
    virtual void on_entity_transform_changed() {}
};

class TransformComponent : public Component {
public:
    Vec3 position = {0, 0, 0};
    Vec3 rotation = {0, 0, 0};
    Vec3 scale = {1, 1, 1};

    TransformComponent() {
        name = "Transform";
        type = COMPONENT_TRANSFORM;
    }

    ComponentType get_type() const override { return COMPONENT_TRANSFORM; }
    std::string get_type_name() const override { return "Transform"; }

    void serialize(nlohmann::json& json) const override {
        json["position"] = {position.x, position.y, position.z};
        json["rotation"] = {rotation.x, rotation.y, rotation.z};
        json["scale"] = {scale.x, scale.y, scale.z};
    }

    void deserialize(const nlohmann::json& json) override {
        if (json.contains("position")) {
            auto& p = json["position"];
            position = {p[0], p[1], p[2]};
        }
        if (json.contains("rotation")) {
            auto& r = json["rotation"];
            rotation = {r[0], r[1], r[2]};
        }
        if (json.contains("scale")) {
            auto& s = json["scale"];
            scale = {s[0], s[1], s[2]};
        }
    }
};

class MeshComponent : public Component {
public:
    Model model;
    bool owns_model_instance = false;
    ModelAsset* asset = nullptr;
    std::string asset_name;
    bool mesh_triangles_detached = false;
    std::vector<std::vector<float>> mesh_vertex_overrides;

    int segments = 16;
    ObjectType type = CUBE;

    bool shader_assigned = false;
    bool owns_materials = false;
    bool uv_dirty = true;
    bool bounds_dirty = true;
    BoundingBox cached_local_bounds = {{0, 0, 0}, {0, 0, 0}};

    EditableMesh editable_mesh;
    bool is_editable_mesh = false;
    bool vertex_gizmo = false;

    MeshComponent() {
        name = "Mesh";
    }

    ComponentType get_type() const override { return COMPONENT_MESH; }
    std::string get_type_name() const override { return "Mesh"; }

    void serialize(nlohmann::json& json) const override;
    void deserialize(const nlohmann::json& json) override;
};

class MaterialComponent : public Component {
public:
    Texture2D texture = {0};
    TextureSource texture_source = TEXTURE_NONE;
    std::string texture_name;
    std::string normal_texture_name;
    std::string roughness_texture_name;
    std::string metallic_texture_name;
    std::vector<std::string> material_slot_sources;

    Color color = WHITE;
    Color outline_color = LIGHTGRAY;

    bool auto_uv = false;
    bool texture_stretch = true;
    float texture_repeat_u = 1.0f;
    float texture_repeat_v = 1.0f;

    Vec2 uv_scale = {1, 1};
    std::vector<std::vector<float>> original_texcoords;
    std::vector<Texture2D> original_material_textures;

    MaterialComponent() {
        name = "Material";
        type = COMPONENT_MATERIAL;
    }

    ComponentType get_type() const override { return COMPONENT_MATERIAL; };
    std::string get_type_name() const override { return "Material"; };

    void serialize(nlohmann::json& json) const override {
        json["color"] = { color.r, color.g, color.b, color.a };
        json["outline_color"] = { outline_color.r, outline_color.g, outline_color.b, outline_color.a };

        json["texture_source"] = static_cast<int>(texture_source);
        json["texture_name"] = texture_name;
        json["normal_texture_name"] = normal_texture_name;
        json["roughness_texture_name"] = roughness_texture_name;
        json["metallic_texture_name"] = metallic_texture_name;
        json["material_slot_sources"] = material_slot_sources;
        json["texture_stretch"] = texture_stretch;

        json["auto_uv"] = auto_uv;
        json["repeat_u"] = texture_repeat_u;
        json["repeat_v"] = texture_repeat_v;
        json["uv_scale"] = { uv_scale.x, uv_scale.y };
    }

    void deserialize(const nlohmann::json& json) override {
        if (json.contains("color")) {
            auto& c = json["color"];
            color = { c[0], c[1], c[2], c[3] };
        }

        if (json.contains("outline_color")) {
            auto& c = json["outline_color"];
            outline_color = { c[0], c[1], c[2], c[3] };
        }

        if (json.contains("texture_source")) texture_source = static_cast<TextureSource>(json["texture_source"].get<int>());
        if (json.contains("texture_name")) texture_name = json["texture_name"];
        if (json.contains("normal_texture_name")) normal_texture_name = json["normal_texture_name"];
        if (json.contains("roughness_texture_name")) roughness_texture_name = json["roughness_texture_name"];
        if (json.contains("metallic_texture_name")) metallic_texture_name = json["metallic_texture_name"];
        if (json.contains("material_slot_sources")) material_slot_sources = json["material_slot_sources"].get<std::vector<std::string>>();
        if (json.contains("texture_stretch")) texture_stretch = json["texture_stretch"];
        
        if (json.contains("auto_uv")) auto_uv = json["auto_uv"];
        if (json.contains("repeat_u")) texture_repeat_u = json["repeat_u"];
        if (json.contains("repeat_v")) texture_repeat_v = json["repeat_v"];
        if (json.contains("uv_scale")) {
            auto& scale = json["uv_scale"];
            uv_scale = { scale[0], scale[1] };
        }
    }
};

class LightComponent : public Component {
public:
    bool created = false;
    Lighting light;
    LightComponent();

    ComponentType get_type() const override { return COMPONENT_LIGHT; }
    std::string get_type_name() const override { return "Light"; }

    void serialize(nlohmann::json& json) const override;
    void deserialize(const nlohmann::json& json) override;
    void on_entity_transform_changed() override;
};

class CollisionComponent : public Component {
public:
    ColliderType collider_type = COLLIDER_BOX;

    bool is_trigger = false;
    bool visualize = true;

    // box
    Vec3 size = {1, 1, 1};

    // sphere/capsule
    float radius = 0.5f;
    float height = 2.0f;

    Vec3 center = {0, 0, 0};

    BoundingBox world_bounds = {{0, 0, 0}, {0, 0, 0}};
    bool dirty = true;

    CollisionComponent() {
        name = "Collision";
        type = COMPONENT_COLLISION;
    }

    ComponentType get_type() const override { return COMPONENT_COLLISION; }
    std::string get_type_name() const override { return "Collision"; }

    void serialize(nlohmann::json& json) const override;
    void deserialize(const nlohmann::json& json) override;
    void on_entity_transform_changed() override;
};

class ComponentManager {
private:
    std::vector<std::shared_ptr<Component>> components;

public:
    void add_component(std::shared_ptr<Component> component) {
        components.push_back(component);
    }

    void remove_component(size_t index) {
        if (index < components.size()) {
            components.erase(components.begin() + index);
        }
    }

    std::shared_ptr<Component> get_component(size_t index) {
        if (index < components.size()) {
            return components[index];
        }
        return nullptr;
    }

    size_t get_component_count() const {
        return components.size();
    }

    template<typename T>
    std::shared_ptr<T> get_component_of_type() {
        for (auto& comp : components) {
            auto casted = std::dynamic_pointer_cast<T>(comp);
            if (casted) return casted;
        }
        return nullptr;
    }

    TransformComponent* get_transform() {
        auto comp = get_component_of_type<TransformComponent>();
        return comp ? comp.get() : nullptr;
    }

    MeshComponent* get_mesh() {
        auto comp = get_component_of_type<MeshComponent>();
        return comp ? comp.get() : nullptr;
    }

    LightComponent* get_light() {
        auto comp = get_component_of_type<LightComponent>();
        return comp ? comp.get() : nullptr;
    }

    MaterialComponent* get_material() {
        auto comp = get_component_of_type<MaterialComponent>();
        return comp ? comp.get() : nullptr;
    }

    CollisionComponent* get_collision() {
        auto comp = get_component_of_type<CollisionComponent>();
        return comp ? comp.get() : nullptr;
    }

    const std::vector<std::shared_ptr<Component>>& get_all_components() const {
        return components;
    }

    std::vector<std::shared_ptr<Component>>& get_all_components() {
        return components;
    }

    void serialize(nlohmann::json& json) const {
        json["components"] = nlohmann::json::array();
        for (const auto& comp : components) {
            nlohmann::json comp_json;
            comp_json["type"] = comp->get_type_name();
            comp_json["enabled"] = comp->enabled;
            nlohmann::json data;
            comp->serialize(data);
            comp_json["data"] = data;
            json["components"].push_back(comp_json);
        }
    }

    void deserialize(const nlohmann::json& json);
};

class Text3DComponent : public Component {
public:
    std::string text = "3D Text";
    float size = 1.0f;
    float thickness = 0.2f;
    float letter_spacing = 0.1f;
    std::string font_path;
    

    Text3DComponent() { name = "3D Text"; type = COMPONENT_CUSTOM; }

    std::string get_type_name() const override { return "3D Text"; }
    ComponentType get_type() const override { return COMPONENT_CUSTOM; }

    void serialize(nlohmann::json& j) const override {
        j["text"] = text;
        j["size"] = size;
        j["thickness"] = thickness;
        j["letter_spacing"] = letter_spacing;
        j["font_path"] = font_path;
    }

    void deserialize(const nlohmann::json& j) override {
        if (j.contains("text")) text = j["text"];
        if (j.contains("size")) size = j["size"];
        if (j.contains("thickness")) thickness = j["thickness"];
        if (j.contains("letter_spacing")) letter_spacing = j["letter_spacing"];
        if (j.contains("font_path")) font_path = j["font_path"];
    }
};

#endif // __COMPONENT_H__
