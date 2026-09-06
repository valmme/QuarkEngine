#include "editor/editor.h"

#include "editor/editor_assets.h"
#include "editor/editor_ui.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "editor/editor_ui.h"
#include "project.h"
#include "tex.h"
#include "imgui.h"
#include "editor/editor_preferences.h"

namespace fs = std::filesystem;

namespace editor_internal {

ImGuizmo::OPERATION gizmo_mode = ImGuizmo::TRANSLATE;
int renaming_index = -1;
char rename_buf[128] = "";
bool scene_asset_dragging = false;

std::string dragged_scene_asset_name;
bool file_dragging = false;
int dragged_file_index = -1;
int dragged_target_folder_index = -1;

std::unordered_map<std::string, Texture> tex_cache;
bool show_about_window = false;
bool has_clipboard = false;
Entity clipboard_data;

void restore_entity_material(Entity& entity) {
    MaterialComponent* material = entity.get_material_component();
    if (!material) return;

    auto apply_texture = [&]() {
        MeshComponent* mesh = entity.get_mesh_component();
        if (!mesh || !material->texture.id) return;
        for (int index = 0; index < mesh->model.materialCount; ++index) {
            if (mesh->model.materials[index].maps) {
                mesh->model.materials[index].maps[MATERIAL_MAP_ALBEDO].texture = material->texture;
            }
        }
    };

    if (!material->albedo_texture_name.empty()) {
        for (const auto& option : texture_options) {
            if (option.name == material->albedo_texture_name) {
                material->texture = option.texture;
                material->texture_source = TEXTURE_EXTERNAL;
                mark_entity_uv_dirty(&entity);
                apply_texture();
                return;
            }
        }
    }

    if (!material->texture_name.empty()) {
        load_material_to_entity(&entity, material->texture_name, -1);
        material->texture_source = TEXTURE_EXTERNAL;
        mark_entity_uv_dirty(&entity);
        apply_texture();
        return;
    }

    if (material->texture_source == TEXTURE_MODEL) {
        restore_model_textures(&entity);
    } else if (material->texture_source == TEXTURE_NONE) {
        material->texture = {0};
        clear_material_textures(&entity);
    }
    mark_entity_uv_dirty(&entity);
    apply_texture();
}

void restore_scene_entity_models(Scene& scene) {
    for (auto& entity : scene.entities) {
        MeshComponent* mesh = entity.get_mesh_component();
        if (!mesh) continue;

        mesh->asset = mesh->asset_name.empty() ? nullptr : find_asset_by_name(mesh->asset_name);
        if (!mesh->asset) {
            mesh->model = {};
            mesh->owns_model_instance = false;
            continue;
        }

        if ((mesh->is_editable_mesh || mesh->vertex_gizmo) && !mesh->editable_mesh.vertices.empty()) {
            mesh->model = {};
            rebuild_mesh_from_editable(mesh->model, mesh->editable_mesh);
            mesh->owns_model_instance = true;
            store_uv(&entity);
            store_material_textures(&entity);
            apply_mesh_overrides(entity);
            restore_entity_material(entity);
        } else if (mesh->asset->is_procedural) {
            mesh->model = mesh->asset->generator(mesh->segments);
            mesh->owns_model_instance = true;
            store_uv(&entity);
            store_material_textures(&entity);
            apply_mesh_overrides(entity);
            restore_entity_material(entity);
        } else {
            if (!load_model_instance(*mesh->asset, mesh->model)) {
                mesh->asset = nullptr;
                mesh->asset_name.clear();
                mesh->model = {};
                mesh->owns_model_instance = false;
                continue;
            }

            mesh->owns_model_instance = true;
            store_uv(&entity);
            store_material_textures(&entity);
            apply_mesh_overrides(entity);
            restore_entity_material(entity);
        }

        mesh->shader_assigned = false;
    }
}

void reset_scene_light_runtime(Scene& scene) {
    for (auto& entity : scene.entities) {
        LightComponent* light = entity.get_light_component();
        if (!light) continue;

        if (light->created && light->light.id >= 0) {
            free_light_id(light->light.id);
        }

        light->created = false;
        light->light.id = -1;
        light->light.light.enabledLoc = -1;
        light->light.light.typeLoc = -1;
        light->light.light.positionLoc = -1;
        light->light.light.targetLoc = -1;
        light->light.light.colorLoc = -1;
        light->light.spot_angle_loc = -1;
        light->light.intensity_loc = -1;
        light->light.range_loc = -1;
    }
    reset_light_registry();
}

}

static SceneState capture_scene_state(const Scene& scene) {
    SceneState state;
    state.selected = scene.selected;
    state.selected_entities = scene.selected_entities;
    state.materials.reserve(scene.entities.size());

    for (auto entity : scene.entities) {
        MeshComponent* mesh = entity.get_mesh_component();
        if (mesh) {
            mesh->model = {};
            mesh->owns_model_instance = false;
            mesh->owns_materials = false;
        }

        MaterialComponent* material = entity.get_material_component();
        if (material) {
            material->original_material_textures.clear();
        }

        SceneState::MaterialSnapshot material_snapshot;
        if (const MaterialComponent* source_material = entity.get_material_component()) {
            material_snapshot.texture_source = source_material->texture_source;
            material_snapshot.texture = source_material->texture;
            material_snapshot.albedo_texture_name = source_material->albedo_texture_name;
            material_snapshot.texture_name = source_material->texture_name;
            material_snapshot.normal_texture_name = source_material->normal_texture_name;
            material_snapshot.material_slot_sources = source_material->material_slot_sources;
            material_snapshot.color = source_material->color;
            material_snapshot.outline_color = source_material->outline_color;
            material_snapshot.auto_uv = source_material->auto_uv;
            material_snapshot.texture_stretch = source_material->texture_stretch;
            material_snapshot.texture_repeat_u = source_material->texture_repeat_u;
            material_snapshot.texture_repeat_v = source_material->texture_repeat_v;
            material_snapshot.uv_scale = source_material->uv_scale;
        }
        state.materials.push_back(std::move(material_snapshot));

        LightComponent* light = entity.get_light_component();
        if (light) {
            light->created = false;
            light->light.id = -1;
            light->light.light.enabledLoc = -1;
            light->light.light.typeLoc = -1;
            light->light.light.positionLoc = -1;
            light->light.light.targetLoc = -1;
            light->light.light.colorLoc = -1;
            light->light.light.attenuationLoc = -1;
            light->light.spot_angle_loc = -1;
            light->light.intensity_loc = -1;
            light->light.range_loc = -1;
        }
        state.entities.push_back(entity);
    }

    return state;
}

static SceneState capture_light_state(const Scene& scene) {
    SceneState state;
    state.light_only = true;
    state.selected = scene.selected;
    state.selected_entities = scene.selected_entities;
    state.lights.reserve(scene.entities.size());
    for (const auto& entity : scene.entities) {
        const LightComponent* light = entity.get_light_component();
        state.lights.push_back(light ? light->light : Lighting{});
    }
    return state;
}

static void restore_light_state(Scene& scene, const SceneState& state) {
    const size_t count = std::min(scene.entities.size(), state.lights.size());
    for (size_t index = 0; index < count; ++index) {
        LightComponent* light = scene.entities[index].get_light_component();
        if (light) light->light = state.lights[index];
    }
    editor_internal::reset_scene_light_runtime(scene);
}

static SceneState capture_hierarchy_state(const Scene& scene) {
    SceneState state;
    state.hierarchy_only = true;
    state.selected = scene.selected;
    state.selected_entities = scene.selected_entities;
    state.parent_ids.reserve(scene.entities.size());
    state.positions.reserve(scene.entities.size());
    state.rotations.reserve(scene.entities.size());
    state.scales.reserve(scene.entities.size());
    for (const auto& entity : scene.entities) {
        state.parent_ids.push_back(entity.parent_id);
        const TransformComponent* transform = entity.get_transform_component();
        state.positions.push_back(transform ? transform->position : Vec3{});
        state.rotations.push_back(transform ? transform->rotation : Vec3{});
        state.scales.push_back(transform ? transform->scale : Vec3{1, 1, 1});
    }
    return state;
}

static SceneState capture_duplicate_state(const Scene& scene, int source_index) {
    SceneState state;
    state.duplicate_only = true;
    state.duplicate_index = static_cast<int>(scene.entities.size());
    state.duplicate_source_index = source_index;
    state.selected = scene.selected;
    state.selected_entities = scene.selected_entities;
    return state;
}

static SceneState capture_tags_state(const Scene& scene) {
    SceneState state;
    state.tags_only = true;
    state.selected = scene.selected;
    state.selected_entities = scene.selected_entities;
    state.tags.reserve(scene.entities.size());
    for (const auto& entity : scene.entities) {
        state.tags.push_back(entity.tags);
    }
    return state;
}

static void restore_tags_state(Scene& scene, const SceneState& state) {
    const size_t count = std::min(scene.entities.size(), state.tags.size());
    for (size_t index = 0; index < count; ++index) {
        scene.entities[index].tags = state.tags[index];
    }
    scene.selected = state.selected;
    scene.selected_entities = state.selected_entities;
}

static SceneState capture_material_state(const Scene& scene) {
    SceneState state;
    state.material_only = true;
    state.selected = scene.selected;
    state.selected_entities = scene.selected_entities;
    state.materials.reserve(scene.entities.size());
    for (const auto& entity : scene.entities) {
        SceneState::MaterialSnapshot snapshot;
        if (const MaterialComponent* material = entity.get_material_component()) {
            snapshot.texture_source = material->texture_source;
            snapshot.texture = material->texture;
            snapshot.albedo_texture_name = material->albedo_texture_name;
            snapshot.texture_name = material->texture_name;
            snapshot.normal_texture_name = material->normal_texture_name;
            snapshot.material_slot_sources = material->material_slot_sources;
            snapshot.color = material->color;
            snapshot.outline_color = material->outline_color;
            snapshot.auto_uv = material->auto_uv;
            snapshot.texture_stretch = material->texture_stretch;
            snapshot.texture_repeat_u = material->texture_repeat_u;
            snapshot.texture_repeat_v = material->texture_repeat_v;
            snapshot.uv_scale = material->uv_scale;
        }
        state.materials.push_back(std::move(snapshot));
    }
    return state;
}

static bool is_removable_component_type(const std::string& type_name) {
    return type_name == "Material" || type_name == "Light" ||
           type_name == "Collision" || type_name == "3D Text";
}

static nlohmann::json capture_component_list(const Entity& entity) {
    nlohmann::json result = nlohmann::json::array();
    const ComponentManager* cm = entity.get_components();
    if (!cm) return result;

    for (const auto& comp : cm->get_all_components()) {
        if (!comp || !is_removable_component_type(comp->get_type_name())) continue;

        nlohmann::json entry;
        entry["type"] = comp->get_type_name();
        entry["enabled"] = comp->enabled;
        nlohmann::json data;
        comp->serialize(data);
        entry["data"] = data;
        result.push_back(std::move(entry));
    }
    return result;
}

static SceneState capture_component_state(const Scene& scene, int entity_index) {
    SceneState state;
    state.component_only = true;
    state.component_entity_index = entity_index;
    state.selected = scene.selected;
    state.selected_entities = scene.selected_entities;
    if (entity_index >= 0 && entity_index < static_cast<int>(scene.entities.size())) {
        state.component_data = capture_component_list(scene.entities[entity_index]).dump();
    }
    return state;
}

static void restore_component_state(Scene& scene, const SceneState& state) {
    const int index = state.component_entity_index;
    if (index < 0 || index >= static_cast<int>(scene.entities.size())) return;

    Entity& entity = scene.entities[index];
    ComponentManager* cm = entity.get_components();
    if (!cm) return;

    nlohmann::json desired = nlohmann::json::parse(state.component_data, nullptr, false);
    if (!desired.is_array()) return;

    std::vector<std::string> desired_types;
    for (const auto& entry : desired) {
        if (entry.contains("type")) desired_types.push_back(entry["type"].get<std::string>());
    }

    bool restore_needs_material = false;
    bool entity_has_light = false;
    {
        auto& comps = cm->get_all_components();

        for (size_t i = 0; i < comps.size();) {
            auto comp = comps[i];
            if (!comp || !is_removable_component_type(comp->get_type_name())) { ++i; continue; }

            const std::string type_name = comp->get_type_name();
            const bool keep = std::find(desired_types.begin(), desired_types.end(), type_name) != desired_types.end();
            if (keep) {
                if (type_name == "Light") entity_has_light = true;
                ++i;
                continue;
            }

            if (auto light = std::dynamic_pointer_cast<LightComponent>(comp)) {
                if (light->created && light->light.id >= 0) free_light_id(light->light.id);
                light->created = false;
                light->light.id = -1;
            }
            comps.erase(comps.begin() + i);
        }

        for (const auto& entry : desired) {
            if (!entry.contains("type")) continue;
            const std::string type_name = entry["type"].get<std::string>();
            const bool present = std::any_of(comps.begin(), comps.end(),
                [&](const std::shared_ptr<Component>& existing) {
                    return existing && existing->get_type_name() == type_name;
                });
            if (present) {
                if (type_name == "Light") entity_has_light = true;
                continue;
            }

            std::shared_ptr<Component> comp;
            if (type_name == "Material") {
                comp = std::make_shared<MaterialComponent>();
                restore_needs_material = true;
            }
            else if (type_name == "Light") {
                comp = std::make_shared<LightComponent>();
                entity_has_light = true;
            }
            else if (type_name == "Collision") {
                comp = std::make_shared<CollisionComponent>();
            }
            else if (type_name == "3D Text") {
                comp = std::make_shared<Text3DComponent>();
            }
            if (!comp) continue;

            comp->enabled = entry.value("enabled", true);
            if (entry.contains("data")) comp->deserialize(entry["data"]);
            comps.push_back(comp);
        }
    }

    if (restore_needs_material) {
        editor_internal::restore_entity_material(entity);
    }

    if (entity_has_light) {
        editor_internal::reset_scene_light_runtime(scene);
    }

    scene.selected = state.selected;
    scene.selected_entities = state.selected_entities;
}

static void restore_material_state(Scene& scene, const SceneState& state) {
    const size_t count = std::min(scene.entities.size(), state.materials.size());
    for (size_t index = 0; index < count; ++index) {
        MaterialComponent* material = scene.entities[index].get_material_component();
        MeshComponent* mesh = scene.entities[index].get_mesh_component();
        if (!material || !mesh) continue;

        const auto& snapshot = state.materials[index];
        material->texture_source = snapshot.texture_source;
        material->albedo_texture_name = snapshot.albedo_texture_name;
        material->texture_name = snapshot.texture_name;
        material->material_slot_sources = snapshot.material_slot_sources;
        material->color = snapshot.color;
        material->outline_color = snapshot.outline_color;
        material->auto_uv = snapshot.auto_uv;
        material->texture_stretch = snapshot.texture_stretch;
        material->texture_repeat_u = snapshot.texture_repeat_u;
        material->texture_repeat_v = snapshot.texture_repeat_v;
        material->uv_scale = snapshot.uv_scale;
        material->texture = snapshot.texture;
        material->normal_texture_name = snapshot.normal_texture_name;

        clear_material_textures(&scene.entities[index]);
        for (int slot = 0; slot < mesh->model.materialCount; ++slot) {
            if (slot >= static_cast<int>(material->material_slot_sources.size()) ||
                material->material_slot_sources[slot].empty()) continue;
            load_material_to_entity(&scene.entities[index], material->material_slot_sources[slot], slot);
        }

        if (!material->albedo_texture_name.empty()) {
            for (const auto& option : texture_options) {
                if (option.name == material->albedo_texture_name) {
                    material->texture = option.texture;
                    material->texture_source = TEXTURE_EXTERNAL;
                    for (int slot = 0; slot < mesh->model.materialCount; ++slot) {
                        if (mesh->model.materials[slot].maps) {
                            if (slot >= static_cast<int>(material->material_slot_sources.size()) ||
                                material->material_slot_sources[slot].empty()) {
                                mesh->model.materials[slot].maps[MATERIAL_MAP_ALBEDO].texture = material->texture;
                            }
                        }
                    }
                    break;
                }
            }
        } else if (material->texture.id != 0) {
            material->texture_source = TEXTURE_EXTERNAL;
            for (int slot = 0; slot < mesh->model.materialCount; ++slot) {
                if (mesh->model.materials[slot].maps) {
                    if (slot >= static_cast<int>(material->material_slot_sources.size()) ||
                        material->material_slot_sources[slot].empty()) {
                        mesh->model.materials[slot].maps[MATERIAL_MAP_ALBEDO].texture = material->texture;
                    }
                }
            }
        } else if (!material->texture_name.empty()) {
            load_material_to_entity(&scene.entities[index], material->texture_name, -1);
            material->texture_source = TEXTURE_EXTERNAL;
        } else if (material->texture_source == TEXTURE_MODEL) {
            restore_model_textures(&scene.entities[index]);
        } else if (material->texture_source == TEXTURE_NONE) {
            material->texture = {0};
            clear_material_textures(&scene.entities[index]);
        }

        if (!material->normal_texture_name.empty()) {
            for (const auto& option : texture_options) {
                if (option.name != material->normal_texture_name) continue;
                for (int slot = 0; slot < mesh->model.materialCount; ++slot) {
                    if (mesh->model.materials[slot].maps) {
                        mesh->model.materials[slot].maps[MATERIAL_MAP_NORMAL].texture = option.texture;
                    }
                }
                break;
            }
        }
        mark_entity_uv_dirty(&scene.entities[index]);
        refresh_entity_render_state(scene.entities[index]);
    }
    scene.selected = state.selected;
    scene.selected_entities = state.selected_entities;
}

static void restore_hierarchy_state(Scene& scene, const SceneState& state) {
    if (state.parent_ids.size() != scene.entities.size() ||
        state.positions.size() != scene.entities.size() ||
        state.rotations.size() != scene.entities.size() ||
        state.scales.size() != scene.entities.size()) return;

    std::vector<int> parent_ids = state.parent_ids;
    for (size_t index = 0; index < parent_ids.size(); ++index) {
        std::vector<bool> visited(parent_ids.size(), false);
        int current = static_cast<int>(index);
        while (current >= 0 && current < static_cast<int>(parent_ids.size())) {
            if (visited[current]) {
                parent_ids[index] = -1;
                break;
            }
            visited[current] = true;
            current = parent_ids[current];
        }
        if (parent_ids[index] < 0 || parent_ids[index] >= static_cast<int>(parent_ids.size()) ||
            parent_ids[index] == static_cast<int>(index)) {
            parent_ids[index] = -1;
        }
    }

    const size_t count = scene.entities.size();
    for (size_t index = 0; index < count; ++index) {
        Entity& entity = scene.entities[index];
        entity.parent_id = parent_ids[index];
        if (TransformComponent* transform = entity.get_transform_component()) {
            transform->position = state.positions[index];
            transform->rotation = state.rotations[index];
            transform->scale = state.scales[index];
        }
    }
    scene.selected = state.selected;
    scene.selected_entities = state.selected_entities;
}

void Editor::save_state() {
    scene_dirty = true;
    undo_stack.push(capture_scene_state(scene));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::save_light_state() {
    scene_dirty = true;
    undo_stack.push(capture_light_state(scene));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::save_hierarchy_state() {
    scene_dirty = true;
    undo_stack.push(capture_hierarchy_state(scene));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::save_component_state(int entity_index) {
    scene_dirty = true;
    undo_stack.push(capture_component_state(scene, entity_index));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::save_transform_state(Entity* entity, const Vec3& position,
    const Vec3& rotation, const Vec3& scale) {
    if (!entity) return;

    const int entity_index = static_cast<int>(entity - scene.entities.data());
    if (entity_index < 0 || entity_index >= static_cast<int>(scene.entities.size())) return;

    SceneState state = capture_hierarchy_state(scene);
    state.positions[entity_index] = position;
    state.rotations[entity_index] = rotation;
    state.scales[entity_index] = scale;
    undo_stack.push(std::move(state));
    scene_dirty = true;
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::save_duplicate_state(int source_index) {
    scene_dirty = true;
    undo_stack.push(capture_duplicate_state(scene, source_index));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::save_tags_state() {
    scene_dirty = true;
    undo_stack.push(capture_tags_state(scene));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::save_material_state() {
    scene_dirty = true;
    undo_stack.push(capture_material_state(scene));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::save_material_state_before(Entity* entity, const MaterialComponent& material) {
    if (!entity) return;
    const int entity_index = static_cast<int>(entity - scene.entities.data());
    if (entity_index < 0 || entity_index >= static_cast<int>(scene.entities.size())) return;

    SceneState state = capture_material_state(scene);
    auto& snapshot = state.materials[entity_index];
    snapshot.texture_source = material.texture_source;
    snapshot.texture = material.texture;
    snapshot.albedo_texture_name = material.albedo_texture_name;
    snapshot.texture_name = material.texture_name;
    snapshot.normal_texture_name = material.normal_texture_name;
    snapshot.material_slot_sources = material.material_slot_sources;
    snapshot.color = material.color;
    snapshot.outline_color = material.outline_color;
    snapshot.auto_uv = material.auto_uv;
    snapshot.texture_stretch = material.texture_stretch;
    snapshot.texture_repeat_u = material.texture_repeat_u;
    snapshot.texture_repeat_v = material.texture_repeat_v;
    snapshot.uv_scale = material.uv_scale;

    scene_dirty = true;
    undo_stack.push(std::move(state));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::undo() {
    if (undo_stack.empty()) return;

    SceneState previous = undo_stack.top();
    undo_stack.pop();

    if (previous.light_only) {
        redo_stack.push(capture_light_state(scene));
        restore_light_state(scene, previous);
        return;
    }

    if (previous.hierarchy_only) {
        redo_stack.push(capture_hierarchy_state(scene));
        restore_hierarchy_state(scene, previous);
        return;
    }

    if (previous.duplicate_only) {
        redo_stack.push(capture_duplicate_state(scene, previous.duplicate_source_index));
        erase_entity_after_hierarchy(*this, previous.duplicate_index);
        return;
    }

    if (previous.tags_only) {
        redo_stack.push(capture_tags_state(scene));
        restore_tags_state(scene, previous);
        return;
    }

    if (previous.material_only) {
        redo_stack.push(capture_material_state(scene));
        restore_material_state(scene, previous);
        return;
    }

    if (previous.component_only) {
        redo_stack.push(capture_component_state(scene, previous.component_entity_index));
        restore_component_state(scene, previous);
        return;
    }

    redo_stack.push(capture_scene_state(scene));

    scene.release_resources();
    scene.entities = std::move(previous.entities);
    scene.selected = previous.selected;
    scene.selected_entities = previous.selected_entities;
    editor_internal::reset_scene_light_runtime(scene);
    editor_internal::restore_scene_entity_models(scene);
    restore_material_state(scene, previous);
}

void Editor::redo() {
    if (redo_stack.empty()) return;

    SceneState next = redo_stack.top();
    redo_stack.pop();

    if (next.light_only) {
        undo_stack.push(capture_light_state(scene));
        restore_light_state(scene, next);
        return;
    }

    if (next.hierarchy_only) {
        undo_stack.push(capture_hierarchy_state(scene));
        restore_hierarchy_state(scene, next);
        return;
    }

    if (next.duplicate_only) {
        undo_stack.push(capture_duplicate_state(scene, next.duplicate_source_index));
        if (next.duplicate_source_index >= 0 &&
            next.duplicate_source_index < static_cast<int>(scene.entities.size())) {
            Entity copy = clone_entity_instance(scene.entities[next.duplicate_source_index], scene);
            scene.entities.push_back(copy);
            scene.selected = static_cast<int>(scene.entities.size()) - 1;
        }
        return;
    }

    if (next.tags_only) {
        undo_stack.push(capture_tags_state(scene));
        restore_tags_state(scene, next);
        return;
    }

    if (next.material_only) {
        undo_stack.push(capture_material_state(scene));
        restore_material_state(scene, next);
        return;
    }

    if (next.component_only) {
        undo_stack.push(capture_component_state(scene, next.component_entity_index));
        restore_component_state(scene, next);
        return;
    }

    undo_stack.push(capture_scene_state(scene));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();

    scene.release_resources();
    scene.entities = std::move(next.entities);
    scene.selected = next.selected;
    scene.selected_entities = next.selected_entities;
    editor_internal::reset_scene_light_runtime(scene);
    editor_internal::restore_scene_entity_models(scene);
    restore_material_state(scene, next);
}

void Editor::handle_input() {
    using namespace editor_internal;

    ImGuiIO& io = ImGui::GetIO();
    const bool keyboard_available = !io.WantTextInput;

    if (keyboard_available && IsKeyPressed(KeyboardKey::P)) gizmo_mode = ImGuizmo::TRANSLATE;
    if (keyboard_available && IsKeyPressed(KeyboardKey::R)) gizmo_mode = ImGuizmo::ROTATE;
    if (keyboard_available && IsKeyPressed(KeyboardKey::S)) gizmo_mode = ImGuizmo::SCALE;

    if (IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();
        bool imported_any = false;

        std::error_code ec;

        if (current_asset_path.empty()) {
            if (!project_path.empty()) {
                current_asset_path = fs::path(project_path) / "resources";
            } else {
                current_asset_path = fs::current_path() / "projects" / "default" / "resources";
            }
            fs::create_directories(current_asset_path, ec);
        }

        for (unsigned int i = 0; i < dropped.count; i++) {
            imported_any = import_path_to_resources(fs::path(dropped.paths[i]), current_asset_path) || imported_any;
        }

        UnloadDroppedFiles(dropped);

        if (imported_any) {
            save_state();
            refresh_textures(&scene, project_path);
            refresh_assets(project_path);
            refresh_models(project_path, scene);
        }
    }

    const bool ctrl = (IsKeyDown(KeyboardKey::LeftControl) || IsKeyDown(KeyboardKey::RightControl)) && keyboard_available;
    const bool shift_down = IsKeyDown(KeyboardKey::LeftShift) || IsKeyDown(KeyboardKey::RightShift);

    if (ctrl && shift_down && IsKeyPressed(KeyboardKey::S)) {
        editor_save_as(*this);
    } else if (ctrl && IsKeyPressed(KeyboardKey::S)) {
        project_save(project_path, scene);
        scene_dirty = false;
    }

    static float last_undo_time = 0.0f;
    static float last_redo_time = 0.0f;
    static float last_copy_time = 0.0f;
    static float last_paste_time = 0.0f;
    static float last_dublicate_time = 0.0f;

    static bool undo_key_was_pressed = false;
    static bool redo_key_was_pressed = false;
    static bool copy_key_was_pressed = false;
    static bool paste_key_was_pressed = false;
    static bool dubl_key_was_pressed = false;

    static float undo_hold_start = 0.0f;
    static float redo_hold_start = 0.0f;
    static float copy_hold_start = 0.0f;
    static float paste_hold_start = 0.0f;
    static float dubl_hold_start = 0.0f;

    const float now = static_cast<float>(GetTime());

    if (ctrl && IsKeyDown(KeyboardKey::Z)) {
        if (!undo_key_was_pressed) {
            undo();
            undo_key_was_pressed = true;
            undo_hold_start = now;
            last_undo_time = now;
        } else if (now - undo_hold_start > 0.5f && now - last_undo_time > 0.15f) {
            undo();
            last_undo_time = now;
        }
    } else {
        undo_key_was_pressed = false;
        undo_hold_start = 0.0f;
    }

    if (ctrl && IsKeyDown(KeyboardKey::Y)) {
        if (!redo_key_was_pressed) {
            redo();
            redo_key_was_pressed = true;
            redo_hold_start = now;
            last_redo_time = now;
        } else if (now - redo_hold_start > 0.5f && now - last_redo_time > 0.15f) {
            redo();
            last_redo_time = now;
        }
    } else {
        redo_key_was_pressed = false;
        redo_hold_start = 0.0f;
    }

    Entity* entity = scene.get_selected();

    if (ctrl && IsKeyDown(KeyboardKey::C)) {
        if (!copy_key_was_pressed) {
            copy_entity(entity);

            copy_key_was_pressed = true;
            copy_hold_start = now;
            last_copy_time = now;
        } else if (now - copy_hold_start > 0.5f && now - last_copy_time > 0.15f) {
            copy_entity(entity);
            last_copy_time = now;
        }
    }

    if (ctrl && IsKeyDown(KeyboardKey::V)) {
        if (!paste_key_was_pressed){
            paste_entity(*this);

            paste_key_was_pressed = true;
            paste_hold_start = now;
            last_paste_time = now;
        } else if (now - paste_hold_start > 0.5f && now - last_paste_time > 0.15f) {
            paste_entity(*this);
            last_paste_time = now;
        }
    }

    if (ctrl && IsKeyDown(KeyboardKey::D)) {
        if (!dubl_key_was_pressed) {
            dublicate_entity(*this, entity);

            dubl_key_was_pressed = true;
            dubl_hold_start = now;
            last_dublicate_time = now;
        } else if (now - dubl_hold_start > 0.5f && now - last_dublicate_time > 0.15f) {
            dublicate_entity(*this, entity);
            last_dublicate_time = now;
        }
    }

    if (keyboard_available && IsKeyPressed(KeyboardKey::Delete)) {
        delete_entity(*this, entity);
    }

    static double last_asset_poll = 0.0;
    const double current_time = GetTime();
    if (current_time - last_asset_poll <= 2.0) return;

    last_asset_poll = current_time;

    const fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) return;

    static std::string last_resource_signature;
    const std::string current_signature = build_resource_signature(resource_dir);
    if (current_signature == last_resource_signature) return;

    last_resource_signature = current_signature;
    refresh_textures(&scene, project_path);
    refresh_assets(project_path);
    refresh_models(project_path, scene);
}
