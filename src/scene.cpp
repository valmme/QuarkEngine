#include "headers/scene.h"
#include <unordered_set>

void Scene::add_object(Model model, ObjectType type) {
    Entity e(static_cast<int>(entities.size()));
    e.model = model;
    e.type = type;
    entities.push_back(e);
    selected = static_cast<int>(entities.size()) - 1;
}

Entity* Scene::get_selected() {
    if (selected < 0 || selected >= static_cast<int>(entities.size())) return nullptr;
    return &entities[selected];
}

void Scene::release_resources() {
    std::unordered_set<void*> released_meshes;

    for (auto& entity : entities) {
        const bool owns_model = (!entity.asset || entity.asset->isProcedural);
        if (owns_model && entity.model.meshCount > 0 && entity.model.meshes) {
            void* mesh_ptr = static_cast<void*>(entity.model.meshes);
            if (released_meshes.insert(mesh_ptr).second) {
                UnloadModel(entity.model);
            }
        }

        entity.model = {0};
        entity.texture = {0};
    }

    entities.clear();
    selected = -1;
}
