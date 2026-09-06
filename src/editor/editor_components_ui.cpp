#include "editor/editor_components_ui.h"
#include "editor/editor_utils.h"
#include "editor/editor_ui.h"
#include "editor/editor_viewers.h"
#include "imgui.h"
#include "text_mesh.h"
#include "models.h"
#include "entity.h"
#include "editor/editor_ui.h"
#include "editor/editor.h"
#include "language_manager.h"
#include <filesystem>
#include <cstring>

#define lang LanguageManager::get()

bool ComponentUIHelper::should_show_component_menu = false;
int ComponentUIHelper::component_to_remove = -1;

void ComponentUIHelper::draw_entity_inspector(Editor& editor, Entity& entity, Shader shader) {
    ImGui::Spacing();
    
    if (entity.get_components()) {
        auto components_manager = entity.get_components();
        size_t component_count = components_manager->get_component_count();
        component_to_remove = -1;

        for (size_t i = 0; i < component_count; ++i) {
            auto comp = components_manager->get_component(i);
            if (!comp) continue;

            std::string tab_label = comp->get_type_name();
            ImGui::PushID(static_cast<int>(i));
            bool open = ImGui::CollapsingHeader(tab_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            
            if (open) {
                ImGui::Spacing();

                bool is_enabled = comp->enabled;
                if (ImGui::Checkbox(lang.word("enabled"), &is_enabled)) {
                    editor.save_state();
                    comp->enabled = is_enabled;
                }

                ImGui::Spacing();

                if (auto transform = std::dynamic_pointer_cast<TransformComponent>(comp)) draw_transform_component(editor, entity, transform.get());
                else if (auto mesh = std::dynamic_pointer_cast<MeshComponent>(comp)) draw_mesh_component(editor, entity, mesh.get());
                else if (auto light = std::dynamic_pointer_cast<LightComponent>(comp)) draw_light_component(editor, entity, light.get(), shader);
                else if (auto material = std::dynamic_pointer_cast<MaterialComponent>(comp)) draw_material_component(editor, entity, material.get());
                else if (auto collision = std::dynamic_pointer_cast<CollisionComponent>(comp)) draw_collision_component(editor, entity, collision.get());
                else if (auto text = std::dynamic_pointer_cast<Text3DComponent>(comp)) draw_3d_text_component(editor, entity, text.get());

                ImGui::Spacing();

                bool can_remove = (comp->get_type() != COMPONENT_TRANSFORM && comp->get_type() != COMPONENT_MESH);
                if (!can_remove) {
                    ImGui::BeginDisabled();
                }
                
                if (can_remove) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::Button(lang.word("remove_component"), ImVec2(-1, 0))) {
                        component_to_remove = static_cast<int>(i);
                    }
                    ImGui::PopStyleColor(2);
                }
                
                if (!can_remove) {
                    ImGui::EndDisabled();
                }
            }
            ImGui::PopID();
            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::Spacing();
        if (ImGui::Button(lang.word("add_component"), ImVec2(-1, 0))) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            auto components_manager = entity.get_components();

            const bool has_material = [&]() {
                for (size_t j = 0; j < components_manager->get_component_count(); ++j) {
                    auto existing = components_manager->get_component(j);
                    if (existing && existing->get_type() == COMPONENT_MATERIAL) return true;
                }
                return false;
            }();
            if (ImGui::MenuItem(lang.word("material"))) {
                if (!has_material) {
                    editor.save_component_state(static_cast<int>(&entity - editor.scene.entities.data()));
                    components_manager->add_component(std::make_shared<MaterialComponent>());
                }
            }

            const bool has_collision = [&]() {
                for (size_t j = 0; j < components_manager->get_component_count(); ++j) {
                    auto existing = components_manager->get_component(j);
                    if (existing && existing->get_type() == COMPONENT_COLLISION) return true;
                }
                return false;
            }();
            if (ImGui::MenuItem(lang.word("collision"))) {
                if (!has_collision) {
                    editor.save_component_state(static_cast<int>(&entity - editor.scene.entities.data()));
                    components_manager->add_component(std::make_shared<CollisionComponent>());
                }
            }

            const bool has_light = [&]() {
                for (size_t j = 0; j < components_manager->get_component_count(); ++j) {
                    auto existing = components_manager->get_component(j);
                    if (existing && existing->get_type() == COMPONENT_LIGHT) return true;
                }
                return false;
            }();
            if (ImGui::MenuItem(lang.word("light"))) {
                if (!has_light) {
                    editor.save_component_state(static_cast<int>(&entity - editor.scene.entities.data()));
                    auto light = std::make_shared<LightComponent>();
                    if (auto transform = entity.get_transform_component()) {
                        light->light.position = transform->position;
                    }
                    components_manager->add_component(light);
                }
            }

            const bool has_text = [&]() {
                for (size_t j = 0; j < components_manager->get_component_count(); ++j) {
                    auto existing = components_manager->get_component(j);
                    if (existing && existing->get_type() == COMPONENT_CUSTOM) return true;
                }
                return false;
            }();
            if (ImGui::MenuItem(lang.word("text"))) {
                if (!has_text) {
                    editor.save_component_state(static_cast<int>(&entity - editor.scene.entities.data()));
                    components_manager->add_component(std::make_shared<Text3DComponent>());
                }
            }
            ImGui::EndPopup();
        }

        if (component_to_remove != -1) {
            const int entity_index = static_cast<int>(&entity - editor.scene.entities.data());
            editor.save_component_state(entity_index);
            std::shared_ptr<Component> comp = components_manager->get_component(component_to_remove);

            if (std::shared_ptr<LightComponent> light = std::dynamic_pointer_cast<LightComponent>(comp)) {
                if (light->created) {
                    light->light.enabled = false;
                    
                    if (light->light.id != -1) {
                        update_lighting(shader, light->light);
                        free_light_id(light->light.id);
                    }
                }
            }

            components_manager->remove_component(component_to_remove);
            component_to_remove = -1;
        }
    }
}

void ComponentUIHelper::draw_transform_component(Editor& editor, Entity& entity, TransformComponent* transform) {
    if (!transform) return;

    static bool transform_edit_pending = false;
    static Vec3 pending_position = {};
    static Vec3 pending_rotation = {};
    static Vec3 pending_scale = {1, 1, 1};

    const auto begin_transform_edit = [&](const Vec3& before_position,
        const Vec3& before_rotation, const Vec3& before_scale) {
        if (ImGui::IsItemActivated()) {
            transform_edit_pending = true;
            pending_position = before_position;
            pending_rotation = before_rotation;
            pending_scale = before_scale;
        }
    };

    const auto finish_transform_edit = [&]() {
        if (ImGui::IsItemDeactivatedAfterEdit() && transform_edit_pending) {
            editor.save_transform_state(&entity, pending_position, pending_rotation, pending_scale);
            transform_edit_pending = false;
        }
    };

    float position[3] = {transform->position.x, transform->position.y, transform->position.z};
    float rotation[3] = {transform->rotation.x, transform->rotation.y, transform->rotation.z};
    float scale[3] = {transform->scale.x, transform->scale.y, transform->scale.z};

    const Vec3 before_position = transform->position;
    if (ImGui::DragFloat3(lang.word("position"), position, 0.1f)) {
        transform->position = {position[0], position[1], position[2]};
        mark_entity_bounds_dirty(&entity);
    }
    begin_transform_edit(before_position, transform->rotation, transform->scale);
    finish_transform_edit();

    const Vec3 before_rotation = transform->rotation;
    if (ImGui::DragFloat3(lang.word("rotation"), rotation, 1.0f)) {
        transform->rotation = {rotation[0], rotation[1], rotation[2]};
        mark_entity_bounds_dirty(&entity);
    }
    begin_transform_edit(transform->position, before_rotation, transform->scale);
    finish_transform_edit();

    const Vec3 before_scale = transform->scale;
    if (ImGui::DragFloat3(lang.word("scale"), scale, 0.1f)) {
        auto count_neg = [](float x, float y, float z) {
            return (x < 0.0f ? 1 : 0) + (y < 0.0f ? 1 : 0) + (z < 0.0f ? 1 : 0);
        };

        bool was_flipped = count_neg(transform->scale.x, transform->scale.y, transform->scale.z) % 2 != 0;
        bool will_flip = count_neg(scale[0], scale[1], scale[2]) % 2 != 0;

        transform->scale = {scale[0], scale[1], scale[2]};
        mark_entity_bounds_dirty(&entity);
        mark_entity_uv_dirty(&entity);

        if (was_flipped != will_flip) {
            update_model(&entity);
        }
    }
    begin_transform_edit(transform->position, transform->rotation, before_scale);
    finish_transform_edit();
}

void ComponentUIHelper::draw_mesh_component(Editor& editor, Entity& entity, MeshComponent* mesh) {
    if (!mesh) return;

    ImGui::Text(lang.word("mesh_config"));
    ImGui::Spacing();

    if (mesh->asset) {
        ImGui::Text("%s: %s", lang.word("asset"), mesh->asset->name.c_str());
    } else {
        ImGui::Text("%s: %s", lang.word("asset"), lang.word("none"));
    }

    const char* object_type_names[] = {
        lang.word("cube"),
        lang.word("sphere"),
        lang.word("cone"),
        lang.word("cylinder"),
        lang.word("hemisphere"),
        lang.word("torus")
    };

    int current_object_type_index = static_cast<int>(mesh->type);
    bool is_procedural_asset = mesh->asset && mesh->asset->is_procedural;

    if (!is_procedural_asset) {
        ImGui::Text(lang.word("mesh_type"), mesh->asset_name.c_str());
        ImGui::BeginDisabled();
    }

    if (ImGui::Combo(lang.word("mesh_type_combo"), &current_object_type_index, object_type_names, IM_ARRAYSIZE(object_type_names))) {
        if (current_object_type_index != static_cast<int>(mesh->type)) {
            editor.save_state();
            ObjectType new_type = static_cast<ObjectType>(current_object_type_index);

            ModelAsset* new_asset = nullptr;
            for (auto& asset_entry : assets) {
                if (asset_entry.is_procedural && asset_entry.type == new_type) {
                    new_asset = &asset_entry;
                    break;
                }
            }

            if (new_asset) {
                mesh->type = new_type;
                mesh->asset = new_asset;
                mesh->asset_name = new_asset->name;
                update_model(&entity);
                mark_entity_bounds_dirty(&entity);
                mark_entity_uv_dirty(&entity);
                store_uv(&entity);
                store_material_textures(&entity);
            }
        }
    }

    if (!is_procedural_asset) {
        ImGui::EndDisabled();
    }

    if (mesh->asset && mesh->asset->is_procedural) {
        if (ImGui::DragInt(lang.word("segments"), &mesh->segments, 1, 3, 100)) {
            editor.save_state();
            update_model(&entity);
            store_uv(&entity);
            store_material_textures(&entity);
            mark_entity_bounds_dirty(&entity);
            mark_entity_uv_dirty(&entity);
        }
    } else {
        ImGui::Text(lang.word("segments_loaded"), mesh->segments);
    }

    ImGui::Separator();

    if (ImGui::Checkbox(lang.word("vertex_gizmo"), &mesh->vertex_gizmo)) {
        if (mesh->vertex_gizmo && mesh->editable_mesh.vertices.empty() && has_valid_model_data(mesh->model)) {
            mesh->editable_mesh.vertices.clear();
            mesh->editable_mesh.triangles.clear();
            mesh->is_editable_mesh = true;

            const Mesh& m = mesh->model.meshes[0];

            std::vector<int> remap(m.vertexCount, -1);
            for (int i = 0; i < m.vertexCount; i++) {
                Vec3 pos = {
                    m.vertices[i*3+0],
                    m.vertices[i*3+1],
                    m.vertices[i*3+2]
                };

                remap[i] = (int)mesh->editable_mesh.vertices.size();
                EditableVertex ev;
                ev.position = pos;
                if (m.texcoords) {
                    ev.u = m.texcoords[i * 2 + 0];
                    ev.v = m.texcoords[i * 2 + 1];
                }
                mesh->editable_mesh.vertices.push_back(ev);
            }

            for (int t = 0; t < m.triangleCount; t++) {
                int ia, ib, ic;
                if (m.indices) {
                    ia = m.indices[t*3+0];
                    ib = m.indices[t*3+1];
                    ic = m.indices[t*3+2];
                }

                else {
                    ia = t*3+0;
                    ib = t*3+1;
                    ic = t*3+2;
                }

                if (ia >= m.vertexCount || ib >= m.vertexCount || ic >= m.vertexCount) continue;

                EditableTriangle tri;
                tri.a = remap[ia];
                tri.b = remap[ib];
                tri.c = remap[ic];

                if (tri.a == tri.b || tri.b == tri.c || tri.a == tri.c) {
                    continue;
                }

                mesh->editable_mesh.triangles.push_back(tri);
            }

            if (g_selected_vertices.empty() && !mesh->editable_mesh.vertices.empty())
                g_selected_vertices.push_back(0);
        }
    }

    if (mesh->vertex_gizmo) {
        ImGui::Text(lang.word("vertices"), (int)mesh->editable_mesh.vertices.size());
        ImGui::Text(lang.word("triangles"), (int)mesh->editable_mesh.triangles.size());

        if (!g_selected_vertices.empty()) {
            EditableVertex& selected_vertex = mesh->editable_mesh.vertices[g_selected_vertices[0]];
            float vertex_pos[3] = { selected_vertex.position.x, selected_vertex.position.y, selected_vertex.position.z };

            ImGui::Separator();

            if (ImGui::DragFloat3("##vertex_position", vertex_pos, 0.1f)) {
                editor.save_state();

                selected_vertex.position = {
                    vertex_pos[0],
                    vertex_pos[1],
                    vertex_pos[2]
                };

                rebuild_mesh_from_editable(mesh->model, mesh->editable_mesh);

                mark_entity_bounds_dirty(&entity);
                mark_entity_uv_dirty(&entity);
            }

            ImGui::Text(lang.word("vertex_position"));
        }
    }
}

void ComponentUIHelper::draw_material_component(Editor& editor, Entity& entity, MaterialComponent* material) {
    if (!material) return;

    ImGui::Text(lang.word("material_properties"));
    ImGui::Spacing();

    MaterialComponent* mat = entity.get_material_component();
    MeshComponent* mesh = entity.get_mesh_component();
    if (!mat || !mesh) return;

    ImGui::Text("Material slots: %d", mesh->model.materialCount);
    for (int slot = 0; slot < mesh->model.materialCount; ++slot) {
        const int material_index = slot;
        if (material_index < 0 || material_index >= mesh->model.materialCount || !mesh->model.materials) continue;

        const Material& material = mesh->model.materials[material_index];
        const bool has_albedo = material.maps && material.maps[MATERIAL_MAP_ALBEDO].texture.valid;
        const bool has_normal = material.maps && material.maps[MATERIAL_MAP_NORMAL].texture.valid;
        const bool has_roughness = material.maps && material.maps[MATERIAL_MAP_ROUGHNESS].texture.valid;
        const bool has_metallic = material.maps && material.maps[MATERIAL_MAP_METALNESS].texture.valid;
        ImGui::Text("Slot %d: Albedo %s | Normal %s | Roughness %s | Metallic %s",
            slot,
            has_albedo ? "yes" : "no",
            has_normal ? "yes" : "no",
            has_roughness ? "yes" : "no",
            has_metallic ? "yes" : "no");
    }

    static std::vector<std::string> available_materials;
    static std::vector<std::string> material_display_names;
    static std::vector<const char*> material_names_cstr;
    static int selected_material_index = -1;
    static bool materials_list_needs_update = true;

    if (materials_list_needs_update) {
        available_materials = get_all_materials_in_project();
        available_materials.insert(available_materials.begin(), "None");
        material_display_names.clear();
        material_names_cstr.clear();
        
        for (const auto& mat_path : available_materials) {
            material_display_names.push_back(
                mat_path == "None" ? "None" : std::filesystem::path(mat_path).filename().generic_string()
            );
        }
        for (const auto& display_name : material_display_names) {
            material_names_cstr.push_back(display_name.c_str());
        }
        
        materials_list_needs_update = false;
    }

    if (mesh->model.materialCount > 0) {
        mat->material_slot_sources.resize(mesh->model.materialCount);
        ImGui::Separator();
        ImGui::Text("Material assignment by slot");
        for (int slot = 0; slot < mesh->model.materialCount; ++slot) {
            int slot_material_index = 0;
            for (int i = 1; i < static_cast<int>(available_materials.size()); ++i) {
                if (available_materials[i] == mat->material_slot_sources[slot]) {
                    slot_material_index = i;
                    break;
                }
            }

            ImGui::PushID(slot);
            ImGui::Text("Slot %d", slot);
            ImGui::SameLine();
            if (ImGui::Combo("##slot_material", &slot_material_index, material_names_cstr.data(),
                             static_cast<int>(material_names_cstr.size()))) {
                editor.save_material_state();
                if (slot_material_index == 0) {
                    if (mesh->model.materials && mesh->model.materials[slot].maps) {
                        mesh->model.materials[slot].maps[MATERIAL_MAP_ALBEDO].texture = {0};
                        mesh->model.materials[slot].maps[MATERIAL_MAP_NORMAL].texture = {0};
                        mesh->model.materials[slot].maps[MATERIAL_MAP_ROUGHNESS].texture = {0};
                        mesh->model.materials[slot].maps[MATERIAL_MAP_METALNESS].texture = {0};
                    }
                    mat->material_slot_sources[slot].clear();
                } else if (slot_material_index < static_cast<int>(available_materials.size())) {
                    load_material_to_entity(&entity, available_materials[slot_material_index], slot);
                    mat->material_slot_sources[slot] = available_materials[slot_material_index];
                }
            }
            ImGui::PopID();
        }
    }

    selected_material_index = 0;
    if (!mat->texture_name.empty()) {
        const std::filesystem::path current_path = std::filesystem::current_path();
        std::filesystem::path material_path(mat->texture_name);
        if (material_path.is_absolute()) {
            std::error_code relative_error;
            material_path = std::filesystem::relative(material_path, current_path, relative_error);
            if (relative_error) material_path = std::filesystem::path(mat->texture_name).filename();
        }

        const std::string display_path = material_path.generic_string();
        for (int i = 1; i < static_cast<int>(available_materials.size()); ++i) {
            if (available_materials[i] == display_path) {
                selected_material_index = i;
                break;
            }
        }
    }

    ImGui::Text(lang.word("material_file"));
    if (!material_names_cstr.empty()) {
        if (ImGui::Combo("##material_combo", &selected_material_index, material_names_cstr.data(), 
                         static_cast<int>(material_names_cstr.size()))) {
            editor.save_material_state();
            if (selected_material_index == 0) {
                clear_material_textures(&entity);
                mat->texture_name.clear();
                mat->texture = {0};
                mat->texture_source = TEXTURE_NONE;
            } else if (selected_material_index > 0 && selected_material_index < static_cast<int>(available_materials.size())) {
                const std::string& selected_path = available_materials[selected_material_index];
                if (std::filesystem::exists(selected_path)) {
                    load_material_to_entity(&entity, selected_path);
                }
            }
        }
    } else {
        ImGui::Text(lang.word("no_materials_found"));
    }

    static int selected_texture_index = 0;
    std::vector<const char*> texture_names;
    texture_names.reserve(texture_options.size());
    selected_texture_index = 0;
    for (size_t i = 0; i < texture_options.size(); ++i) {
        texture_names.push_back(texture_options[i].name.c_str());
        if (texture_options[i].name == mat->albedo_texture_name)
            selected_texture_index = static_cast<int>(i);
    }

    ImGui::Text("Direct texture");
    if (!texture_names.empty() && ImGui::Combo("##direct_texture", &selected_texture_index,
        texture_names.data(), static_cast<int>(texture_names.size()))) {
        editor.save_material_state();
        if (selected_texture_index == 0) {
            mat->albedo_texture_name.clear();
            mat->texture = {0};
            if (mat->texture_name.empty()) {
                mat->texture_source = TEXTURE_NONE;
                clear_material_textures(&entity);
            } else if (std::filesystem::exists(mat->texture_name)) {
                load_material_to_entity(&entity, mat->texture_name);
            }
        } else {
            mat->albedo_texture_name = texture_options[selected_texture_index].name;
            mat->texture_name.clear();
            mat->texture = texture_options[selected_texture_index].texture;
            mat->texture_source = TEXTURE_EXTERNAL;
        }
        mark_entity_uv_dirty(&entity);
    }

    static int selected_normal_index = 0;
    selected_normal_index = 0;
    for (size_t i = 0; i < texture_options.size(); ++i) {
        if (texture_options[i].name == mat->normal_texture_name) {
            selected_normal_index = static_cast<int>(i);
            break;
        }
    }
    ImGui::Text("Normal map");
    if (!texture_names.empty() && ImGui::Combo("##normal_map", &selected_normal_index,
        texture_names.data(), static_cast<int>(texture_names.size()))) {
        editor.save_material_state();
        if (selected_normal_index == 0) {
            mat->normal_texture_name.clear();
            for (int m = 0; m < mesh->model.materialCount; ++m) {
                if (mesh->model.materials[m].maps) {
                    mesh->model.materials[m].maps[MATERIAL_MAP_NORMAL].texture = {0};
                }
            }
        } else {
            mat->normal_texture_name = texture_options[selected_normal_index].name;
            const Texture2D normal_texture = texture_options[selected_normal_index].texture;
            for (int m = 0; m < mesh->model.materialCount; ++m) {
                if (mesh->model.materials[m].maps) {
                    mesh->model.materials[m].maps[MATERIAL_MAP_NORMAL].texture = normal_texture;
                }
            }
        }
    }

    if (!mat->texture_name.empty() && mat->texture_name.length() > 4 && 
        mat->texture_name.substr(mat->texture_name.length() - 4) == ".mtl") {
        ImGui::TextDisabled("%s: %s", lang.word("current"), mat->texture_name.c_str());
    } else {
        ImGui::TextDisabled("%s: %s", lang.word("current"), lang.word("none"));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text(lang.word("uv_settings"));
    ImGui::Spacing();

    bool uv_changed = false;

    const MaterialComponent before_stretch = *mat;
    if (ImGui::Checkbox(lang.word("stretch_texture"), &mat->texture_stretch)) {
        if (ImGui::IsItemActivated()) editor.save_material_state_before(&entity, before_stretch);
        uv_changed = true;
    }

    if (!mat->texture_stretch) {
        const MaterialComponent before_repeat_u = *mat;
        if (ImGui::DragFloat(lang.word("repeat_u"), &mat->texture_repeat_u, 0.1f, 0.1f, 10.0f)) {
            if (ImGui::IsItemActivated()) editor.save_material_state_before(&entity, before_repeat_u);
            uv_changed = true;
        }

        const MaterialComponent before_repeat_v = *mat;
        if (ImGui::DragFloat(lang.word("repeat_v"), &mat->texture_repeat_v, 0.1f, 0.1f, 10.0f)) {
            if (ImGui::IsItemActivated()) editor.save_material_state_before(&entity, before_repeat_v);
            uv_changed = true;
        }

        float uv_scale[2] = {mat->uv_scale.x, mat->uv_scale.y};
        const MaterialComponent before_uv_scale = *mat;
        if (ImGui::DragFloat2(lang.word("uv_scale_x"), uv_scale, 0.1f, 0.1f, 5.0f)) {
            if (ImGui::IsItemActivated()) editor.save_material_state_before(&entity, before_uv_scale);
            mat->uv_scale = {uv_scale[0], uv_scale[1]};
            uv_changed = true;
        }
    }

    const MaterialComponent before_auto_uv = *mat;
    if (ImGui::Checkbox(lang.word("auto_uv"), &mat->auto_uv)) {
        if (ImGui::IsItemActivated()) editor.save_material_state_before(&entity, before_auto_uv);
        uv_changed = true;
    }

    if (uv_changed) {
        mark_entity_uv_dirty(&entity);
    }
}

void ComponentUIHelper::draw_light_component(Editor& editor, Entity& entity, LightComponent* light, Shader shader) {
    if (!light) return;

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        editor.save_light_state();
    }

    ImGui::Text(lang.word("light_properties"));
    ImGui::Spacing();

    bool changed = false;

    TransformComponent* transform = entity.get_transform_component();
    float light_position[3] = {
        transform ? transform->position.x : light->light.position.x,
        transform ? transform->position.y : light->light.position.y,
        transform ? transform->position.z : light->light.position.z
    };
    if (ImGui::DragFloat3(lang.word("position"), light_position, 0.1f)) {
        if (transform) {
            transform->position = { light_position[0], light_position[1], light_position[2] };
            mark_entity_bounds_dirty(&entity);
        }
        light->light.position = { light_position[0], light_position[1], light_position[2] };
        changed = true;
    }
    changed |= ImGui::DragFloat3(lang.word("target"), (float*)&light->light.target, 0.1f);

    float light_color[4] = {
        light->light.color.r / 255.0f,
        light->light.color.g / 255.0f,
        light->light.color.b / 255.0f,
        light->light.color.a / 255.0f
    };
    if (ImGui::ColorEdit4(lang.word("color"), light_color)) {
        light->light.color = {
            static_cast<unsigned char>(light_color[0] * 255.0f),
            static_cast<unsigned char>(light_color[1] * 255.0f),
            static_cast<unsigned char>(light_color[2] * 255.0f),
            static_cast<unsigned char>(light_color[3] * 255.0f)
        };
        changed = true;
    }

    changed |= ImGui::DragFloat(lang.word("intensity"), &light->light.intensity, 0.1f, 0.0f, 10.0f);
    changed |= ImGui::DragFloat(lang.word("range"), &light->light.range, 0.1f, 0.1f, 100.0f);
    changed |= ImGui::DragFloat(lang.word("spot_angle"), &light->light.spot_angle, 1.0f, 5.0f, 180.0f);

    const char* light_types[] = {
        lang.word("directional"),
        lang.word("point"),
        lang.word("spot")
    };

    const int light_type_values[] = {
        LIGHT_DIRECTIONAL,
        LIGHT_POINT,
        LIGHT_SPOT
    };

    int light_type_index = 0;
    for (int i = 0; i < IM_ARRAYSIZE(light_type_values); i++) {
        if (light->light.light.type == light_type_values[i]) {
            light_type_index = i;
            break;
        }
    }

    if (ImGui::Combo(lang.word("light_type"), &light_type_index, light_types, IM_ARRAYSIZE(light_types))) {
        light->light.light.type = light_type_values[light_type_index];
        changed = true;
    }

    if (light->enabled != light->light.enabled) {
        changed = true;
    }

    if (changed) {
        if (transform) {
            light->light.position = transform->position;
        }
        light->light.enabled = light->enabled;

        if (light->light.id != -1)
            update_lighting(shader, light->light);
    }
}

void ComponentUIHelper::draw_collision_component(Editor& editor, Entity& entity, CollisionComponent* collision) {
    if (!collision) return;

    ImGui::Text(lang.word("collision_properties"));
    ImGui::Spacing();

    bool changed = false;

    const char* collider_types[] = {
        lang.word("cube"),
        lang.word("sphere"),
        lang.word("capsule"),
        lang.word("mesh")
    };

    int collider_type = static_cast<int>(collision->collider_type);

    if (ImGui::Combo(lang.word("collider_type"), &collider_type, collider_types, IM_ARRAYSIZE(collider_types))) {
        editor.save_state();

        collision->collider_type = static_cast<ColliderType>(collider_type);
        collision->dirty = true;
        changed = true;
    }

    if (ImGui::Checkbox(lang.word("visualize"), &collision->visualize)) {
        editor.save_state();
        changed = true;
    }

    if (ImGui::DragFloat3(lang.word("center"), (float*)&collision->center, 0.1f)) {
        editor.save_state();

        collision->dirty = true;
        changed = true;
    }

    ImGui::Spacing();

    switch (collision->collider_type) {
        case COLLIDER_BOX: {
            if (ImGui::DragFloat3(lang.word("size"), (float*)&collision->size, 0.1f, 0.01f, 1000.0f)) {
                editor.save_state();

                collision->dirty = true;
                changed = true;
            }

            break;
        }

        case COLLIDER_SPHERE: {
            if (ImGui::DragFloat(lang.word("radius"), (float*)&collision->radius, 0.1f, 0.01f, 1000.0f)) {
                editor.save_state();

                collision->dirty = true;
                changed = true;
            }

            break;
        }

        case COLLIDER_CAPSULE: {
            if (ImGui::DragFloat(lang.word("radius"), (float*)&collision->radius, 0.1f, 0.01f, 1000.0f)) {
                editor.save_state();

                collision->dirty = true;
                changed = true;
            }

            if (ImGui::DragFloat(lang.word("height"), (float*)&collision->height, 0.1f, 0.1f, 1000.0f)) {
                editor.save_state();

                collision->dirty = true;
                changed = true;
            }

            break;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button(lang.word("reset"), ImVec2(-1, 0))) {
        collision->size = Vec3(1.0f, 1.0f, 1.0f);
        collision->radius = 0.5f;
        collision->height = 2.0f;
        collision->center = Vec3(0.0f, 0.0f, 0.0f);
    }
    ImGui::PopStyleColor(2);

    if (changed) {
        mark_entity_bounds_dirty(&entity);
    }
}

void ComponentUIHelper::draw_3d_text_component(Editor& editor, Entity& entity, Text3DComponent* text) {
    if (!text) return;

    char buf[256] = {};
    std::snprintf(buf, sizeof(buf), "%s", text->text.c_str());

    if (ImGui::InputText("Text", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        editor.save_state();
        text->text = buf;
        update_model(&entity);
        mark_entity_bounds_dirty(&entity);
    }

    auto drag = [&](const char* label, float& val, float spd, float mn, float mx) {
        if (ImGui::DragFloat(label, &val, spd, mn, mx)) {
            editor.save_state();
            update_model(&entity);
            mark_entity_bounds_dirty(&entity);
        }
    };

    drag("Size",           text->size,           0.01f, 0.05f, 10.f);
    drag("Thickness",      text->thickness,      0.01f, 0.01f,  5.f);
    drag("Letter Spacing", text->letter_spacing, 0.005f, 0.f, 100);

    ImGui::Separator();

    static std::vector<std::pair<std::string,std::string>> s_fonts;
    static std::vector<const char*> s_font_names;
    static int s_selected = -1;
    static bool s_scanned = false;

    if (!s_scanned) {
        s_fonts = get_system_fonts();
        s_font_names.clear();
        for (auto& f : s_fonts) s_font_names.push_back(f.first.c_str());

        for (int i = 0; i < (int)s_fonts.size(); i++) {
            if (s_fonts[i].second == text->font_path) { s_selected = i; break; }
        }
        s_scanned = true;
    }

    ImGui::Text("Font");
    if (!s_font_names.empty()) {
        if (ImGui::Combo("##font", &s_selected, s_font_names.data(), (int)s_font_names.size())) {
            editor.save_state();
            text->font_path = s_fonts[s_selected].second;
            update_model(&entity);
            mark_entity_bounds_dirty(&entity);
        }
    } 
    
    else {
        ImGui::TextDisabled("No fonts found");
    }

    if (!text->font_path.empty() && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", text->font_path.c_str());
}