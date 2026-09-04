#include "editor/editor_entity.h"
#include "editor/editor_assets.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "nlohmann/json.hpp"
#include "models.h"
#include "entity.h"
#include <fstream>

void assign_entity_name(Entity& entity, const char* new_name) {
    if (!new_name || new_name[0] == '\0') return;
    entity.name = new_name;
}

Entity make_entity_from_asset(Scene& scene, ModelAsset& asset) {
    Entity entity;
    MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh) return entity;

    entity.id = static_cast<int>(scene.entities.size());
    mesh->type = asset.type;
    mesh->asset = &asset;
    mesh->asset_name = asset.name;
    mesh->segments = 16;
    const std::string base_name = asset.is_procedural ? object_type_name(asset.type) : fs::path(asset.name).stem().string();
    entity.name = scene.make_unique_name(base_name.empty() ? "Model" : base_name);

    auto mat_comp = std::make_shared<MaterialComponent>();
    entity.get_components()->add_component(mat_comp);
    MaterialComponent* mat = mat_comp.get();

    if (asset.is_procedural) {
        mesh->model = asset.generator(mesh->segments);
        mesh->owns_model_instance = true;
        clear_mesh_overrides(entity);
        store_uv(&entity);
        store_material_textures(&entity);
        mat->texture_source = TEXTURE_NONE;
        mat->texture_name.clear();

        if (asset.name == "Text") {
            auto text_comp = std::make_shared<Text3DComponent>();
            entity.get_components()->add_component(text_comp);
        }
    } 

    else {
        if (!load_model_instance(asset, mesh->model)) {
            mesh->asset = nullptr;
            mesh->asset_name.clear();
            mesh->model;
            return entity;
        }

        mesh->owns_model_instance = true;
        clear_mesh_overrides(entity);
        store_uv(&entity);
        store_material_textures(&entity);

        bool has_embedded = false;
        for (int i = 0; i < mesh->model.materialCount; i++) {
            if (mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture.id != 0) {
                has_embedded = true;
                break;
            }
        }

        mat->texture_source = has_embedded ? TEXTURE_MODEL : TEXTURE_NONE;

        if (!asset.filepath.empty()) {
            std::filesystem::path model_path(asset.filepath);
            std::filesystem::path mtl_path = model_path.parent_path() / (model_path.stem().string() + ".mtl");
            
            if (std::filesystem::exists(mtl_path)) {
                load_material_to_entity(&entity, mtl_path);
                return entity;
            }
        }
    }

    mat->texture = {0};
    return entity;
}

void make_prefab(Entity entity, const fs::path path) {
    nlohmann::json j;

    j["name"] = entity.name;
    j["tags"] = entity.tags;
    j["is_group"] = entity.is_group;
    j["parent_id"] = entity.parent_id;

    if (entity.components) {
        entity.components->serialize(j);
    }

    std::ofstream f(path / (entity.name + ".prefab"));

    if (!f.is_open()) {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to open %s.prefab ", entity.name.c_str()));
        return;
    }

    f << j.dump(4);
    f.close();
}

Entity make_entity_from_prefab(Scene& scene, const fs::path filename) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        TraceLog(LogLevel::Error, "PREFAB", TextFormat("Failed to open prefab %s", filename.string().c_str()));
        return {};
    }

    nlohmann::json j;
    f >> j;

    Entity entity;
    
    entity.name = j.value("name", "Entity");
    if (j.contains("tags") && j["tags"].is_array()) {
        entity.tags = j["tags"].get<std::vector<std::string>>();
    }
    entity.is_group = j.value("is_group", false);
    entity.parent_id = j.value("parent_id", -1);
    entity.id = static_cast<int>(scene.entities.size());

    if (j.contains("components")) {
        entity.components->deserialize(j);
    }

    auto mesh = entity.get_mesh_component();
    if (mesh && !mesh->asset_name.empty()) {
        load_model_instance(*find_asset_by_name(mesh->asset_name), mesh->model);
    }

    auto mat = entity.get_material_component();
    if (mat && !mat->texture_name.empty()) {
        load_material_to_entity(&entity, mat->texture_name);
    }

    TraceLog(LogLevel::Info, "PREFAB", TextFormat("[TEXTURE_NAME] %s", mat->texture_name.c_str()));

    return entity;
}