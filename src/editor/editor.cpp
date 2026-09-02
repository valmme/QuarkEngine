#include "editor/editor.h"

#include "editor/editor_assets.h"
#include "editor/editor_ui.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "project.h"
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

void restore_scene_entity_models(Scene& scene) {
    for (auto& entity : scene.entities) {
        MeshComponent* mesh = entity.get_mesh_component();
        if (!mesh || !mesh->asset) continue;

        if (mesh->asset->is_procedural) {
            mesh->model = mesh->asset->generator(mesh->segments);
            mesh->owns_model_instance = true;
            store_uv(&entity);
            store_material_textures(&entity);
            apply_mesh_overrides(entity);
        } else {
            if (!load_model_instance(*mesh->asset, mesh->model)) {
                mesh->asset = nullptr;
                mesh->asset_name.clear();
                mesh->model;
                mesh->owns_model_instance = false;
                continue;
            }

            mesh->owns_model_instance = true;
            store_uv(&entity);
            store_material_textures(&entity);
            apply_mesh_overrides(entity);
        }

        mesh->shader_assigned = false;
    }
}

}

static SceneState capture_scene_state(const Scene& scene) {
    SceneState state;
    state.selected = scene.selected;
    state.selected_entities = scene.selected_entities;

    for (auto entity : scene.entities) {
        MeshComponent* mesh = entity.get_mesh_component();
        if (mesh) {
            mesh->model.materials = nullptr;
            mesh->model.meshMaterial = nullptr;
            mesh->owns_materials = false;
        }
        state.entities.push_back(entity);
    }

    return state;
}

void Editor::save_state() {
    scene_dirty = true;
    undo_stack.push(capture_scene_state(scene));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}

void Editor::undo() {
    if (undo_stack.empty()) return;

    redo_stack.push(capture_scene_state(scene));

    SceneState previous = undo_stack.top();
    undo_stack.pop();

    scene.entities = previous.entities;
    scene.selected = previous.selected;
    scene.selected_entities = previous.selected_entities;
    editor_internal::restore_scene_entity_models(scene);
}

void Editor::redo() {
    if (redo_stack.empty()) return;

    undo_stack.push(capture_scene_state(scene));
    while (undo_stack.size() > static_cast<size_t>(g_editor_preferences.undo_history_limit))
        undo_stack.pop();

    SceneState next = redo_stack.top();
    redo_stack.pop();

    scene.entities = next.entities;
    scene.selected = next.selected;
    scene.selected_entities = next.selected_entities;
    editor_internal::restore_scene_entity_models(scene);
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

    if (ctrl && IsKeyPressed(KeyboardKey::S)) {
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
