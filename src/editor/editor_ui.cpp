#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define CloseWindow WinCloseWindow
#define ShowCursor WinShowCursor
#define Rectangle WinRectangle
#include <windows.h>
#include <shlobj.h>
#include <ole2.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef near
#undef far
#endif

#include "editor/editor_ui.h"

#include "editor/editor.h"
#include "editor/editor_assets.h"
#include "editor/editor_components_ui.h"
#include "editor/editor_entity.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "ImGuizmo.h"
#include "lighting.h"
#include "version.h"
#include "project.h"
#include "tex.h"
#include "qcImGui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include "language_manager.h"
#include "editor/editor_preferences.h"

#ifdef _WIN32
#endif

#define lang LanguageManager::get()

static std::string browse_project_folder() {
#ifdef _WIN32
    char path[MAX_PATH] = {};
    BROWSEINFOA browse_info = {};
    browse_info.lpszTitle = "Select folder for scene";
    browse_info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST item_id = SHBrowseForFolderA(&browse_info);
    if (!item_id) return {};

    SHGetPathFromIDListA(item_id, path);
    CoTaskMemFree(item_id);
    return path;
#else
    return {};
#endif
}

void ApplyCustomImGuiTheme();

RenderTexture2D scene_rt = {};
bool show_hierarchy = true;
bool show_inspector = true;
bool show_assets = true;
bool show_scene = true;
bool show_preferences = false;
static bool show_exit_confirmation = false;
static Editor* pending_delete_editor = nullptr;
static Entity* pending_delete_entity = nullptr;
static Editor* deferred_hierarchy_delete_editor = nullptr;
static int deferred_hierarchy_delete_index = -1;

bool g_is_scene_hovered = false;
bool g_is_scene_active = false;

ImVec2 g_scene_window_pos = { 0, 0 };
ImVec2 g_scene_window_size = { 0, 0 };
static const Scene* g_active_scene = nullptr;

MeshEditState g_mesh_edit_state;

static const char* language_labels[] = {
    "Arabic",
    "Azerbaijani",
    "Belarusian",
    "Bosnian",
    "Brazilian Portuguese",
    "Bulgarian",
    "Catalan",
    "Chuvash",
    "Czech",
    "Danish",
    "Dutch",
    "English",
    "Esperanto",
    "Estonian",
    "Finnish",
    "French",
    "Galician",
    "German",
    "Greek",
    "Hungarian",
    "Index",
    "Italian",
    "Japanese",
    "Korean",
    "Kyrgyz",
    "License",
    "Norwegian",
    "Persian",
    "Polish",
    "Portuguese",
    "Romanian",
    "Russian",
    "Serbian",
    "Serbian (Cyrillic)",
    "Simplified Chinese",
    "Slovak",
    "Spanish",
    "Swedish",
    "Traditional Chinese",
    "Turkish",
    "Ukrainian"
};

static const char* language_codes[] = {
    "arabic",
    "azerbaijani",
    "belarusian",
    "bosnian",
    "brazilian_portuguese",
    "bulgarian",
    "catalan",
    "chuvash",
    "czech",
    "danish",
    "dutch",
    "english",
    "esperanto",
    "estonian",
    "finnish",
    "french",
    "galician",
    "german",
    "greek",
    "hungarian",
    "index",
    "italian",
    "japanese",
    "korean",
    "kyrgyz",
    "license",
    "norwegian",
    "persian",
    "polish",
    "portuguese",
    "romanian",
    "russian",
    "serbian",
    "serbian_cyrillic",
    "simplified_chinese",
    "slovak",
    "spanish",
    "swedish",
    "traditional_chinese",
    "turkish",
    "ukrainian"
};

PolygonEditMode g_poly_mode = POLY_NONE;
std::vector<int> g_selected_vertices;
static int g_drag_vertex = -1;

int find_index(const char* value) {
    int n = sizeof(language_codes) / sizeof(language_codes[0]);
    for (int i = 0; i < n; i++) {
        if (strcmp(language_codes[i], value) == 0) return i;
    }
    return 0;
}

void open_url(const char* url) {
    #ifdef _WIN32
        system((std::string("start ") + url).c_str());
    #elif __APPLE__
        system((std::string("open ") + url).c_str());
    #elif __linux__
        system((std::string("xdg-open ") + url).c_str());
    #else
        TraceLog(LogLevel::Error, "EDITOR", TextFormat("Cannot open URL: %s", url));
    #endif
}

static Mat4 compose_local_entity_transform_Mat4(const Entity& entity) {
    const TransformComponent* transform = entity.get_transform_component();
    if (!transform) return Mat4Identity();
    Mat4 matScale = Mat4Scale(transform->scale.x, transform->scale.y, transform->scale.z);
    Mat4 matRotation = Mat4RotateXYZ({transform->rotation.x * DEG2RAD, transform->rotation.y * DEG2RAD, transform->rotation.z * DEG2RAD});
    Mat4 matTranslation = Mat4Translate(transform->position.x, transform->position.y, transform->position.z);
    return Mat4Multiply(Mat4Multiply(matTranslation, matRotation), matScale);
}

static Mat4 compose_entity_world_transform_Mat4(const Scene& scene, int entity_index, std::vector<int>& stack) {
    if (entity_index < 0 || entity_index >= static_cast<int>(scene.entities.size())) return Mat4Identity();
    if (std::find(stack.begin(), stack.end(), entity_index) != stack.end())
        return compose_local_entity_transform_Mat4(scene.entities[entity_index]);

    stack.push_back(entity_index);
    const Entity& entity = scene.entities[entity_index];
    Mat4 world = compose_local_entity_transform_Mat4(entity);
    if (entity.parent_id >= 0 && entity.parent_id < static_cast<int>(scene.entities.size()))
        world = Mat4Multiply(compose_entity_world_transform_Mat4(scene, entity.parent_id, stack), world);
    stack.pop_back();
    return world;
}

Mat4 compose_entity_transform_Mat4(const Entity& entity) {
    if (!g_active_scene) return compose_local_entity_transform_Mat4(entity);
    for (int entity_index = 0; entity_index < static_cast<int>(g_active_scene->entities.size()); ++entity_index) {
        if (&g_active_scene->entities[entity_index] == &entity) {
            std::vector<int> stack;
            return compose_entity_world_transform_Mat4(*g_active_scene, entity_index, stack);
        }
    }
    return compose_local_entity_transform_Mat4(entity);
}

static Mat4 compose_mesh_world_transform(const Entity& entity) {
    const MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh) return compose_entity_transform_Mat4(entity);
    return Mat4Multiply(compose_entity_transform_Mat4(entity), mesh->model.transform);
}

Vec3 ray_plane_hit(Ray ray) {
    if (fabsf(ray.direction.y) < 0.0001f) {
        return ray.position;
    }

    float t = -ray.position.y / ray.direction.y;

    return {
        ray.position.x + ray.direction.x * t,
        0.0f,
        ray.position.z + ray.direction.z * t
    };
}

void sync_mesh_edit_state(const Editor& editor) {
    if (editor.scene.selected != g_mesh_edit_state.entity_index) {
        g_mesh_edit_state.entity_index = editor.scene.selected;
        g_mesh_edit_state.mesh_index = 0;
        g_mesh_edit_state.triangle_index = 0;
        g_mesh_edit_state.vertex_corner = 0;
        g_mesh_edit_state.was_using_gizmo = false;
        g_selected_vertices.clear();
    }
}

bool get_selected_triangle_vertices(const Entity& entity, int mesh_index, int triangle_index, int out_indices[3]) {
    const MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component || !has_valid_model_data(mesh_component->model)) return false;
    if (mesh_index < 0 || mesh_index >= mesh_component->model.meshCount) return false;

    const Mesh& mesh = mesh_component->model.meshes[mesh_index];
    if (triangle_index < 0 || triangle_index >= mesh.triangleCount) return false;
    return get_mesh_triangle_vertex_indices(mesh, triangle_index, out_indices);
}

bool get_selected_vertex_index(const Entity& entity, int mesh_index, int triangle_index, int vertex_corner, int& out_vertex_index) {
    int triangle_vertices[3] = {};
    if (!get_selected_triangle_vertices(entity, mesh_index, triangle_index, triangle_vertices)) return false;
    if (vertex_corner < 0 || vertex_corner > 2) return false;
    out_vertex_index = triangle_vertices[vertex_corner];
    return true;
}

Vec3 get_mesh_vertex_local_position(const Entity& entity, int mesh_index, int vertex_index) {
    const MeshComponent* mesh_component = entity.get_mesh_component();
    const Mesh& mesh = mesh_component->model.meshes[mesh_index];
    return {
        mesh.vertices[vertex_index * 3 + 0],
        mesh.vertices[vertex_index * 3 + 1],
        mesh.vertices[vertex_index * 3 + 2]
    };
}

Vec3 get_mesh_vertex_world_position(const Entity& entity, int mesh_index, int vertex_index) {
    const Mat4 transform = compose_mesh_world_transform(entity);
    return Vec3Transform(get_mesh_vertex_local_position(entity, mesh_index, vertex_index), transform);
}

bool ensure_mesh_edit_ready(Entity& entity) {
    if (entity_has_mesh_overrides(entity)) return true;
    MeshComponent* mesh = entity.get_mesh_component();
    if (mesh && !mesh->mesh_triangles_detached) detach_mesh_triangles(entity);
    capture_mesh_overrides_from_model(entity);
    return entity_has_mesh_overrides(entity);
}

bool set_mesh_vertex_local_position(Entity& entity, int mesh_index, int vertex_index, const Vec3& local_position) {
    MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component || !has_valid_model_data(mesh_component->model)) return false;
    if (mesh_index < 0 || mesh_index >= mesh_component->model.meshCount) return false;

    Mesh& mesh = mesh_component->model.meshes[mesh_index];
    if (!mesh.vertices || vertex_index < 0 || vertex_index >= mesh.vertexCount) return false;
    if (!ensure_mesh_edit_ready(entity)) return false;
    if (mesh_index >= static_cast<int>(mesh_component->mesh_vertex_overrides.size())) return false;

    std::vector<float>& mesh_override = mesh_component->mesh_vertex_overrides[mesh_index];
    if (mesh_override.size() != static_cast<size_t>(mesh.vertexCount * 3)) return false;

    mesh_override[vertex_index * 3 + 0] = local_position.x;
    mesh_override[vertex_index * 3 + 1] = local_position.y;
    mesh_override[vertex_index * 3 + 2] = local_position.z;

    const bool applied = apply_mesh_overrides(entity);
    if (applied) {
        mark_entity_bounds_dirty(&entity);
    }
    return applied;
}

bool set_mesh_vertex_world_position(Entity& entity, int mesh_index, int vertex_index, const Vec3& world_position) {
    const Mat4 inverse_transform = Mat4Invert(compose_mesh_world_transform(entity));
    const Vec3 local_position = Vec3Transform(world_position, inverse_transform);
    return set_mesh_vertex_local_position(entity, mesh_index, vertex_index, local_position);
}

bool pick_mesh_triangle(
    const Entity& entity,
    int mesh_index,
    Ray ray,
    int& out_triangle_index,
    int& out_vertex_corner
) {
    const MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component || !has_valid_model_data(mesh_component->model)) return false;
    if (mesh_index < 0 || mesh_index >= mesh_component->model.meshCount) return false;

    const Mesh& mesh = mesh_component->model.meshes[mesh_index];
    if (!mesh.vertices || mesh.triangleCount <= 0) return false;

    const Mat4 transform = compose_mesh_world_transform(entity);
    float best_distance = FLT_MAX;
    int best_triangle = -1;
    int best_corner = 0;

    for (int triangle_index = 0; triangle_index < mesh.triangleCount; triangle_index++) {
        int indices[3] = {};
        if (!get_mesh_triangle_vertex_indices(mesh, triangle_index, indices)) continue;

        Vec3 vertices[3] = {};
        for (int i = 0; i < 3; i++) {
            vertices[i] = transform * Vec3(
                mesh.vertices[indices[i] * 3 + 0],
                mesh.vertices[indices[i] * 3 + 1],
                mesh.vertices[indices[i] * 3 + 2]
            );
        }

        const RayCollision hit = GetRayCollisionTriangle(ray, vertices[0], vertices[1], vertices[2]);
        if (!hit.hit || hit.distance >= best_distance) continue;

        best_distance = hit.distance;
        best_triangle = triangle_index;

        float closest_corner_distance = FLT_MAX;
        for (int i = 0; i < 3; i++) {
            const float distance_to_corner = (hit.point - vertices[i]).length();
            if (distance_to_corner < closest_corner_distance) {
                closest_corner_distance = distance_to_corner;
                best_corner = i;
            }
        }
    }

    if (best_triangle < 0) return false;

    out_triangle_index = best_triangle;
    out_vertex_corner = best_corner;
    return true;
}

bool raycast_entity(const Entity& entity, Ray ray, float& out_distance) {
    const MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh || !has_valid_model_data(mesh->model)) return false;

    const Mat4 transform = compose_entity_transform_Mat4(entity);

    bool hit_any = false;
    float best_distance = FLT_MAX;

    for (int i = 0; i < mesh->model.meshCount; i++) {
        const Mesh& m = mesh->model.meshes[i];
        
        for (int j = 0; j < m.triangleCount; j++) {
            int indices[3] = {};
            if (!get_mesh_triangle_vertex_indices(m, j, indices)) continue;

            Vec3 verts[3];

            for (int k = 0; k < 3; k++) {
                verts[k] = Vec3Transform({
                    m.vertices[indices[k] * 3 + 0],
                    m.vertices[indices[k] * 3 + 1],
                    m.vertices[indices[k] * 3 + 2],
                }, transform);
            }

            RayCollision hit = GetRayCollisionTriangle(ray, verts[0], verts[1], verts[2]);

            if (hit.hit && hit.distance < best_distance) {
                best_distance = hit.distance;
                hit_any = true;
            }
        }
    }

    out_distance = best_distance;
    return hit_any;

}

void reset_mesh_edit_model(Entity& entity) {
    MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh) return;
    clear_mesh_overrides(entity);

    if (mesh->asset && mesh->asset->is_procedural) {
        update_model(&entity);
    } 
    
    else if (mesh->asset) {
        if (entity_owns_model(entity) && mesh->model.meshCount > 0) {
            UnloadModel(mesh->model);
        }

        mesh->model;
        if (!load_model_instance(*mesh->asset, mesh->model)) {
            mesh->asset = nullptr;
            mesh->asset_name.clear();
            mesh->owns_model_instance = false;
        } 
        
        else {
            mesh->owns_model_instance = true;
        }
    }

    if (mesh->asset) {
        store_uv(&entity);
        store_material_textures(&entity);
        mesh->shader_assigned = false;
    }
}

void reset_editor_layout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

    ImGui::DockBuilderDockWindow(lang.word("hierarchy"), dock_id_left);
    ImGui::DockBuilderDockWindow(lang.word("inspector"), dock_id_right);
    ImGui::DockBuilderDockWindow(lang.word("assets"), dock_id_bottom);
    ImGui::DockBuilderDockWindow(lang.word("scene"), dock_main_id);
    
    ImGui::DockBuilderFinish(dockspace_id);
    
    show_hierarchy = show_inspector = show_assets = show_scene = true;
}

void draw_gizmo(Editor& editor, FlyCamera camera) {
    sync_mesh_edit_state(editor);
    
    if (camera.active) return;

    Entity* entity = editor.scene.get_selected();
    if (!entity) return;
    int entity_index = -1;
    for (int index = 0; index < static_cast<int>(editor.scene.entities.size()); ++index) {
        if (&editor.scene.entities[index] == entity) {
            entity_index = index;
            break;
        }
    }
    TransformComponent* transform = entity->get_transform_component();
    MeshComponent* mesh = entity->get_mesh_component();
    if (!transform || !mesh) return;
    if (mesh->vertex_gizmo) return;
    if (g_scene_window_size.x <= 0 || g_scene_window_size.y <= 0) return;

    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(g_scene_window_pos.x, g_scene_window_pos.y, g_scene_window_size.x, g_scene_window_size.y);
    ImGuizmo::AllowAxisFlip(false);

    float translation_snap[3] = {
        g_editor_preferences.gizmo_translation_snap,
        g_editor_preferences.gizmo_translation_snap,
        g_editor_preferences.gizmo_translation_snap
    };
    float rotation_snap[3] = {
        g_editor_preferences.gizmo_rotation_snap,
        g_editor_preferences.gizmo_rotation_snap,
        g_editor_preferences.gizmo_rotation_snap
    };
    float scale_snap[3] = {
        g_editor_preferences.gizmo_scale_snap,
        g_editor_preferences.gizmo_scale_snap,
        g_editor_preferences.gizmo_scale_snap
    };
    const bool gizmo_snap_enabled = g_editor_preferences.gizmo_snap_enabled ||
        IsKeyDown(KeyboardKey::LeftControl) || IsKeyDown(KeyboardKey::RightControl);

    Mat4 view = Mat4::lookAt(
        camera.get_camera().position,
        camera.get_camera().target,
        camera.get_camera().up
    );

    Mat4 projection = Mat4::perspective(
        camera.get_camera().fovy * DEG2RAD,
        g_scene_window_size.x / g_scene_window_size.y,
        0.1f,
        1000.0f
    );

    float view_Mat4[16] = {};
    float projection_Mat4[16] = {};
    float transform_Mat4[16] = {};

    memcpy(view_Mat4, &view, sizeof(view_Mat4));
    memcpy(projection_Mat4, &projection, sizeof(projection_Mat4));

    draw_mesh_vertex_overlay(editor, camera.get_camera());

    if (g_mesh_edit_state.enabled && has_valid_model_data(mesh->model)) {
        if (g_mesh_edit_state.mesh_index >= mesh->model.meshCount) 
            g_mesh_edit_state.mesh_index = 0;

        int vertex_index = -1;
        if (get_selected_vertex_index(
            *entity, 
            g_mesh_edit_state.mesh_index, 
            g_mesh_edit_state.triangle_index, 
            g_mesh_edit_state.vertex_corner, 
            vertex_index)) {
            
            const Vec3 vertex_world = get_mesh_vertex_world_position(
                *entity, 
                g_mesh_edit_state.mesh_index, 
                vertex_index
            );
            
            float vertex_translation[3] = { vertex_world.x, vertex_world.y, vertex_world.z };
            float vertex_rotation[3] = { 0.0f, 0.0f, 0.0f };
            float vertex_scale[3] = { 1.0f, 1.0f, 1.0f };

            ImGuizmo::RecomposeMatrixFromComponents(
                vertex_translation, 
                vertex_rotation, 
                vertex_scale, 
                transform_Mat4
            );
            
            ImGuizmo::Manipulate(
                view_Mat4,
                projection_Mat4,
                ImGuizmo::TRANSLATE,
                ImGuizmo::WORLD,
                transform_Mat4,
                nullptr,
                gizmo_snap_enabled ? translation_snap : nullptr
            );

            if (ImGuizmo::IsUsing() && !g_mesh_edit_state.was_using_gizmo) {
                editor.save_hierarchy_state();
            }

            if (ImGuizmo::IsUsing()) {
                float next_translation[3] = {};
                float next_rotation[3] = {};
                float next_scale[3] = {};
                ImGuizmo::DecomposeMatrixToComponents(
                    transform_Mat4, 
                    next_translation, 
                    next_rotation, 
                    next_scale
                );
                
                set_mesh_vertex_world_position(
                    *entity,
                    g_mesh_edit_state.mesh_index,
                    vertex_index,
                    { next_translation[0], next_translation[1], next_translation[2] }
                );
            }

            g_mesh_edit_state.was_using_gizmo = ImGuizmo::IsUsing();
            return;
        }
    }

    float translation[3] = { transform->position.x, transform->position.y, transform->position.z };
    float rotation[3] = { transform->rotation.x, transform->rotation.y, transform->rotation.z };
    float scale[3] = { transform->scale.x, transform->scale.y, transform->scale.z };

    Mat4 gizmo_transform = compose_local_entity_transform_Mat4(*entity);
    const int parent_id = entity_index >= 0 ? entity->parent_id : -1;
    if (parent_id >= 0 && parent_id < static_cast<int>(editor.scene.entities.size())) {
        std::vector<int> stack;
        gizmo_transform = Mat4Multiply(
            compose_entity_world_transform_Mat4(editor.scene, parent_id, stack),
            gizmo_transform);
    }
    memcpy(transform_Mat4, &gizmo_transform, sizeof(transform_Mat4));
    
    static bool was_using = false;
    
    ImGuizmo::Manipulate(
        view_Mat4,
        projection_Mat4,
        editor_internal::gizmo_mode,
        ImGuizmo::WORLD,
        transform_Mat4,
        nullptr,
        gizmo_snap_enabled
            ? (editor_internal::gizmo_mode == ImGuizmo::TRANSLATE ? translation_snap :
               editor_internal::gizmo_mode == ImGuizmo::ROTATE ? rotation_snap : scale_snap)
            : nullptr
    );

    if (ImGuizmo::IsUsing() && !was_using) {
        editor.save_hierarchy_state();
    }

    if (ImGuizmo::IsUsing()) {
        if (parent_id >= 0 && parent_id < static_cast<int>(editor.scene.entities.size())) {
            std::vector<int> stack;
            const Mat4 parent_world = compose_entity_world_transform_Mat4(editor.scene, parent_id, stack);
            Mat4 world_transform = Mat4::identity();
            memcpy(&world_transform, transform_Mat4, sizeof(transform_Mat4));
            const Mat4 local_transform = parent_world.inverted() * world_transform;
            memcpy(transform_Mat4, &local_transform, sizeof(transform_Mat4));
        }

        float next_translation[3] = {};
        float next_rotation[3] = {};
        float next_scale[3] = {};
        ImGuizmo::DecomposeMatrixToComponents(
            transform_Mat4, 
            next_translation, 
            next_rotation, 
            next_scale
        );

        transform->position = { next_translation[0], next_translation[1], next_translation[2] };
        transform->rotation = { next_rotation[0], next_rotation[1], next_rotation[2] };
        transform->scale = { next_scale[0], next_scale[1], next_scale[2] };

        const Vec3 position_delta = transform->position - Vec3{translation[0], translation[1], translation[2]};
        if (editor.scene.selected_entities.size() > 1) {
            for (int selected_index : editor.scene.selected_entities) {
                if (selected_index == editor.scene.selected ||
                    selected_index < 0 || selected_index >= static_cast<int>(editor.scene.entities.size())) continue;
                Entity& selected_entity = editor.scene.entities[selected_index];
                TransformComponent* selected_transform = selected_entity.get_transform_component();
                if (!selected_transform) continue;
                selected_transform->position = selected_transform->position + position_delta;
                mark_entity_bounds_dirty(&selected_entity);
                if (MaterialComponent* selected_material = selected_entity.get_material_component();
                    selected_material && !selected_material->texture_stretch) {
                    mark_entity_uv_dirty(&selected_entity);
                }
            }
        }

        MaterialComponent* mat = entity->get_material_component();
        if (mat && !mat->texture_stretch) mark_entity_uv_dirty(entity);
    }

    was_using = ImGuizmo::IsUsing();
    g_mesh_edit_state.was_using_gizmo = false;
}

static Vec2 world_to_scene_screen(const Vec3& world, const Camera3D& camera) {
    const Mat4 view = Mat4::lookAt(camera.position, camera.target, camera.up);
    const Mat4 projection = Mat4::perspective(
        camera.fovy * DEG2RAD,
        g_scene_window_size.x / g_scene_window_size.y,
        0.1f,
        1000.0f
    );
    const Vec4 clip = projection * (view * Vec4{world.x, world.y, world.z, 1.0f});
    if (fabsf(clip.w) <= 0.000001f) return {g_scene_window_pos.x, g_scene_window_pos.y};

    const float nx = clip.x / clip.w * 0.5f + 0.5f;
    const float ny = -clip.y / clip.w * 0.5f + 0.5f;
    return {
        g_scene_window_pos.x + nx * g_scene_window_size.x,
        g_scene_window_pos.y + ny * g_scene_window_size.y
    };
}

static Ray scene_screen_to_world_ray(const Vec2& mouse, const Camera3D& camera) {
    float nx = (mouse.x - g_scene_window_pos.x) / g_scene_window_size.x;
    float ny = (mouse.y - g_scene_window_pos.y) / g_scene_window_size.y;

    Mat4 view = Mat4::lookAt(
        camera.position,
        camera.target,
        camera.up
    );

    Mat4 proj = Mat4::perspective(
        camera.fovy * DEG2RAD,
        g_scene_window_size.x / g_scene_window_size.y,
        0.1f,
        1000.0f
    );

    Mat4 vp = proj * view;
    Mat4 inv_vp = vp.inverted();

    float ndcX =  nx * 2.0f - 1.0f;
    float ndcY = -(ny * 2.0f - 1.0f);

    auto mul = [](Mat4 m, Vec4 v) -> Vec4 {
        return {
            m.m0*v.x + m.m4*v.y + m.m8*v.z  + m.m12*v.w,
            m.m1*v.x + m.m5*v.y + m.m9*v.z  + m.m13*v.w,
            m.m2*v.x + m.m6*v.y + m.m10*v.z + m.m14*v.w,
            m.m3*v.x + m.m7*v.y + m.m11*v.z + m.m15*v.w
        };
    };

    Vec4 near_w = mul(inv_vp, {ndcX, ndcY, -1.0f, 1.0f});
    Vec4 far_w  = mul(inv_vp, {ndcX, ndcY,  1.0f, 1.0f});

    Vec3 near_pos = { near_w.x/near_w.w, near_w.y/near_w.w, near_w.z/near_w.w };
    Vec3 far_pos  = { far_w.x/far_w.w,   far_w.y/far_w.w,   far_w.z/far_w.w  };

    Ray ray;
    ray.position  = near_pos;
    ray.direction = Vec3Normalize(Vec3Subtract(far_pos, near_pos));
    return ray;
}

void draw_mesh_vertex_overlay(Editor& editor, Camera3D camera) {
    sync_mesh_edit_state(editor);
    if (!g_mesh_edit_state.enabled) return;

    Entity* entity = editor.scene.get_selected();
    MeshComponent* mesh = entity ? entity->get_mesh_component() : nullptr;
    if (!entity || !mesh || !has_valid_model_data(mesh->model)) return;
    if (g_mesh_edit_state.mesh_index >= mesh->model.meshCount) g_mesh_edit_state.mesh_index = 0;

    int triangle_vertices[3] = {};
    if (!get_selected_triangle_vertices(*entity, g_mesh_edit_state.mesh_index, g_mesh_edit_state.triangle_index, triangle_vertices)) return;

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    Vec2 screen_points[3] = {};

    for (int i = 0; i < 3; i++) {
        Vec3 wp = get_mesh_vertex_world_position(*entity, g_mesh_edit_state.mesh_index, triangle_vertices[i]);
        screen_points[i] = world_to_scene_screen(wp, camera);
    }

    for (int i = 0; i < 3; i++) {
        const int next = (i + 1) % 3;
        draw_list->AddLine(
            ImVec2(screen_points[i].x, screen_points[i].y),
            ImVec2(screen_points[next].x, screen_points[next].y),
            IM_COL32(255, 210, 120, 220), 2.0f
        );
    }

    for (int i = 0; i < 3; i++) {
        const bool selected = g_mesh_edit_state.vertex_corner == i;
        const ImU32 fill = selected ? IM_COL32(255, 170, 64, 255) : IM_COL32(70, 180, 255, 240);
        const float radius = selected ? 8.0f : 6.0f;
        draw_list->AddCircleFilled(ImVec2(screen_points[i].x, screen_points[i].y), radius, fill);
        draw_list->AddCircle(ImVec2(screen_points[i].x, screen_points[i].y), radius, IM_COL32(20, 20, 20, 255), 0, 2.0f);
    }

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
    if (!g_is_scene_hovered) return;
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    const Vec2 mouse = GetMousePosition();
    float best_distance = 18.0f;
    int best_corner = -1;

    for (int i = 0; i < 3; i++) {
        const float dx = mouse.x - screen_points[i].x;
        const float dy = mouse.y - screen_points[i].y;
        if (sqrtf(dx*dx + dy*dy) < best_distance) {
            best_distance = sqrtf(dx*dx + dy*dy);
            best_corner = i;
        }
    }

    if (best_corner >= 0) {
        g_mesh_edit_state.vertex_corner = best_corner;
        return;
    }

    int picked_triangle = -1;
    int picked_corner = 0;
    if (pick_mesh_triangle(*entity, g_mesh_edit_state.mesh_index,
            scene_screen_to_world_ray(mouse, camera),
            picked_triangle, picked_corner)) {
        g_mesh_edit_state.triangle_index = picked_triangle;
        g_mesh_edit_state.vertex_corner = picked_corner;
    }
}

void handle_scene_asset_drop(Editor& editor, Camera3D camera)
{
    if (!editor_internal::scene_asset_dragging) return;
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return;

    const std::string asset_name = editor_internal::dragged_scene_asset_name;
    editor_internal::scene_asset_dragging = false;
    editor_internal::dragged_scene_asset_name.clear();

    if (ImGuizmo::IsUsing()) return;

    const ImVec2 mouse = { (float)GetMouseX(), (float)GetMouseY() };

    const bool mouse_over_scene =
        mouse.x >= g_scene_window_pos.x && mouse.x <= g_scene_window_pos.x + g_scene_window_size.x &&
        mouse.y >= g_scene_window_pos.y && mouse.y <= g_scene_window_pos.y + g_scene_window_size.y;

    if (!mouse_over_scene) return;

    const bool is_material =
        asset_name.size() >= 4 &&
        asset_name.substr(asset_name.size() - 4) == ".mtl";

    if (is_material)
    {
        Ray ray = GetScreenToWorldRay({ mouse.x, mouse.y }, camera);

        Entity* hit_entity = nullptr;
        float best_distance = FLT_MAX;

        for (Entity& entity : editor.scene.entities)
        {
            const MeshComponent* mesh_comp = entity.get_mesh_component();
            if (!mesh_comp || !has_valid_model_data(mesh_comp->model)) continue;

            Mat4 transform_mat = compose_entity_transform_Mat4(entity);

            for (int i = 0; i < mesh_comp->model.meshCount; i++)
            {
                const Mesh& mesh = mesh_comp->model.meshes[i];

                BoundingBox box = GetMeshBoundingBox(mesh);

                Vec3 corners[8] = {
                    { box.min.x, box.min.y, box.min.z },
                    { box.max.x, box.min.y, box.min.z },
                    { box.min.x, box.max.y, box.min.z },
                    { box.max.x, box.max.y, box.min.z },
                    { box.min.x, box.min.y, box.max.z },
                    { box.max.x, box.min.y, box.max.z },
                    { box.min.x, box.max.y, box.max.z },
                    { box.max.x, box.max.y, box.max.z }
                };

                BoundingBox world_box = {
                    { FLT_MAX, FLT_MAX, FLT_MAX },
                    { -FLT_MAX, -FLT_MAX, -FLT_MAX }
                };

                for (int c = 0; c < 8; c++)
                {
                    Vec3 p = Vec3Transform(corners[c], transform_mat);

                    world_box.min.x = std::min(world_box.min.x, p.x);
                    world_box.min.y = std::min(world_box.min.y, p.y);
                    world_box.min.z = std::min(world_box.min.z, p.z);

                    world_box.max.x = std::max(world_box.max.x, p.x);
                    world_box.max.y = std::max(world_box.max.y, p.y);
                    world_box.max.z = std::max(world_box.max.z, p.z);
                }

                if (!GetRayCollisionBox(ray, world_box).hit)
                    continue;

                float mesh_best = FLT_MAX;
                bool mesh_hit = false;

                for (int t = 0; t < mesh.triangleCount; t++)
                {
                    int indices[3];
                    if (!get_mesh_triangle_vertex_indices(mesh, t, indices))
                        continue;

                    Vec3 v0 = Vec3Transform(
                        {
                            mesh.vertices[indices[0] * 3 + 0],
                            mesh.vertices[indices[0] * 3 + 1],
                            mesh.vertices[indices[0] * 3 + 2]
                        },
                        transform_mat
                    );

                    Vec3 v1 = Vec3Transform(
                        {
                            mesh.vertices[indices[1] * 3 + 0],
                            mesh.vertices[indices[1] * 3 + 1],
                            mesh.vertices[indices[1] * 3 + 2]
                        },
                        transform_mat
                    );

                    Vec3 v2 = Vec3Transform(
                        {
                            mesh.vertices[indices[2] * 3 + 0],
                            mesh.vertices[indices[2] * 3 + 1],
                            mesh.vertices[indices[2] * 3 + 2]
                        },
                        transform_mat
                    );

                    RayCollision hit = GetRayCollisionTriangle(ray, v0, v1, v2);

                    if (hit.hit && hit.distance < mesh_best)
                    {
                        mesh_best = hit.distance;
                        mesh_hit = true;
                    }
                }

                if (mesh_hit && mesh_best < best_distance)
                {
                    best_distance = mesh_best;
                    hit_entity = &entity;
                }
            }
        }

        if (hit_entity && hit_entity->get_material_component())
        {
            editor.save_state();
            load_material_to_entity(hit_entity, asset_name);
            mark_entity_uv_dirty(hit_entity);
        }

        return;
    }

    if (asset_name.size() >= 7 && asset_name.substr(asset_name.size() - 7) == ".prefab") {
        Entity e = make_entity_from_prefab(editor.scene, asset_name);
        
        MeshComponent* mesh = e.get_mesh_component();
        TransformComponent* transform = e.get_transform_component();

        if (!mesh || !transform || !has_valid_model_data(mesh->model)) return;

        editor.save_state();
        transform->position = get_scene_drop_position(camera);

        editor.scene.entities.push_back(e);
        editor.scene.selected = (int)editor.scene.entities.size() - 1;

        return;
    }

    ModelAsset* asset = find_asset_by_name(asset_name);
    if (!asset) return;

    Entity entity = make_entity_from_asset(editor.scene, *asset);
    MeshComponent* mesh = entity.get_mesh_component();
    TransformComponent* transform = entity.get_transform_component();

    if (!mesh || !transform || !has_valid_model_data(mesh->model)) return;

    editor.save_state();
    transform->position = get_scene_drop_position(camera);

    editor.scene.entities.push_back(entity);
    editor.scene.selected = (int)editor.scene.entities.size() - 1;
}

bool polygon_create_vertex(Entity& entity, const Vec3& world_position) {
    MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh) return false;

    EditableVertex vert;
    vert.position = Vec3Transform(world_position, Mat4Invert(compose_mesh_world_transform(entity)));
    mesh->editable_mesh.vertices.push_back(vert);
    return true;
}

void polygon_create_triangle(Entity& entity, int a, int b, int c) {
    MeshComponent* mesh = entity.get_mesh_component();
    mesh->editable_mesh.triangles.push_back({a, b, c});

    rebuild_mesh_from_editable(mesh->model, mesh->editable_mesh);
}

void draw_polygon_editor(Editor& editor, Camera3D camera) {
    Entity* entity = editor.scene.get_selected();
    if (!entity) return;

    MeshComponent* mesh = entity->get_mesh_component();
    if (!mesh || !mesh->vertex_gizmo) return;

    EditableMesh& e_mesh = mesh->editable_mesh;
    if (g_selected_vertices.empty() && !e_mesh.vertices.empty())
        g_selected_vertices.push_back(0);

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const Mat4 mesh_world_transform = compose_mesh_world_transform(*entity);
    const auto to_world = [&mesh_world_transform](const Vec3& local_position) {
        return Vec3Transform(local_position, mesh_world_transform);
    };

    for (const auto& tri : e_mesh.triangles) {
        if (tri.a >= (int)e_mesh.vertices.size()) continue;
        if (tri.b >= (int)e_mesh.vertices.size()) continue;
        if (tri.c >= (int)e_mesh.vertices.size()) continue;

        Vec2 p1 = world_to_scene_screen(to_world(e_mesh.vertices[tri.a].position), camera);
        Vec2 p2 = world_to_scene_screen(to_world(e_mesh.vertices[tri.b].position), camera);
        Vec2 p3 = world_to_scene_screen(to_world(e_mesh.vertices[tri.c].position), camera);

        draw->AddLine({p1.x,p1.y}, {p2.x,p2.y}, IM_COL32(0,255,0,255), 2.0f);
        draw->AddLine({p2.x,p2.y}, {p3.x,p3.y}, IM_COL32(0,255,0,255), 2.0f);
        draw->AddLine({p3.x,p3.y}, {p1.x,p1.y}, IM_COL32(0,255,0,255), 2.0f);
    }

    for (int i = 0; i < (int)e_mesh.vertices.size(); i++) {
        Vec2 screen = world_to_scene_screen(to_world(e_mesh.vertices[i].position), camera);

        bool selected = false;
        for (int si : g_selected_vertices)
            if (si == i) { selected = true; break; }

        ImU32 fill = selected ? IM_COL32(255,180,60,255) : IM_COL32(0,220,0,255);
        float radius = selected ? 9.0f : 6.0f;
        draw->AddCircleFilled({screen.x,screen.y}, radius, fill);
        draw->AddCircle({screen.x,screen.y}, radius, IM_COL32(20,20,20,255), 0, 2.0f);
    }

    if (g_selected_vertices.size() == 1 && g_poly_mode != POLY_CREATE) {
        int sel = g_selected_vertices[0];
        if (sel >= 0 && sel < (int)e_mesh.vertices.size()) {
            Vec3 world_pos = to_world(e_mesh.vertices[sel].position);

            float view_Mat4[16] = {};
            float projection_Mat4[16] = {};
            float transform_Mat4[16] = {};

            Mat4 view = Mat4::lookAt(camera.position, camera.target, camera.up);
            Mat4 projection = Mat4::perspective(
                camera.fovy * DEG2RAD,
                g_scene_window_size.x / g_scene_window_size.y,
                0.1f,
                1000.0f
            );

            memcpy(view_Mat4, &view, sizeof(view_Mat4));
            memcpy(projection_Mat4, &projection, sizeof(projection_Mat4));

            float t[3] = {world_pos.x, world_pos.y, world_pos.z};
            float r[3] = {0,0,0};
            float s[3] = {1,1,1};

            ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
            ImGuizmo::SetRect(g_scene_window_pos.x, g_scene_window_pos.y, g_scene_window_size.x, g_scene_window_size.y);
            ImGuizmo::RecomposeMatrixFromComponents(t, r, s, transform_Mat4);

            static bool was_using_poly_gizmo = false;
            const bool gizmo_snap_enabled = g_editor_preferences.gizmo_snap_enabled ||
                IsKeyDown(KeyboardKey::LeftControl) || IsKeyDown(KeyboardKey::RightControl);
            const float snap[3] = {
                g_editor_preferences.gizmo_translation_snap,
                g_editor_preferences.gizmo_translation_snap,
                g_editor_preferences.gizmo_translation_snap
            };
            ImGuizmo::Manipulate(
                view_Mat4,
                projection_Mat4,
                ImGuizmo::TRANSLATE,
                ImGuizmo::WORLD,
                transform_Mat4,
                nullptr,
                gizmo_snap_enabled ? snap : nullptr
            );

            if (ImGuizmo::IsUsing() && !was_using_poly_gizmo)
                editor.save_state();

            if (ImGuizmo::IsUsing()) {
                float nt[3]={}, nr[3]={}, ns[3]={};
                ImGuizmo::DecomposeMatrixToComponents(transform_Mat4, nt, nr, ns);
                e_mesh.vertices[sel].position = Vec3Transform(
                    {nt[0], nt[1], nt[2]},
                    Mat4Invert(mesh_world_transform)
                );
                rebuild_mesh_from_editable(mesh->model, e_mesh);
                mark_entity_bounds_dirty(entity);
            }

            was_using_poly_gizmo = ImGuizmo::IsUsing();
        }
    }

    if (!g_is_scene_hovered) return;
    if (ImGuizmo::IsUsing()) return;

    const Vec2 mouse = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && g_poly_mode != POLY_CREATE) {
        float best_dist = 16.0f;
        int best_vert = -1;

        for (int i = 0; i < (int)e_mesh.vertices.size(); i++) {
            Vec2 sp = world_to_scene_screen(to_world(e_mesh.vertices[i].position), camera);
            float dx = mouse.x - sp.x, dy = mouse.y - sp.y;
            float d = sqrtf(dx*dx + dy*dy);
            if (d < best_dist) { best_dist = d; best_vert = i; }
        }

        if (best_vert >= 0) {
            bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            if (ctrl) {
                auto it = std::find(g_selected_vertices.begin(), g_selected_vertices.end(), best_vert);
                if (it != g_selected_vertices.end()) g_selected_vertices.erase(it);
                else g_selected_vertices.push_back(best_vert);
            } 
            
            else {
                g_selected_vertices = {best_vert};
            }
        } 
        
        else {
            Ray ray = scene_screen_to_world_ray(mouse, camera);
            float best_hit_dist = FLT_MAX;
            int picked_triangle = -1;
            int picked_corner = 0;

            for (int t = 0; t < (int)e_mesh.triangles.size(); t++) {
                const auto& tri = e_mesh.triangles[t];
                if (tri.a >= (int)e_mesh.vertices.size()) continue;
                if (tri.b >= (int)e_mesh.vertices.size()) continue;
                if (tri.c >= (int)e_mesh.vertices.size()) continue;

                Vec3 va = to_world(e_mesh.vertices[tri.a].position);
                Vec3 vb = to_world(e_mesh.vertices[tri.b].position);
                Vec3 vc = to_world(e_mesh.vertices[tri.c].position);

                RayCollision hit = GetRayCollisionTriangle(ray, va, vb, vc);
                if (!hit.hit || hit.distance >= best_hit_dist) continue;
                best_hit_dist = hit.distance;
                picked_triangle = t;

                float cd[3] = {
                    Vec3Distance(hit.point, va),
                    Vec3Distance(hit.point, vb),
                    Vec3Distance(hit.point, vc)
                };

                picked_corner = (cd[0]<cd[1]) ? (cd[0]<cd[2]?0:2) : (cd[1]<cd[2]?1:2);
            }

            if (picked_triangle >= 0) {
                const auto& tri = e_mesh.triangles[picked_triangle];
                int verts[3] = {tri.a, tri.b, tri.c};
                bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

                if (!ctrl) g_selected_vertices.clear();
                int pick = verts[picked_corner];
                if (std::find(g_selected_vertices.begin(), g_selected_vertices.end(), pick) == g_selected_vertices.end())
                    g_selected_vertices.push_back(pick);

            }
            
            else if (!IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL)) {
                g_selected_vertices.clear();
            }
        }
    }

    if (g_poly_mode == POLY_CREATE && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Ray ray = scene_screen_to_world_ray(mouse, camera);
        Vec3 place_pos = {};
        bool hit_mesh = false;

        for (int t = 0; t < (int)e_mesh.triangles.size(); t++) {
            const auto& tri = e_mesh.triangles[t];
            if (tri.a >= (int)e_mesh.vertices.size()) continue;
            if (tri.b >= (int)e_mesh.vertices.size()) continue;
            if (tri.c >= (int)e_mesh.vertices.size()) continue;

            RayCollision hit = GetRayCollisionTriangle(ray,
                to_world(e_mesh.vertices[tri.a].position),
                to_world(e_mesh.vertices[tri.b].position),
                to_world(e_mesh.vertices[tri.c].position));

            if (hit.hit) { place_pos = hit.point; hit_mesh = true; break; }
        }

        if (!hit_mesh) place_pos = ray_plane_hit(ray);

        editor.save_state();
        polygon_create_vertex(*entity, place_pos);

        int new_index = (int)e_mesh.vertices.size() - 1;
        g_selected_vertices.push_back(new_index);

        if ((int)g_selected_vertices.size() == 3) {
            polygon_create_triangle(*entity, g_selected_vertices[0], g_selected_vertices[1], g_selected_vertices[2]);
            g_selected_vertices.clear();
        }
    }
}

void copy_entity(Entity* entity) {
    using namespace editor_internal;

    clipboard_data = *entity;
    has_clipboard = true;
}

void paste_entity(Editor& editor) {
    using namespace editor_internal;

    const MeshComponent* clipboard_mesh = clipboard_data.get_mesh_component();
    const TransformComponent* clipboard_transform = clipboard_data.get_transform_component();
    const LightComponent* clipboard_light = clipboard_data.get_light_component();
    const MaterialComponent* clipboard_mat = clipboard_data.get_material_component();
    ModelAsset* asset = (clipboard_mesh && !clipboard_mesh->asset_name.empty())
        ? find_asset_by_name(clipboard_mesh->asset_name)
        : nullptr;

    if (asset) {
        editor.save_state();
        Entity pasted = make_entity_from_asset(editor.scene, *asset);
        if (auto pasted_transform = pasted.get_transform_component(); pasted_transform && clipboard_transform) {
            pasted_transform->position = clipboard_transform->position;
            pasted_transform->rotation = clipboard_transform->rotation;
            pasted_transform->scale = clipboard_transform->scale;
        }

        auto pasted_mesh = pasted.get_mesh_component();
        auto pasted_mat  = pasted.get_material_component();

        if (pasted_mesh && clipboard_mesh) {
            pasted_mat->color = clipboard_mat->color;
            pasted_mat->outline_color = clipboard_mat->outline_color;
            pasted_mat->texture_source = clipboard_mat->texture_source;
            pasted_mat->texture_name = clipboard_mat->texture_name;
            pasted_mat->texture = clipboard_mat->texture;
            pasted_mat->texture_stretch = clipboard_mat->texture_stretch;
            pasted_mat->auto_uv = clipboard_mat->auto_uv;
            pasted_mat->texture_repeat_u = clipboard_mat->texture_repeat_u;
            pasted_mat->texture_repeat_v = clipboard_mat->texture_repeat_v;
            pasted_mat->uv_scale = clipboard_mat->uv_scale;
            pasted_mesh->mesh_triangles_detached = clipboard_mesh->mesh_triangles_detached;
            pasted_mesh->mesh_vertex_overrides = clipboard_mesh->mesh_vertex_overrides;
            apply_mesh_overrides(pasted);
        }
        if (clipboard_light) {
            auto light_copy = std::make_shared<LightComponent>(*clipboard_light);
            const int light_type = light_copy->light.light.type;
            light_copy->created = false;
            light_copy->light.id = -1;
            light_copy->light.light = {};
            light_copy->light.light.type = light_type;
            pasted.get_components()->add_component(light_copy);
        }
        editor.scene.entities.push_back(pasted);
        editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
    }
}

Entity clone_entity_instance(const Entity& source, Scene& scene) {
    Entity copy = source;
    MeshComponent* mesh = copy.get_mesh_component();
    if (mesh) {
        mesh->model = {};
        mesh->owns_model_instance = false;
        mesh->owns_materials = false;

        if (mesh->asset && (mesh->asset->is_procedural
            ? static_cast<bool>(mesh->asset->generator)
            : !mesh->asset->filepath.empty())) {
            if (mesh->asset->is_procedural) {
                mesh->model = mesh->asset->generator(mesh->segments);
            } else {
                load_model_instance(*mesh->asset, mesh->model);
            }
            mesh->owns_model_instance = mesh->model.meshes != nullptr;
            store_uv(&copy);
            store_material_textures(&copy);
            apply_mesh_overrides(copy);
            refresh_entity_render_state(copy);
        }
    }

    if (auto light = copy.get_light_component()) {
        const int light_type = light->light.light.type;
        light->created = false;
        light->light.id = -1;
        light->light.light = {};
        light->light.light.type = light_type;
        light->light.light.enabled = light->light.enabled;
        light->light.light.color = light->light.color;
        light->light.light.position = light->light.position;
        light->light.light.target = light->light.target;
    }

    copy.id = static_cast<int>(scene.entities.size());
    copy.name = scene.make_default_name_for(copy);
    return copy;
}

void dublicate_entity(Editor& editor, Entity* entity) {
    if (!entity) return;
    const int source_index = static_cast<int>(entity - editor.scene.entities.data());
    editor.save_duplicate_state(source_index);
    Entity copy = clone_entity_instance(*entity, editor.scene);
    editor.scene.entities.push_back(copy);
    editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
}

void erase_entity_after_hierarchy(Editor& editor, int index) {
    if (index < 0 || index >= static_cast<int>(editor.scene.entities.size())) return;

    Entity& entity = editor.scene.entities[index];
    if (auto light = entity.get_light_component(); light && light->created) {
        light->light.enabled = false;
        free_light_id(light->light.id);
        light->created = false;
        light->light.id = -1;
    }

    if (auto mesh = entity.get_mesh_component(); mesh && mesh->owns_model_instance && mesh->model.meshes) {
        UnloadModel(mesh->model);
        mesh->owns_model_instance = false;
    }

    editor.scene.entities.erase(editor.scene.entities.begin() + index);
    for (int current = 0; current < static_cast<int>(editor.scene.entities.size()); ++current) {
        Entity& remaining = editor.scene.entities[current];
        remaining.id = current;
        if (remaining.parent_id == index) remaining.parent_id = -1;
        else if (remaining.parent_id > index) remaining.parent_id--;
    }

    editor.scene.selected = -1;
    editor.scene.selected_entities.clear();
}

void delete_entity(Editor& editor, Entity* entity) {
    if (!entity) return;
    if (g_editor_preferences.confirm_delete) {
        pending_delete_editor = &editor;
        pending_delete_entity = entity;
        ImGui::OpenPopup("Confirm Delete");
        return;
    }

    const int index = static_cast<int>(entity - editor.scene.entities.data());
    erase_entity_after_hierarchy(editor, index);
}

static void draw_bottom_status_bar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) return;

    const float status_bar_height = 28.0f;
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 5.0f));
    if (ImGui::BeginViewportSideBar("##main_status_bar", viewport, ImGuiDir_Down, status_bar_height, flags)) {
        ImGui::TextDisabled("Quark Engine Editor v%s", "1.0.0");

        const char* fps_text = TextFormat("FPS: %d", GetFPS());
        const float fps_text_width = ImGui::CalcTextSize(fps_text).x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - fps_text_width - 10.0f);
        ImGui::Text("%s", fps_text);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void draw_ui(Editor& editor, Shader shader, FlyCamera& camera, PluginContext* plugin_ctx) {
    g_active_scene = &editor.scene;
    using namespace editor_internal;

    ImVec4& selection_style = ImGui::GetStyle().Colors[ImGuiCol_HeaderActive];
    selection_style = ImVec4(
        g_editor_preferences.selection_red / 255.0f,
        g_editor_preferences.selection_green / 255.0f,
        g_editor_preferences.selection_blue / 255.0f,
        1.0f
    );

    ImGuizmo::BeginFrame();

    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport(), dockspace_flags);

    static bool checked_layout_marker = false;
    const char* ini_filename = ImGui::GetIO().IniFilename;
    const std::filesystem::path ini_path = ini_filename ? ini_filename : "imgui.ini";
    const std::filesystem::path layout_marker = ini_path.parent_path() / ".quark_layout_initialized";
    if (!checked_layout_marker && !std::filesystem::exists(layout_marker)) {
        reset_editor_layout(dockspace_id);
        std::ofstream(layout_marker).close();
    }
    checked_layout_marker = true;

    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        reset_editor_layout(dockspace_id);
    }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(lang.word("file"))) {
            editor.plugin_manager->draw_ui_region(UI_MENU_FILE, *plugin_ctx);

            if (ImGui::MenuItem(lang.word("save"), "Ctrl+S")) {
                project_save(editor.project_path, editor.scene);
                editor.scene_dirty = false;
            }
            if (ImGui::MenuItem("Save As...")) {
                const std::string selected_folder = browse_project_folder();
                if (!selected_folder.empty()) {
                    project_save(selected_folder, editor.scene);
                    editor.project_path = selected_folder;
                    editor.current_asset_path = std::filesystem::path(selected_folder) / "resources";
                    std::error_code error;
                    std::filesystem::create_directories(editor.current_asset_path, error);
                    editor.scene_dirty = false;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(lang.word("exit"))) {
                if (g_editor_preferences.confirm_exit && editor.scene_dirty)
                    show_exit_confirmation = true;
                else
                    CloseWindow();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(lang.word("edit"))) {
            editor.plugin_manager->draw_ui_region(UI_MENU_EDIT, *plugin_ctx);

            if (ImGui::MenuItem(lang.word("undo"), "Ctrl+Z")) editor.undo();
            if (ImGui::MenuItem(lang.word("redo"), "Ctrl+Y")) editor.redo();

            ImGui::Separator();

            Entity* entity = editor.scene.get_selected();
            if (ImGui::MenuItem(lang.word("copy"), "Ctrl+C", false, entity != nullptr)) {
                copy_entity(entity);
            }

            if (ImGui::MenuItem(lang.word("paste"), "Ctrl+V", false, has_clipboard)) {
                paste_entity(editor);
            }

            if (ImGui::MenuItem(lang.word("dublicate"), "Ctrl+D", false, entity != nullptr)) {
                dublicate_entity(editor, entity);
            }

            ImGui::Separator();

            if (ImGui::MenuItem(lang.word("delete"), "Del", false, entity != nullptr)) {
                delete_entity(editor, entity);
            }

            ImGui::Separator();
            if (ImGui::MenuItem(lang.word("preferences"))) {
                show_preferences = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(lang.word("layout"))) {
            if (ImGui::MenuItem(lang.word("reset_layout"))) {
                reset_editor_layout(dockspace_id);
            }
            ImGui::Separator();
            ImGui::MenuItem(lang.word("hierarchy"), nullptr, &show_hierarchy);
            ImGui::MenuItem(lang.word("inspector"), nullptr, &show_inspector);
            ImGui::MenuItem(lang.word("assets"), nullptr, &show_assets);
            ImGui::MenuItem(lang.word("scene"), nullptr, &show_scene);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(lang.word("create"))) {
            for (auto& asset : assets) {
                if (!asset.is_procedural) continue;
                if (ImGui::MenuItem(asset.name.c_str())) {
                    editor.save_state();
                    Entity created = make_entity_from_asset(editor.scene, asset);
                    const MeshComponent* created_mesh = created.get_mesh_component();
                    if (created_mesh && has_valid_model_data(created_mesh->model)) {
                        editor.scene.entities.push_back(created);
                        editor.scene.selected = static_cast<int>(editor.scene.entities.size()) - 1;
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu(lang.word("help"))) {
            editor.plugin_manager->draw_ui_region(UI_MENU_HELP, *plugin_ctx);

            if (ImGui::MenuItem(lang.word("about"))) show_about_window = true;
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    draw_bottom_status_bar();

    ImGuiIO& io = ImGui::GetIO();

    if (show_hierarchy) {
        ImGui::Begin(lang.word("hierarchy"), &show_hierarchy);
        editor.plugin_manager->draw_ui_region(UI_HIERARCHY, *plugin_ctx);

        if (editor.scene.selected_entities.empty() && editor.scene.selected >= 0)
            editor.scene.select_entity(editor.scene.selected, false);

    auto accept_entity_drop = [&](int target_index) {
        if (!ImGui::BeginDragDropTarget()) return;

        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_INDEX")) {
            if (payload->IsDelivery() && payload->DataSize == sizeof(int)) {
                const int dropped_index = *static_cast<const int*>(payload->Data);
                if (dropped_index != target_index) {
                    editor.save_hierarchy_state();
                    move_entity_to_parent(editor.scene, dropped_index, target_index);
                }
            }
        }

        if (ImGui::IsDragDropPayloadBeingAccepted()) {
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetItemRectMin(),
                ImGui::GetItemRectMax(),
                IM_COL32(80, 180, 255, 255),
                2.0f,
                0,
                2.0f
            );
        }

        ImGui::EndDragDropTarget();
    };

    auto draw_entity_item = [&](int entity_index) {
        Entity& entity = editor.scene.entities[entity_index];
        const bool selected = editor.scene.is_selected(entity_index);
        
        ImGui::PushID(entity_index);
        
        if (ImGui::Selectable(entity.name.c_str(), selected)) {
            const bool ctrl = ImGui::GetIO().KeyCtrl;
            editor.scene.select_entity(entity_index, ctrl);
        }
        
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("ENTITY_INDEX", &entity_index, sizeof(int));
            ImGui::Text("%s", entity.name.c_str());
            ImGui::EndDragDropSource();
        }

        accept_entity_drop(entity_index);
        
        if (ImGui::BeginPopupContextItem(TextFormat("context_%d", entity.id))) {
            if (ImGui::MenuItem(lang.word("delete"))) {
                editor.save_state();

                deferred_hierarchy_delete_editor = &editor;
                deferred_hierarchy_delete_index = entity_index;

                ImGui::EndPopup();
                ImGui::PopID();
                return;
            }

            if (ImGui::MenuItem(lang.word("rename"))) {
                editor.save_state();
                renaming_index = entity_index;
                const size_t copied = entity.name.copy(rename_buf, sizeof(rename_buf) - 1);
                rename_buf[copied] = '\0';
            }

            if (ImGui::MenuItem(lang.word("dublicate"))) {
                const int source_index = static_cast<int>(&entity - editor.scene.entities.data());
                editor.save_duplicate_state(source_index);
                Entity copy = clone_entity_instance(entity, editor.scene);
                editor.scene.entities.push_back(copy);
            }

            ImGui::EndPopup();
        }
        
        ImGui::PopID();
    };

    std::function<void(int)> draw_entity_tree = [&](int parent_id) {
        auto children = get_entity_children(editor.scene, parent_id);
        for (int child_idx : children) {
            Entity& child = editor.scene.entities[child_idx];
            
            if (child.is_group) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
                if (editor.scene.is_selected(child_idx)) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }
                
                bool open = ImGui::TreeNodeEx(child.name.c_str(), flags);
                
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    ImGui::SetDragDropPayload("ENTITY_INDEX", &child_idx, sizeof(int));
                    ImGui::Text("%s", child.name.c_str());
                    ImGui::EndDragDropSource();
                }
                
                accept_entity_drop(child_idx);
                
                if (ImGui::BeginPopupContextItem(TextFormat("group_context_%d", child.id))) {
                    if (ImGui::MenuItem(lang.word("delete"))) {
                        editor.save_state();
                        deferred_hierarchy_delete_editor = &editor;
                        deferred_hierarchy_delete_index = child_idx;
                        ImGui::EndPopup();
                        if (open) ImGui::TreePop();
                        return;
                    }

                    if (ImGui::MenuItem(lang.word("rename"))) {
                        editor.save_state();
                        renaming_index = child_idx;
                        const size_t copied = child.name.copy(rename_buf, sizeof(rename_buf) - 1);
                        rename_buf[copied] = '\0';
                    }

                    if (ImGui::MenuItem(lang.word("dublicate"))) {
                        editor.save_duplicate_state(child_idx);
                        Entity copy = clone_entity_instance(child, editor.scene);
                        editor.scene.entities.push_back(copy);
                    }

                    ImGui::EndPopup();
                }
                
                ImGui::PushID(child_idx);
                if (ImGui::IsItemClicked()) {
                    const bool ctrl = ImGui::GetIO().KeyCtrl;
                    editor.scene.select_entity(child_idx, ctrl);
                }
                ImGui::PopID();
                
                if (open) {
                    draw_entity_tree(child_idx);
                    ImGui::TreePop();
                }
            } else {
                draw_entity_item(child_idx);
                const auto nested_children = get_entity_children(editor.scene, child_idx);
                if (!nested_children.empty()) {
                    ImGui::Indent();
                    draw_entity_tree(child_idx);
                    ImGui::Unindent();
                }
            }
        }
    };
    
    draw_entity_tree(-1);

    const ImVec2 hierarchy_space = ImGui::GetContentRegionAvail();
    if (hierarchy_space.x > 1.0f && hierarchy_space.y > 1.0f) {
        ImGui::InvisibleButton("HierarchyRootDropZone", hierarchy_space);
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_INDEX")) {
                if (payload->IsDelivery() && payload->DataSize == sizeof(int)) {
                    const int dropped_index = *static_cast<const int*>(payload->Data);
                    if (dropped_index >= 0 && dropped_index < static_cast<int>(editor.scene.entities.size()) &&
                        editor.scene.entities[dropped_index].parent_id != -1) {
                        editor.save_hierarchy_state();
                        move_entity_to_parent(editor.scene, dropped_index, -1);
                    }
                }
            }

            if (ImGui::IsDragDropPayloadBeingAccepted()) {
                ImGui::GetWindowDrawList()->AddRect(
                    ImGui::GetItemRectMin(),
                    ImGui::GetItemRectMax(),
                    IM_COL32(80, 180, 255, 255),
                    2.0f,
                    0,
                    2.0f
                );
            }
            ImGui::EndDragDropTarget();
        }
    }

    if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu(lang.word("create"))) {
            editor.save_state();
            for (int asset_index = 0; asset_index < static_cast<int>(assets.size()); asset_index++) {
                auto& asset = assets[asset_index];
                const std::string label = asset.name + "##create_" + std::to_string(asset_index);
                if (ImGui::MenuItem(label.c_str())) {
                    Entity entity = make_entity_from_asset(editor.scene, asset);
                    const MeshComponent* mesh = entity.get_mesh_component();
                    if (!mesh || !has_valid_model_data(mesh->model)) continue;
                    editor.scene.entities.push_back(entity);
                }
            }
            ImGui::EndMenu();
        }
        
        ImGui::Separator();
        if (ImGui::MenuItem(lang.word("create_group"))) {
            editor.save_state();
            create_group(editor.scene, "Group", -1);
        }
        
        ImGui::EndPopup();
    }
    
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_INDEX")) {
            int dropped_index = *(const int*)payload->Data;
            editor.save_hierarchy_state();
            move_entity_to_parent(editor.scene, dropped_index, -1);
        }
        ImGui::EndDragDropTarget();
    }

    if (deferred_hierarchy_delete_editor == &editor && deferred_hierarchy_delete_index >= 0) {
        const int index = deferred_hierarchy_delete_index;
        deferred_hierarchy_delete_editor = nullptr;
        deferred_hierarchy_delete_index = -1;
        erase_entity_after_hierarchy(editor, index);
    }
    ImGui::End();
    }

    if (show_inspector) {
        ImGui::Begin(lang.word("inspector"), &show_inspector);
        editor.plugin_manager->draw_ui_region(UI_INSPECTOR, *plugin_ctx);
        
        ImGui::Text(lang.word("mode"));
        ImGui::SameLine();
        if (ImGui::Button("P")) gizmo_mode = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::Button("R")) gizmo_mode = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::Button("S")) gizmo_mode = ImGuizmo::SCALE;

        Entity* entity = editor.scene.get_selected();
        if (entity) {
            ImGui::Separator();
            ImGui::Spacing();
            
            char inspector_name[128] = {};
            const size_t copied = entity->name.copy(inspector_name, sizeof(inspector_name) - 1);
            inspector_name[copied] = '\0';

            static std::string last_name;
            if (ImGui::InputText(lang.word("name"), inspector_name, IM_ARRAYSIZE(inspector_name))) {
                if (last_name != inspector_name) {
                    editor.save_state();
                    assign_entity_name(*entity, inspector_name);
                    last_name = inspector_name;
                }
            } else {
                last_name = inspector_name;
            }

            ImGui::Spacing();
            const char* current_tag = entity->tags.empty() ? "Untagged" : entity->tags.front().c_str();
            bool open_add_tag_popup = false;
            if (ImGui::BeginCombo("Tag", current_tag)) {
                if (ImGui::Selectable("Untagged", entity->tags.empty())) {
                    if (!entity->tags.empty()) {
                        editor.save_tags_state();
                        entity->tags.clear();
                    }
                }

                for (size_t tag_index = 0; tag_index < entity->tags.size(); ++tag_index) {
                    ImGui::PushID(static_cast<int>(tag_index));
                    ImGui::Selectable(entity->tags[tag_index].c_str(), tag_index == 0);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("x")) {
                        editor.save_tags_state();
                        entity->tags.erase(entity->tags.begin() + static_cast<std::ptrdiff_t>(tag_index));
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }

                ImGui::Separator();
                if (ImGui::Selectable("Add Tag...")) {
                    open_add_tag_popup = true;
                }
                ImGui::EndCombo();
            }

            static char tag_buf[128] = {};
            if (open_add_tag_popup) {
                ImGui::OpenPopup("AddTagPopup");
            }
            if (ImGui::BeginPopup("AddTagPopup")) {
                ImGui::TextUnformatted("Add Tag");
                ImGui::InputText("##tag_name", tag_buf, IM_ARRAYSIZE(tag_buf));
                if (ImGui::Button("Add") && tag_buf[0] != '\0') {
                    const std::string new_tag(tag_buf);
                    if (std::find(entity->tags.begin(), entity->tags.end(), new_tag) == entity->tags.end()) {
                        editor.save_tags_state();
                        entity->tags.push_back(new_tag);
                    }
                    tag_buf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    tag_buf[0] = '\0';
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::Spacing();
            ComponentUIHelper::draw_entity_inspector(editor, *entity, shader);
        } else {
            draw_selected_texture_inspector(editor);
        }

        ImGui::End();
    }

    if (show_scene) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin(lang.word("scene"), &show_scene)) {
            editor.plugin_manager->draw_ui_region(UI_INSPECTOR, *plugin_ctx);
            g_scene_window_pos = ImGui::GetCursorScreenPos();
            g_scene_window_size = ImGui::GetContentRegionAvail();

            if (g_scene_window_size.x > 0 && g_scene_window_size.y > 0) {
                if (scene_rt.id == 0 ||
                    scene_rt.texture.width  != (int)g_scene_window_size.x ||
                    scene_rt.texture.height != (int)g_scene_window_size.y) {
                    if (scene_rt.id != 0) {
                        UnloadRenderTexture(scene_rt);
                    }
                    scene_rt = LoadRenderTexture((int)g_scene_window_size.x, (int)g_scene_window_size.y);
                }

                if (scene_rt.id > 0) {
                    const bool top_left_texture_backend =
                        GetCurrentBackend() == RendererType::Vulkan ||
                        GetCurrentBackend() == RendererType::D3D11;
                    qcImGuiAddImage(
                        ImGui::GetWindowDrawList(),
                        &scene_rt.texture,
                        g_scene_window_pos,
                        ImVec2(g_scene_window_pos.x + g_scene_window_size.x,
                            g_scene_window_pos.y + g_scene_window_size.y),
                        top_left_texture_backend ? ImVec2(0, 0) : ImVec2(0, 1),
                        top_left_texture_backend ? ImVec2(1, 1) : ImVec2(1, 0)
                    );
                }

                g_is_scene_hovered = ImGui::IsWindowHovered();
                g_is_scene_active  = ImGui::IsWindowFocused();

                draw_gizmo(editor, camera);
                draw_polygon_editor(editor, camera.get_camera());
                handle_scene_asset_drop(editor, camera.get_camera());
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (renaming_index != -1) ImGui::OpenPopup(lang.word("rename"));

    if (ImGui::BeginPopupModal(lang.word("rename"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##rename", rename_buf, IM_ARRAYSIZE(rename_buf));
        if (ImGui::Button(lang.word("ok"))) {
            if (renaming_index >= 0 && renaming_index < static_cast<int>(editor.scene.entities.size())) {
                assign_entity_name(editor.scene.entities[renaming_index], rename_buf);
            }
            renaming_index = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(lang.word("cancel"))) {
            renaming_index = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (show_assets) draw_assets_ui(editor);
    draw_model_viewer_window();
    draw_material_viewer_window(editor, editor.scene.get_selected());

    if (show_about_window) {
        ImGui::OpenPopup(lang.word("about_quark_engine"));
        show_about_window = false;
    }

    if (ImGui::BeginPopupModal(lang.word("about_quark_engine"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto backend = qc::GetCurrentBackend();

        ImGui::Text("Quark Engine %s using %s",
            QUARK_ENGINE_VERSION,
            backend == qc::RendererType::Vulkan ? "Vulkan" :
            backend == qc::RendererType::OpenGL ? "OpenGL" :
            backend == qc::RendererType::Auto ? "Auto" :
            "Unknown"
        );
        ImGui::Separator();
        ImGui::Text(lang.word("quarkcore_version"), QC_VERSION_STRING, "stable");
        ImGui::Text(lang.word("imgui_version"), IMGUI_VERSION);
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), lang.word("api_docs"));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(lang.word("open_docs"));
            if (ImGui::IsMouseClicked(0)) {
                open_url("https://quark-engine.gitbook.io/quark-engine-docs");
            }
        }

        ImGui::Spacing();
        if (ImGui::Button(lang.word("close"), ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (show_preferences) {
        ImGui::Begin(lang.word("preferences"), &show_preferences);
        ImGui::Text(lang.word("preferences"));
        ImGui::Separator();

        bool preferences_changed = false;
        if (ImGui::BeginTabBar("PreferencesTabs")) {
            if (ImGui::BeginTabItem("General")) {
                static int language_index = -1;
                if (language_index == -1)
                    language_index = find_index(LanguageManager::get().current.c_str());

                ImGui::Text(lang.word("language"));
                if (ImGui::Combo("##language_combo", &language_index, language_labels, IM_ARRAYSIZE(language_labels))) {
                    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
                    lang.set_lang(language_codes[language_index]);
                    ImGui::LoadIniSettingsFromDisk(ImGui::GetIO().IniFilename);
                    preferences_changed = true;
                }
                preferences_changed |= ImGui::Checkbox("Wireframe", &g_wireframe_enabled);
                preferences_changed |= ImGui::Checkbox("Show collision shapes", &g_editor_preferences.show_colliders);
                preferences_changed |= ImGui::Checkbox("Confirm delete", &g_editor_preferences.confirm_delete);
                preferences_changed |= ImGui::Checkbox("Show bounding boxes", &g_editor_preferences.show_bounding_boxes);
                preferences_changed |= ImGui::Checkbox("Focus camera on selection", &g_editor_preferences.focus_on_selection);
                preferences_changed |= ImGui::Checkbox("Confirm exit with unsaved changes", &g_editor_preferences.confirm_exit);
                preferences_changed |= ImGui::Checkbox("Open last project", &g_editor_preferences.open_last_project);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Rendering")) {
                preferences_changed |= ImGui::Checkbox("Show scene grid", &g_editor_preferences.show_grid);
                preferences_changed |= ImGui::Checkbox("Show coordinate axes", &g_editor_preferences.show_axes);
                preferences_changed |= ImGui::Checkbox("Limit frame rate", &g_editor_preferences.limit_fps);
                ImGui::BeginDisabled(!g_editor_preferences.limit_fps);
                preferences_changed |= ImGui::SliderInt("Target FPS", &g_editor_preferences.target_fps, 30, 240);
                ImGui::EndDisabled();
                preferences_changed |= ImGui::SliderFloat("Camera speed", &g_editor_preferences.camera_speed, 0.1f, 20.0f, "%.1f");
                preferences_changed |= ImGui::SliderFloat("Camera sensitivity", &g_editor_preferences.camera_sensitivity, 0.0005f, 0.02f, "%.4f");
                preferences_changed |= ImGui::SliderFloat("Zoom sensitivity", &g_editor_preferences.camera_zoom_sensitivity, 0.1f, 5.0f, "%.1f");
                preferences_changed |= ImGui::SliderFloat("Camera FOV", &g_editor_preferences.camera_fov, 20.0f, 120.0f, "%.0f deg");
                preferences_changed |= ImGui::SliderFloat("Shadow bias", &g_editor_preferences.shadow_bias, 0.0001f, 0.05f, "%.4f");
                const char* shadow_filter_names[] = { "Hard", "9 samples", "25 samples" };
                preferences_changed |= ImGui::Combo("Shadow filtering", &g_editor_preferences.shadow_filter_quality, shadow_filter_names, 3);
                preferences_changed |= ImGui::SliderInt("Undo history limit", &g_editor_preferences.undo_history_limit, 10, 500);
                camera.cam.fovy = g_editor_preferences.camera_fov;
                const char* backend_names[] = { "Auto", "OpenGL", "Vulkan", "Direct3D 11" };
                ImGui::Text("Renderer backend (restart required)");
                preferences_changed |= ImGui::Combo("##renderer_backend", &g_editor_preferences.renderer_backend, backend_names, 4);
                const char* msaa_names[] = { "Off", "2x", "4x", "8x" };
                int msaa_index = g_editor_preferences.msaa_samples == 2 ? 1 : g_editor_preferences.msaa_samples == 4 ? 2 : g_editor_preferences.msaa_samples == 8 ? 3 : 0;
                ImGui::Text("MSAA (restart required)");
                if (ImGui::Combo("##msaa", &msaa_index, msaa_names, 4)) {
                    g_editor_preferences.msaa_samples = msaa_index == 1 ? 2 : msaa_index == 2 ? 4 : msaa_index == 3 ? 8 : 1;
                    preferences_changed = true;
                }
                const char* filter_names[] = { "Nearest", "Linear" };
                ImGui::Text("Texture filtering (restart required)");
                preferences_changed |= ImGui::Combo("##texture_filter", &g_editor_preferences.texture_filter, filter_names, 2);
                float background_color[3] = {
                    g_editor_preferences.background_red / 255.0f,
                    g_editor_preferences.background_green / 255.0f,
                    g_editor_preferences.background_blue / 255.0f
                };
                if (ImGui::ColorEdit3("Scene background", background_color)) {
                    g_editor_preferences.background_red = static_cast<int>(std::round(background_color[0] * 255.0f));
                    g_editor_preferences.background_green = static_cast<int>(std::round(background_color[1] * 255.0f));
                    g_editor_preferences.background_blue = static_cast<int>(std::round(background_color[2] * 255.0f));
                    preferences_changed = true;
                }
                preferences_changed |= ImGui::Checkbox("Enable autosave", &g_editor_preferences.autosave_enabled);
                preferences_changed |= ImGui::Checkbox("Create autosave backup", &g_editor_preferences.autosave_backup_enabled);
                ImGui::BeginDisabled(!g_editor_preferences.autosave_enabled);
                preferences_changed |= ImGui::SliderInt("Autosave interval (minutes)", &g_editor_preferences.autosave_interval_minutes, 1, 60);
                ImGui::EndDisabled();
                ImGui::Separator();
                preferences_changed |= ImGui::Checkbox("Enable Gizmo snapping", &g_editor_preferences.gizmo_snap_enabled);
                ImGui::BeginDisabled(!g_editor_preferences.gizmo_snap_enabled);
                preferences_changed |= ImGui::SliderFloat("Translation snap", &g_editor_preferences.gizmo_translation_snap, 0.01f, 10.0f, "%.2f");
                preferences_changed |= ImGui::SliderFloat("Rotation snap", &g_editor_preferences.gizmo_rotation_snap, 1.0f, 90.0f, "%.1f deg");
                preferences_changed |= ImGui::SliderFloat("Scale snap", &g_editor_preferences.gizmo_scale_snap, 0.01f, 1.0f, "%.2f");
                ImGui::EndDisabled();
                preferences_changed |= ImGui::Checkbox("Enable shadows", &g_editor_preferences.shadows_enabled);
                const char* shadow_sizes[] = { "512", "1024", "2048" };
                int shadow_size_index = g_editor_preferences.shadow_map_size == 512 ? 0 :
                    g_editor_preferences.shadow_map_size == 2048 ? 2 : 1;
                if (ImGui::Combo("Shadow map size (restart required)", &shadow_size_index, shadow_sizes, 3)) {
                    g_editor_preferences.shadow_map_size = shadow_size_index == 0 ? 512 :
                        shadow_size_index == 2 ? 2048 : 1024;
                    preferences_changed = true;
                }
                if (preferences_changed)
                    SetTargetFPS(g_editor_preferences.limit_fps ? g_editor_preferences.target_fps : 0);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Interface")) {
                static ImGuiStyle interface_base_style;
                static float interface_base_scale = 1.0f;
                static bool interface_base_initialized = false;
                if (!interface_base_initialized) {
                    interface_base_style = ImGui::GetStyle();
                    interface_base_scale = g_editor_preferences.interface_scale;
                    interface_base_initialized = true;
                }
                preferences_changed |= ImGui::Checkbox("Hierarchy", &show_hierarchy);
                preferences_changed |= ImGui::Checkbox("Inspector", &show_inspector);
                preferences_changed |= ImGui::Checkbox("Assets", &show_assets);
                preferences_changed |= ImGui::Checkbox("Scene", &show_scene);
                preferences_changed |= ImGui::Checkbox("VSync (restart required)", &g_editor_preferences.vsync_enabled);
                const bool light_theme_changed = ImGui::Checkbox("Light theme", &g_editor_preferences.light_theme);
                preferences_changed |= light_theme_changed;
                if (light_theme_changed) {
                    ApplyCustomImGuiTheme();
                    ImGui::GetStyle().ScaleAllSizes(g_editor_preferences.interface_scale);
                    ImGui::GetStyle().FontScaleMain = g_editor_preferences.interface_scale;
                    interface_base_style = ImGui::GetStyle();
                    interface_base_scale = g_editor_preferences.interface_scale;
                }
                preferences_changed |= ImGui::Checkbox("Show light helpers", &g_editor_preferences.show_light_helpers);
                preferences_changed |= ImGui::Checkbox("Show camera frustum", &g_editor_preferences.show_cameras);
                preferences_changed |= ImGui::Checkbox("Show selection visualization", &g_editor_preferences.show_selection_visualization);
                preferences_changed |= ImGui::SliderInt("Asset preview size", &g_editor_preferences.asset_preview_size, 32, 128);
                const char* asset_filter_names[] = { "All", "Images", "Models", "Materials" };
                ImGui::Text("Asset type filter");
                preferences_changed |= ImGui::Combo("##asset_type_filter", &g_editor_preferences.asset_filter, asset_filter_names, 4);
                float selection_color[3] = { g_editor_preferences.selection_red / 255.0f, g_editor_preferences.selection_green / 255.0f, g_editor_preferences.selection_blue / 255.0f };
                if (ImGui::ColorEdit3("Selection color", selection_color)) {
                    g_editor_preferences.selection_red = static_cast<int>(selection_color[0] * 255.0f);
                    g_editor_preferences.selection_green = static_cast<int>(selection_color[1] * 255.0f);
                    g_editor_preferences.selection_blue = static_cast<int>(selection_color[2] * 255.0f);
                    preferences_changed = true;
                }
                float wireframe_color[3] = { g_editor_preferences.wireframe_red / 255.0f, g_editor_preferences.wireframe_green / 255.0f, g_editor_preferences.wireframe_blue / 255.0f };
                if (ImGui::ColorEdit3("Wireframe color", wireframe_color)) {
                    g_editor_preferences.wireframe_red = static_cast<int>(wireframe_color[0] * 255.0f);
                    g_editor_preferences.wireframe_green = static_cast<int>(wireframe_color[1] * 255.0f);
                    g_editor_preferences.wireframe_blue = static_cast<int>(wireframe_color[2] * 255.0f);
                    preferences_changed = true;
                }
                float bounds_color[3] = { g_editor_preferences.bounds_red / 255.0f, g_editor_preferences.bounds_green / 255.0f, g_editor_preferences.bounds_blue / 255.0f };
                if (ImGui::ColorEdit3("Bounding box color", bounds_color)) {
                    g_editor_preferences.bounds_red = static_cast<int>(bounds_color[0] * 255.0f);
                    g_editor_preferences.bounds_green = static_cast<int>(bounds_color[1] * 255.0f);
                    g_editor_preferences.bounds_blue = static_cast<int>(bounds_color[2] * 255.0f);
                    preferences_changed = true;
                }
                if (ImGui::SliderFloat("Interface scale", &g_editor_preferences.interface_scale, 0.75f, 2.0f, "%.2fx")) {
                    ImGui::GetStyle() = interface_base_style;
                    ImGui::GetStyle().ScaleAllSizes(g_editor_preferences.interface_scale / interface_base_scale);
                    if (ImGui::GetStyle().SeparatorSize <= 0.0f)
                        ImGui::GetStyle().SeparatorSize = 1.0f;
                    if (ImGui::GetStyle().WindowBorderHoverPadding <= 0.0f)
                        ImGui::GetStyle().WindowBorderHoverPadding = 1.0f;
                    if (ImGui::GetStyle().WindowMinSize.x < 1.0f)
                        ImGui::GetStyle().WindowMinSize.x = 1.0f;
                    if (ImGui::GetStyle().WindowMinSize.y < 1.0f)
                        ImGui::GetStyle().WindowMinSize.y = 1.0f;
                    ImGui::GetStyle().FontScaleMain = g_editor_preferences.interface_scale;
                    preferences_changed = true;
                }
                if (preferences_changed) {
                    g_editor_preferences.show_hierarchy = show_hierarchy;
                    g_editor_preferences.show_inspector = show_inspector;
                    g_editor_preferences.show_assets = show_assets;
                    g_editor_preferences.show_scene = show_scene;
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        if (preferences_changed) {
            g_editor_preferences.wireframe_enabled = g_wireframe_enabled;
            save_editor_preferences();
        }

        ImGui::End();
    }

    if (pending_delete_editor && pending_delete_entity) {
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete selected entity?");
            if (ImGui::Button("Delete")) {
                Editor* editor_to_delete = pending_delete_editor;
                Entity* entity_to_delete = pending_delete_entity;
                pending_delete_editor = nullptr;
                pending_delete_entity = nullptr;
                const bool previous_confirm = g_editor_preferences.confirm_delete;
                g_editor_preferences.confirm_delete = false;
                delete_entity(*editor_to_delete, entity_to_delete);
                g_editor_preferences.confirm_delete = previous_confirm;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                pending_delete_editor = nullptr;
                pending_delete_entity = nullptr;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (show_exit_confirmation) {
        ImGui::OpenPopup("Unsaved Changes");
        show_exit_confirmation = false;
    }
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("The scene has unsaved changes.");
        if (ImGui::Button("Save and Exit")) {
            project_save(editor.project_path, editor.scene);
            editor.scene_dirty = false;
            CloseWindow();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Exit Without Saving")) {
            CloseWindow();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void Editor::draw_ui(Shader shader, FlyCamera& camera, PluginContext* ctx) {
    ::draw_ui(*this, shader, camera, ctx);
}
