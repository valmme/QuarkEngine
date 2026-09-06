#include "scene.h"
#include "models.h"
#include <algorithm>
#include <unordered_set>

Entity* Scene::get_selected() {
    if (selected < 0 || selected >= static_cast<int>(entities.size())) return nullptr;
    return &entities[selected];
}

bool Scene::is_selected(int entity_index) const {
    return std::find(selected_entities.begin(), selected_entities.end(), entity_index) != selected_entities.end();
}

void Scene::select_entity(int entity_index, bool additive) {
    if (entity_index < 0 || entity_index >= static_cast<int>(entities.size())) return;
    if (!additive) selected_entities.clear();

    auto it = std::find(selected_entities.begin(), selected_entities.end(), entity_index);
    if (additive && it != selected_entities.end()) {
        selected_entities.erase(it);
        selected = selected_entities.empty() ? -1 : selected_entities.back();
        return;
    }

    if (it == selected_entities.end()) selected_entities.push_back(entity_index);
    selected = entity_index;
}

std::string Scene::make_unique_name(const std::string& base_name) const {
    auto is_name_taken = [this](const std::string& candidate) {
        for (const auto& entity : entities) {
            if (entity.name == candidate) return true;
        }
        return false;
    };

    if (!is_name_taken(base_name)) return base_name;

    for (int suffix = 1;; ++suffix) {
        const std::string candidate = base_name + " (" + std::to_string(suffix) + ")";
        if (!is_name_taken(candidate)) return candidate;
    }
}

std::string Scene::make_default_name_for(const Entity& entity) const {
    const LightComponent* light = entity.get_light_component();
    const MeshComponent* mesh = entity.get_mesh_component();
    const std::string base_name = (light && light->enabled)
        ? "Light"
        : object_type_name(mesh ? mesh->type : CUBE);
    return make_unique_name(base_name);
}

void Scene::release_resources() {
    std::unordered_set<void*> released_meshes;

    for (auto& entity : entities) {
        MeshComponent* mesh = entity.get_mesh_component();
        MaterialComponent* mat = entity.get_material_component();
        if (!mesh) continue;

        const bool owns_model = entity_owns_model(entity);
        if (mesh->owns_materials) {
            if (mesh->model.materials) {
                free(mesh->model.materials);
                mesh->model.materials = nullptr;
            }

            if (mesh->model.meshMaterial) {
                free(mesh->model.meshMaterial);
                mesh->model.meshMaterial = nullptr;
            }

            mesh->model.materialCount = 0;
            mesh->owns_materials = false;
        }

        if (owns_model && mesh->model.meshCount > 0 && mesh->model.meshes) {
            void* mesh_ptr = static_cast<void*>(mesh->model.meshes);
            if (released_meshes.insert(mesh_ptr).second) {
                UnloadModel(mesh->model);
            }
        }
        mesh->model = {};
        mesh->owns_model_instance = false;
        if (mat) mat->texture = {0};
    }

    entities.clear();
    selected = -1;
    selected_entities.clear();
}
