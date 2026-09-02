#include "application.h"
#include "qcImGui.h"
#include "imgui.h"
#include "plugins/plugin_manager.h"
#include "lighting.h"
#include "language_manager.h"
#include "editor/editor_preferences.h"
#include "text_mesh.h"
#include "editor/editor_ui.h"
#include "editor/editor_entity.h"
#include "project.h"
#include "hub.h"
#include "tex.h"
#include <cfloat>
#include <iostream>
#include <SDL3/SDL_video.h>

using namespace qc;

namespace fs = std::filesystem;

PluginManager* g_plugin_manager = nullptr;
PluginContext* ctx = nullptr;

extern bool g_is_scene_hovered;
extern bool g_is_scene_active;
extern RenderTexture2D scene_rt;
extern ImVec2 g_scene_window_pos;
extern ImVec2 g_scene_window_size;

static bool language_uses_ms_pgothic(const std::string& language_code) {
    return language_code == "japanese" ||
        language_code == "korean" ||
        language_code == "simplified_chinese" ||
        language_code == "traditional_chinese";
}

static const ImWchar* get_ms_pgothic_glyph_ranges(ImGuiIO& io, const std::string& language_code) {
    if (language_code == "japanese") return io.Fonts->GetGlyphRangesJapanese();
    if (language_code == "korean") return io.Fonts->GetGlyphRangesKorean();
    if (language_code == "simplified_chinese") return io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    if (language_code == "traditional_chinese") return io.Fonts->GetGlyphRangesChineseFull();
    return nullptr;
}

static void reload_editor_fonts(const std::string& language_code) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const std::string base_font_path = LanguageManager::get().editor_font_path();
    const std::string merge_font_path = LanguageManager::get().editor_font_merge_path();
    ImFont* default_font = io.Fonts->AddFontFromFileTTF(base_font_path.c_str(), 16.0f);

    if (!merge_font_path.empty()) {
        ImFontConfig merge_config = {};
        merge_config.MergeMode = true;
        merge_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(
            merge_font_path.c_str(),
            16.0f,
            &merge_config,
            nullptr
        );
    } else if (language_uses_ms_pgothic(language_code) && base_font_path != "assets/MS-Pgothic-Regular.ttf") {
        ImFontConfig merge_config = {};
        merge_config.MergeMode = true;
        merge_config.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF(
            "assets/MS-Pgothic-Regular.ttf",
            16.0f,
            &merge_config,
            get_ms_pgothic_glyph_ranges(io, language_code)
        );
    }

    io.FontDefault = default_font;
    io.Fonts->Build();
}

static void init_plugin_context(PluginContext* ctx) {
    ctx->ui_begin        = [](const char* t)             { return ImGui::Begin(t); };
    ctx->ui_end          = []()                           { ImGui::End(); };
    ctx->ui_begin_menu   = [](const char* l)             { return ImGui::BeginMenu(l); };
    ctx->ui_end_menu     = []()                           { ImGui::EndMenu(); };
    ctx->ui_menu_item    = [](const char* l)             { return ImGui::MenuItem(l); };
    ctx->ui_text         = [](const char* t)             { ImGui::Text("%s", t); };
    ctx->ui_button       = [](const char* l)             { return ImGui::Button(l); };
    ctx->ui_checkbox     = [](const char* l, bool* v)    { return ImGui::Checkbox(l, v); };
    ctx->ui_slider_float = [](const char* l, float* v, float mn, float mx) { return ImGui::SliderFloat(l, v, mn, mx); };
    ctx->ui_input_float  = [](const char* l, float* v)   { return ImGui::InputFloat(l, v); };
    ctx->ui_color_edit3  = [](const char* l, float c[3]) { return ImGui::ColorEdit3(l, c); };
    ctx->ui_separator    = []()                           { ImGui::Separator(); };
    ctx->ui_same_line    = []()                           { ImGui::SameLine(); };

    ctx->register_ui_callback = [](UIRegion region, PluginUICallback callback) {
        if (g_plugin_manager) g_plugin_manager->register_ui_callback(region, callback);
    };
}

static void update_plugins(PluginManager& plugin_manager, Editor& editor) {
    static Editor* s_editor = nullptr;
    s_editor = &editor;

    ctx->scene           = &s_editor->scene;

    ctx->delta_time      = GetFrameTime();
    ctx->entity_count    = (int)s_editor->scene.entities.size();
    ctx->selected        = &s_editor->scene.selected;

    ctx->ui_begin            = [](const char* t) { return ImGui::Begin(t); };
    ctx->ui_end              = []() { ImGui::End(); };
    ctx->ui_begin_menu       = [](const char* label) { return ImGui::BeginMenu(label); };
    ctx->ui_end_menu         = []() { ImGui::EndMenu(); };
    ctx->ui_menu_item        = [](const char* label) { return ImGui::MenuItem(label); };
    ctx->ui_text             = [](const char* t) { ImGui::Text("%s", t); };
    ctx->ui_button           = [](const char* l) { return ImGui::Button(l); };
    ctx->ui_checkbox         = [](const char* l, bool* v) { return ImGui::Checkbox(l, v); };
    ctx->ui_slider_float     = [](const char* l, float* v, float mn, float mx) { return ImGui::SliderFloat(l, v, mn, mx); };
    ctx->ui_input_float      = [](const char* l, float* v) { return ImGui::InputFloat(l, v); };
    ctx->ui_color_edit3      = [](const char* l, float c[3]) { return ImGui::ColorEdit3(l, c); };
    ctx->ui_separator        = []() { ImGui::Separator(); };
    ctx->ui_same_line        = []() { ImGui::SameLine(); };

    ctx->entity_get_name     = [](int i) -> const char* { return s_editor->scene.entities[i].name.c_str(); };
    ctx->entity_get_position = [](int i, float* x, float* y, float* z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) { *x = t->position.x; *y = t->position.y; *z = t->position.z; } };
    ctx->entity_get_rotation = [](int i, float* x, float* y, float* z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) { *x = t->rotation.x; *y = t->rotation.y; *z = t->rotation.z; } };
    ctx->entity_get_scale    = [](int i, float* x, float* y, float* z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) { *x = t->scale.x; *y = t->scale.y; *z = t->scale.z; } };
    ctx->entity_get_color    = [](int i, unsigned char* r, unsigned char* g, unsigned char* b, unsigned char* a) { if (auto* m = s_editor->scene.entities[i].get_material_component()) { *r = m->color.r; *g = m->color.g; *b = m->color.b; *a = m->color.a; } };

    ctx->entity_set_position = [](int i, float x, float y, float z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) t->position = {x, y, z}; };
    ctx->entity_set_rotation = [](int i, float x, float y, float z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) t->rotation = {x, y, z}; };
    ctx->entity_set_scale    = [](int i, float x, float y, float z) { if (auto* t = s_editor->scene.entities[i].get_transform_component()) t->scale = {x, y, z}; };
    ctx->entity_set_color    = [](int i, unsigned char r, unsigned char g, unsigned char b, unsigned char a) { if (auto* m = s_editor->scene.entities[i].get_material_component()) m->color = {r, g, b, a}; };
    ctx->entity_set_name     = [](int i, const char* name) { s_editor->scene.entities[i].name = name; };

    ctx->scene_save = []() { project_save(s_editor->project_path, s_editor->scene); };
    ctx->scene_spawn = [](const char* asset_name) -> int {
        for (auto& a : assets) {
            if (a.name == asset_name) {
                Entity e = make_entity_from_asset(s_editor->scene, a);
                const MeshComponent* mesh = e.get_mesh_component();
                if (!mesh || !mesh->model.meshCount) return -1;
                s_editor->scene.entities.push_back(std::move(e));
                return (int)s_editor->scene.entities.size() - 1;
            }
        }
        return -1;
    };
    ctx->scene_delete = [](int i) {
        if (i < 0 || i >= (int)s_editor->scene.entities.size()) return;
        s_editor->scene.entities.erase(s_editor->scene.entities.begin() + i);
        if (s_editor->scene.selected >= (int)s_editor->scene.entities.size())
            s_editor->scene.selected = -1;
    };
    ctx->register_ui_callback = [](UIRegion region, PluginUICallback callback) {
        if (g_plugin_manager) {
            g_plugin_manager->register_ui_callback(region, callback);
        }
    };

    plugin_manager.update_all(*ctx);
    plugin_manager.draw_ui_all(*ctx);
}

static Mat4 compose_local_entity_transform(const Entity& entity) {
    const TransformComponent* transform = entity.get_transform_component();
    if (!transform)
        return Mat4::identity();

    Mat4 matScale = Mat4::scale(
        transform->scale.x,
        transform->scale.y,
        transform->scale.z
    );

    Mat4 matRotation =
        Mat4::rotationX(transform->rotation.x * DEG2RAD) *
        Mat4::rotationY(transform->rotation.y * DEG2RAD) *
        Mat4::rotationZ(transform->rotation.z * DEG2RAD);

    Mat4 matTranslation = Mat4::translation(
        transform->position.x,
        transform->position.y,
        transform->position.z
    );

    return matTranslation * matRotation * matScale;
}

static Mat4 compose_entity_transform(const Scene& scene, int entity_index, std::vector<int>& stack) {
    if (entity_index < 0 || entity_index >= static_cast<int>(scene.entities.size()))
        return Mat4::identity();
    if (std::find(stack.begin(), stack.end(), entity_index) != stack.end())
        return compose_local_entity_transform(scene.entities[entity_index]);

    stack.push_back(entity_index);
    const Entity& entity = scene.entities[entity_index];
    Mat4 world = compose_local_entity_transform(entity);
    if (entity.parent_id >= 0 && entity.parent_id < static_cast<int>(scene.entities.size()))
        world = compose_entity_transform(scene, entity.parent_id, stack) * world;
    stack.pop_back();
    return world;
}

static Mat4 compose_entity_transform(const Scene& scene, int entity_index) {
    std::vector<int> stack;
    return compose_entity_transform(scene, entity_index, stack);
}

static void expand_bounds_with_point(BoundingBox& bounds, const Vec3& point) {
    bounds.min.x = fminf(bounds.min.x, point.x);
    bounds.min.y = fminf(bounds.min.y, point.y);
    bounds.min.z = fminf(bounds.min.z, point.z);
    bounds.max.x = fmaxf(bounds.max.x, point.x);
    bounds.max.y = fmaxf(bounds.max.y, point.y);
    bounds.max.z = fmaxf(bounds.max.z, point.z);
}

static BoundingBox compute_scene_bounds(const Scene& scene) {
    BoundingBox bounds = {
        { FLT_MAX, FLT_MAX, FLT_MAX },
        { -FLT_MAX, -FLT_MAX, -FLT_MAX }
    };
    bool has_bounds = false;

    for (const auto& entity : scene.entities) {
        const MeshComponent* mesh = entity.get_mesh_component();
        if (!mesh || !mesh->enabled || mesh->model.meshCount <= 0 || !mesh->model.meshes) continue;

        Entity& mutable_entity = const_cast<Entity&>(entity);
        MeshComponent* mutable_mesh = mutable_entity.get_mesh_component();
        if (mutable_mesh->bounds_dirty) {
            mutable_mesh->cached_local_bounds = GetModelBoundingBox(mesh->model);
            mutable_mesh->bounds_dirty = false;
        }
        BoundingBox local_bounds = mutable_mesh->cached_local_bounds;
        const int entity_index = static_cast<int>(&entity - scene.entities.data());
        Mat4 transform = compose_entity_transform(scene, entity_index);

        const Vec3 corners[8] = {
            { local_bounds.min.x, local_bounds.min.y, local_bounds.min.z },
            { local_bounds.max.x, local_bounds.min.y, local_bounds.min.z },
            { local_bounds.min.x, local_bounds.max.y, local_bounds.min.z },
            { local_bounds.max.x, local_bounds.max.y, local_bounds.min.z },
            { local_bounds.min.x, local_bounds.min.y, local_bounds.max.z },
            { local_bounds.max.x, local_bounds.min.y, local_bounds.max.z },
            { local_bounds.min.x, local_bounds.max.y, local_bounds.max.z },
            { local_bounds.max.x, local_bounds.max.y, local_bounds.max.z }
        };

        for (const Vec3& corner : corners) {
            expand_bounds_with_point(bounds, Vec3(transform * corner));
        }
        has_bounds = true;
    }

    if (!has_bounds) {
        bounds.min = { -5.0f, -5.0f, -5.0f };
        bounds.max = { 5.0f, 5.0f, 5.0f };
    }

    return bounds;
}

static void set_model_shader(Model& model, Shader shader) {
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].shader = &shader;
    }
}

static void set_shader_light_enabled(Shader shader, int slot, bool enabled) {
    int enabled_loc = GetShaderLocation(shader, TextFormat("lights[%i].enabled", slot));
    int enabled_value = enabled ? 1 : 0;
    SetShaderValue(shader, enabled_loc, &enabled_value, SHADER_UNIFORM_INT);
}

static void disable_all_shader_lights(Shader shader) {
    for (int slot = 0; slot < QC_MAX_LIGHTS; slot++) {
        set_shader_light_enabled(shader, slot, false);
    }
}

static bool prepare_scene_light_uniforms(Scene& scene, Shader shader, const Vec3& scene_center) {
    disable_all_shader_lights(shader);

    bool has_active_scene_light = false;
    for (auto& e : scene.entities) {
        LightComponent* light = e.get_light_component();
        TransformComponent* transform = e.get_transform_component();
        if (!light || !transform || !light->enabled) continue;

        light->light.enabled = true;
        if (!light->created) {
            int new_id = allocate_light_id();
            if (new_id != -1) {
                light->light.id = new_id;
                light->light.light = create_light_at_slot(new_id, light->light.light.type,
                    transform->position, light->light.target, light->light.color, shader);
                initialize_lighting_uniform_cache(light->light, shader, new_id);
                light->created = true;
            }
        }

        if (!light->created || light->light.id == -1) continue;

        light->light.position = transform->position;

        if (light->light.light.type == LIGHT_DIRECTIONAL &&
            (light->light.position - light->light.target).length() <= 0.000001f) {
            light->light.target = scene_center;
        }

        light->light.light.position = light->light.position;
        light->light.light.target = light->light.target;
        light->light.light.color = light->light.color;
        update_lighting(shader, light->light);
        has_active_scene_light = true;
    }

    return has_active_scene_light;
}

static void render_scene_shadow_maps(Scene& scene, Shader shadow_shader,
    std::array<RenderTexture2D, QC_MAX_LIGHTS>& shadow_maps,
    std::array<Camera3D, QC_MAX_LIGHTS>& shadow_cameras,
    const Vec3& scene_center,
    Shader lighting_shader,
    std::array<int, QC_MAX_LIGHTS>& light_view_locations,
    std::array<int, QC_MAX_LIGHTS>& light_projection_locations) {
    std::array<bool, QC_MAX_LIGHTS> rendered = {};
    Material shadow_material = {};
    shadow_material.shader = &shadow_shader;

    for (auto& entity : scene.entities) {
        LightComponent* light = entity.get_light_component();
        TransformComponent* transform = entity.get_transform_component();
        if (!light || !transform || !light->enabled || !light->created ||
            light->light.id < 0 || light->light.id >= QC_MAX_LIGHTS ||
            rendered[light->light.id]) continue;

        const int slot = light->light.id;
        shadow_cameras[slot].position = transform->position;
        shadow_cameras[slot].target = light->light.target;
        if ((shadow_cameras[slot].position - shadow_cameras[slot].target).length() <= 0.000001f) {
            shadow_cameras[slot].target = scene_center;
            if ((shadow_cameras[slot].position - shadow_cameras[slot].target).length() <= 0.000001f)
                shadow_cameras[slot].target = shadow_cameras[slot].position + Vec3{0.0f, -1.0f, 0.0f};
        }
        rendered[slot] = true;

        BeginTextureMode(shadow_maps[slot]);
        ClearBackground(WHITE);
        BeginMode3D(shadow_cameras[slot]);
        BeginShaderMode(shadow_shader);

        Mat4 light_view;
        Mat4 light_projection;
        for (int source_index = 0; source_index < static_cast<int>(scene.entities.size()); ++source_index) {
            auto& source = scene.entities[source_index];
            MeshComponent* mesh = source.get_mesh_component();
            TransformComponent* source_transform = source.get_transform_component();
            if (!mesh || !mesh->enabled || !source_transform ||
                mesh->model.meshCount <= 0 || !mesh->model.meshes) continue;

            const Mat4 entity_transform = compose_entity_transform(scene, source_index) * mesh->model.transform;
            for (int mesh_index = 0; mesh_index < mesh->model.meshCount; ++mesh_index) {
                DrawMesh(mesh->model.meshes[mesh_index], shadow_material, entity_transform);
            }
        }

        for (int i = 0; i < 16; ++i) {
            light_view.m[i] = GetMatrixModelview()[i];
            light_projection.m[i] = GetMatrixProjection()[i];
        }
        SetShaderValueMatrix(lighting_shader, light_view_locations[slot], light_view.m);
        SetShaderValueMatrix(lighting_shader, light_projection_locations[slot], light_projection.m);

        EndShaderMode();
        EndMode3D();
        EndTextureMode();
    }
}

static void assign_shadow_maps(Scene& scene,
    const std::array<RenderTexture2D, QC_MAX_LIGHTS>& shadow_maps,
    Shader lighting_shader) {
    for (auto& entity : scene.entities) {
        MeshComponent* mesh = entity.get_mesh_component();
        if (!mesh || !mesh->model.materials) continue;

        set_model_shader(mesh->model, lighting_shader);
        for (int material_index = 0; material_index < mesh->model.materialCount; ++material_index) {
            Material& material = mesh->model.materials[material_index];
            if (!material.maps) continue;
            for (int shadow_index = 0; shadow_index < QC_MAX_LIGHTS; ++shadow_index)
                material.maps[MATERIAL_MAP_HEIGHT + shadow_index].texture = shadow_maps[shadow_index].texture;
        }
        mesh->shader_assigned = true;
    }
}

void ApplyCustomImGuiTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();

    if (g_editor_preferences.light_theme) {
        ImGui::StyleColorsLight();
        style.WindowRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.FrameBorderSize = 1.0f;
        style.WindowBorderSize = 1.0f;
        style.FramePadding = ImVec2(6, 3);
        style.ItemSpacing = ImVec2(6, 4);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.07f, 0.09f, 0.12f, 1.0f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.30f, 0.33f, 0.38f, 1.0f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.93f, 0.94f, 0.96f, 1.0f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.97f, 0.98f, 0.99f, 1.0f);
        colors[ImGuiCol_PopupBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        colors[ImGuiCol_Border] = ImVec4(0.70f, 0.73f, 0.78f, 1.0f);
        colors[ImGuiCol_Separator] = ImVec4(0.78f, 0.80f, 0.84f, 1.0f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.86f, 0.88f, 0.91f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.79f, 0.84f, 0.91f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.70f, 0.79f, 0.92f, 1.0f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.84f, 0.86f, 0.89f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.78f, 0.82f, 0.88f, 1.0f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.88f, 0.90f, 0.93f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.82f, 0.85f, 0.90f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.79f, 0.91f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.71f, 0.87f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.84f, 0.88f, 0.94f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.73f, 0.82f, 0.94f, 1.0f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.62f, 0.75f, 0.92f, 1.0f);
        colors[ImGuiCol_Tab] = ImVec4(0.84f, 0.87f, 0.91f, 1.0f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.72f, 0.81f, 0.93f, 1.0f);
        colors[ImGuiCol_TabActive] = ImVec4(0.96f, 0.97f, 0.99f, 1.0f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.86f, 0.88f, 0.92f, 1.0f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.92f, 0.94f, 0.97f, 1.0f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.12f, 0.45f, 0.85f, 1.0f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.56f, 0.84f, 1.0f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.20f, 0.42f, 0.73f, 1.0f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.35f, 0.58f, 0.91f, 0.35f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.25f, 0.55f, 0.90f, 0.35f);
        return;
    }

    // ====== SHAPES ======
    style.WindowRounding = 0.0f;
    style.FrameRounding  = 0.0f;
    style.PopupRounding  = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;

    style.FramePadding = ImVec2(6, 3);
    style.ItemSpacing = ImVec2(6, 4);

    ImVec4* colors = style.Colors;

    // ====== GLOBAL ======
    colors[ImGuiCol_Text]           = ImVec4(0.80f, 0.82f, 0.85f, 1.00f); // #c9cdd1
    colors[ImGuiCol_TextDisabled]   = ImVec4(0.54f, 0.58f, 0.63f, 1.00f);
    colors[ImGuiCol_WindowBg]       = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #2a2c2f
    colors[ImGuiCol_ChildBg]        = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_PopupBg]        = ImVec4(0.20f, 0.21f, 0.23f, 1.00f); // #32353a
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.00f);

    // ====== BORDERS ======
    colors[ImGuiCol_Border]         = ImVec4(0.27f, 0.28f, 0.30f, 1.00f); // #44484d
    colors[ImGuiCol_Separator]      = ImVec4(0.24f, 0.25f, 0.27f, 1.00f);

    // ====== FRAMES (inputs, edits) ======
    colors[ImGuiCol_FrameBg]        = ImVec4(0.14f, 0.15f, 0.16f, 1.00f); // #24272a
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.23f, 0.25f, 0.27f, 1.00f); // #3B4045 hover
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff
    colors[ImGuiCol_InputTextCursor]= ImVec4(0.93f, 0.95f, 0.98f, 1.00f);

    // ====== TITLE / MENUBAR ======
    colors[ImGuiCol_TitleBg]        = ImVec4(0.19f, 0.20f, 0.22f, 1.00f); // #31343a
    colors[ImGuiCol_TitleBgActive]  = ImVec4(0.24f, 0.26f, 0.29f, 1.00f);
    colors[ImGuiCol_MenuBarBg]      = ImVec4(0.19f, 0.20f, 0.22f, 1.00f);

    // ====== BUTTONS ======
    colors[ImGuiCol_Button]         = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #51565c
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.36f, 0.38f, 0.41f, 1.00f); // #5C6169 hover
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // #3D4247 pressed

    // ====== HEADERS (Tree, Selectable) ======
    colors[ImGuiCol_Header]         = ImVec4(0.16f, 0.17f, 0.18f, 1.00f); // #292B2E
    colors[ImGuiCol_HeaderHovered]  = ImVec4(0.23f, 0.25f, 0.27f, 1.00f); // #3B4045
    colors[ImGuiCol_HeaderActive]   = ImVec4(0.18f, 0.53f, 0.78f, 1.00f); // #2e87c7ff selection

    // ====== SELECTION ======
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.18f, 0.47f, 0.78f, 0.35f); // #2e78c759

    // ====== SCROLLBAR ======
    colors[ImGuiCol_ScrollbarBg]    = ImVec4(0.18f, 0.20f, 0.22f, 1.00f); // #2f3337
    colors[ImGuiCol_ScrollbarGrab]  = ImVec4(0.33f, 0.36f, 0.38f, 1.00f); // #555b62
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.43f, 0.46f, 1.00f); // #666E75FF
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.53f, 0.56f, 1.00f); // #80878FFF

    // ====== TABS ======
    colors[ImGuiCol_Tab]            = ImVec4(0.20f, 0.21f, 0.23f, 1.00f); // #333538FF
    colors[ImGuiCol_TabHovered]     = ImVec4(0.36f, 0.39f, 0.43f, 1.00f);
    colors[ImGuiCol_TabActive]      = ImVec4(0.27f, 0.30f, 0.34f, 1.00f);
    colors[ImGuiCol_TabUnfocused]   = ImVec4(0.17f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.24f, 0.27f, 1.00f);

    // ====== CHECKBOX ======
    colors[ImGuiCol_CheckMark]      = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff

    // ====== RESIZE GRIP ======
    colors[ImGuiCol_ResizeGrip]         = ImVec4(0.30f, 0.32f, 0.35f, 1.00f); // #4D5259FF
    colors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.40f, 0.43f, 0.46f, 1.00f); // #666E75FF
    colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.0f, 0.6f, 1.0f, 1.0f); // #0099ffff

    // ====== DOCKING ======
    colors[ImGuiCol_DockingPreview] = ImVec4(0.78f, 0.52f, 0.17f, 0.4f); // #c9802b66
    colors[ImGuiCol_NavCursor]      = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_TextLink]       = ImVec4(0.40f, 0.72f, 0.98f, 1.00f);
}

Application::Application(const CommandLineOptions& options)
    : options(options) {
}

static void apply_log_level(const std::string& level) {
    if (level.empty()) return;
    std::cout << "[log-level] requested '" << level << "\n";
}

void Application::initialize() {
    if (options.help_requested || options.version_requested)
        return;

    fs::create_directories("projects");
    fs::create_directories("assets");

    if (options.test_mode)
        return;

    apply_log_level(options.log_level);

    std::string lang_code = load_or_create_config();
    if (!options.lang_override.empty())
        lang_code = options.lang_override;
    load_editor_preferences();
    LanguageManager::get().set_lang(lang_code);
    g_wireframe_enabled = g_editor_preferences.wireframe_enabled;
    show_hierarchy = g_editor_preferences.show_hierarchy;
    show_inspector = g_editor_preferences.show_inspector;
    show_assets = g_editor_preferences.show_assets;
    show_scene = g_editor_preferences.show_scene;

    RendererType renderer_type = RendererType::OpenGL;
    if (g_editor_preferences.renderer_backend == 1) renderer_type = RendererType::OpenGL;
    else if (g_editor_preferences.renderer_backend == 2) renderer_type = RendererType::Vulkan;
    else if (g_editor_preferences.renderer_backend == 3) renderer_type = RendererType::D3D11;
    if (options.renderer_override == RendererOverride::OpenGL) renderer_type = RendererType::OpenGL;
    else if (options.renderer_override == RendererOverride::Vulkan) renderer_type = RendererType::Vulkan;

    SetMSAASamples(g_editor_preferences.msaa_samples);
    SetTextureFilterMode(g_editor_preferences.texture_filter == 0
        ? TextureFilterMode::Nearest : TextureFilterMode::Linear);

    InitWindow(1280, 720, "Quark Engine", renderer_type);

    if (options.fps_override >= 0) {
        SetTargetFPS(options.fps_override);
    } else {
        if (g_editor_preferences.target_fps <= 0)
            g_editor_preferences.target_fps = static_cast<int>(GetCurrentMonitorRefreshRate());
        SetTargetFPS(g_editor_preferences.limit_fps ? g_editor_preferences.target_fps : 0);
    }

    const bool vsync_enabled = options.vsync_override == TriState::On ? true
        : options.vsync_override == TriState::Off ? false
        : g_editor_preferences.vsync_enabled;
    if (!vsync_enabled)
        SDL_GL_SetSwapInterval(0);

    SetExitKey(KEY_NULL);

    headless = options.headless;

    if (!headless) {
        init_freetype();
        qcImGuiSetup(false);
        reload_editor_fonts(LanguageManager::get().current);
        ApplyCustomImGuiTheme();
        ImGui::GetStyle().ScaleAllSizes(g_editor_preferences.interface_scale);
        ImGui::GetStyle().FontScaleMain = g_editor_preferences.interface_scale;
        if (ImGui::GetStyle().WindowBorderHoverPadding <= 0.0f)
            ImGui::GetStyle().WindowBorderHoverPadding = 1.0f;
        if (ImGui::GetStyle().SeparatorSize <= 0.0f)
            ImGui::GetStyle().SeparatorSize = 1.0f;
    }

    project_path = options.project_path;

    if (project_path.empty() && !headless) {
        if (g_editor_preferences.open_last_project &&
            !g_editor_preferences.last_project_path.empty() &&
            project_is_valid(g_editor_preferences.last_project_path)) {
            project_path = g_editor_preferences.last_project_path;
        } else {
            project_path = run_hub();
        }
        if (project_path.empty()) {
            qcImGuiShutdown();
            shutdown_freetype();
            CloseWindow();
            return;
        }
    }

    if (project_path.empty())
        project_path = (fs::path("projects") / "default").string();

    project_path = project_resolve_root(project_path);
    g_editor_preferences.last_project_path = project_path;
    save_editor_preferences();

    fs::create_directories(fs::path(project_path) / "resources");

    editor.project_path = project_path;
    camera.speed = g_editor_preferences.camera_speed;
    camera.zoom_sensitivity = g_editor_preferences.camera_zoom_sensitivity;
    camera.cam.fovy = g_editor_preferences.camera_fov;

    const bool vulkan_backend = GetCurrentBackend() == RendererType::Vulkan;
    lighting_shader = LoadShader(vulkan_backend ? "assets/vulkan_lighting.vs" : "assets/lighting.vs",
        vulkan_backend ? "assets/vulkan_lighting.fs" : "assets/lighting.fs");
    shadow_shader = LoadShader(vulkan_backend ? "assets/vulkan_shadow_depth.vs" : "assets/shadow_depth.vs",
        vulkan_backend ? "assets/vulkan_shadow_depth.fs" : "assets/shadow_depth.fs");
    lighting_shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lighting_shader, "viewPos");
    shadows_enabled_loc = GetShaderLocation(lighting_shader, "shadowsEnabled");
    shadow_bias_loc = GetShaderLocation(lighting_shader, "shadowBias");
    shadow_filter_loc = GetShaderLocation(lighting_shader, "shadowFilterQuality");
    const int shadows_enabled = g_editor_preferences.shadows_enabled ? 1 : 0;
    SetShaderValue(lighting_shader, shadows_enabled_loc, &shadows_enabled, SHADER_UNIFORM_INT);
    SetShaderValue(lighting_shader, shadow_bias_loc, g_editor_preferences.shadow_bias);
    SetShaderValue(lighting_shader, shadow_filter_loc, g_editor_preferences.shadow_filter_quality);

    for (int i = 0; i < QC_MAX_LIGHTS; ++i) {
        shadow_maps[i] = LoadRenderTexture(g_editor_preferences.shadow_map_size, g_editor_preferences.shadow_map_size);
        shadow_cameras[i] = CreateCamera3D();
        shadow_cameras[i].fovy = 55.0f;
        light_view_locations[i] = GetShaderLocation(lighting_shader, TextFormat("lightViews[%i]", i));
        light_projection_locations[i] = GetShaderLocation(lighting_shader, TextFormat("lightProjections[%i]", i));
    }

    use_tex_loc = GetShaderLocation(lighting_shader, "useTexture");
    ambient_loc = GetShaderLocation(lighting_shader, "ambient");
    emission_color_loc = GetShaderLocation(lighting_shader, "emissionColor");
    emission_power_loc = GetShaderLocation(lighting_shader, "emissionPower");
    SetShaderValue(lighting_shader, ambient_loc, Vec4{0.025f, 0.025f, 0.025f, 1.0f});
    SetShaderValue(lighting_shader, emission_color_loc, Vec3{0.0f, 0.0f, 0.0f});
    SetShaderValue(lighting_shader, emission_power_loc, 0.0f);

    g_plugin_manager = new PluginManager();
    ctx = new PluginContext{};
    init_plugin_context(ctx);

    load_models();
    load_textures(project_path);
    refresh_assets(project_path);
    refresh_models(project_path, editor.scene);

    if (project_is_valid(project_path) && !options.new_project)
        project_load(project_path, editor.scene, lighting_shader);
    else
        project_new(project_path, editor.scene);

    if (headless) {
        editor.scene.release_resources();
        unload_models();
        unload_textures();
        UnloadShader(lighting_shader);
        UnloadShader(shadow_shader);
        for (auto& shadow_map : shadow_maps)
            UnloadRenderTexture(shadow_map);
        CloseWindow();
        return;
    }

    if (!options.no_plugins)
        g_plugin_manager->load_all(options.plugins_dir.c_str(), ctx);
    editor.plugin_manager = g_plugin_manager;

    active_font_language = LanguageManager::get().current;
    last_autosave_time = GetTime();
    last_selected_entity = editor.scene.selected;

    ready_to_run = true;
}

void Application::update_frame() {
    if (active_font_language != LanguageManager::get().current) {
        active_font_language = LanguageManager::get().current;
        reload_editor_fonts(active_font_language);
    }

    camera.speed = g_editor_preferences.camera_speed;
    camera.sensitivity = g_editor_preferences.camera_sensitivity;
    camera.zoom_sensitivity = g_editor_preferences.camera_zoom_sensitivity;
    if (g_editor_preferences.focus_on_selection && editor.scene.selected != last_selected_entity) {
        Entity* selected_entity = editor.scene.get_selected();
        TransformComponent* selected_transform = selected_entity ? selected_entity->get_transform_component() : nullptr;
        if (selected_transform) {
            const int selected_index = editor.scene.selected;
            const Vec3 world_position = Vec3Transform(
                {0.0f, 0.0f, 0.0f},
                compose_entity_transform(editor.scene, selected_index)
            );
            camera.focus_on(world_position);
        }
    }
    last_selected_entity = editor.scene.selected;

    if (!options.no_autosave && g_editor_preferences.autosave_enabled &&
        GetTime() - last_autosave_time >= g_editor_preferences.autosave_interval_minutes * 60.0) {
        if (g_editor_preferences.autosave_backup_enabled) {
            std::error_code backup_error;
            fs::copy_file(
                fs::path(editor.project_path) / "scene.json",
                fs::path(editor.project_path) / "scene.json.bak",
                fs::copy_options::overwrite_existing,
                backup_error
            );
        }
        project_save(editor.project_path, editor.scene);
        last_autosave_time = GetTime();
    }

    SetWindowTitle(TextFormat("Quark Engine | %s | FPS: %d",
        fs::path(project_path).filename().string().c_str(), GetFPS()));

    BoundingBox scene_bounds = compute_scene_bounds(editor.scene);
    Vec3 scene_center = {
        (scene_bounds.min.x + scene_bounds.max.x) * 0.5f,
        (scene_bounds.min.y + scene_bounds.max.y) * 0.5f,
        (scene_bounds.min.z + scene_bounds.max.z) * 0.5f
    };
    Vec3 cam_pos = camera.get_camera().position;
    SetShaderValue(lighting_shader, lighting_shader.locs[SHADER_LOC_VECTOR_VIEW], &cam_pos, SHADER_UNIFORM_VEC3);
    const int runtime_shadows_enabled = g_editor_preferences.shadows_enabled ? 1 : 0;
    SetShaderValue(lighting_shader, shadows_enabled_loc, &runtime_shadows_enabled, SHADER_UNIFORM_INT);
    SetShaderValue(lighting_shader, shadow_bias_loc, g_editor_preferences.shadow_bias);
    SetShaderValue(lighting_shader, shadow_filter_loc, g_editor_preferences.shadow_filter_quality);
    prepare_scene_light_uniforms(editor.scene, lighting_shader, scene_center);
    if (g_editor_preferences.shadows_enabled)
        render_scene_shadow_maps(editor.scene, shadow_shader, shadow_maps, shadow_cameras, scene_center,
            lighting_shader, light_view_locations, light_projection_locations);
    assign_shadow_maps(editor.scene, shadow_maps, lighting_shader);
}

void Application::render_frame() {
    BeginDrawing();
        ClearBackground(DARKGRAY);

        if (scene_rt.id > 0 && IsRenderTextureValid(scene_rt)) {
            BeginTextureMode(scene_rt);
            ClearBackground(Color{
                static_cast<unsigned char>(g_editor_preferences.background_red),
                static_cast<unsigned char>(g_editor_preferences.background_green),
                static_cast<unsigned char>(g_editor_preferences.background_blue),
                255
            });
            BeginMode3D(camera.get_camera());
                if (g_editor_preferences.show_grid)
                    DrawGrid(20, 1.0f);
                if (g_editor_preferences.show_axes) {
                    DrawLine3D({0, 0, 0}, {3, 0, 0}, RED);
                    DrawLine3D({0, 0, 0}, {0, 3, 0}, GREEN);
                    DrawLine3D({0, 0, 0}, {0, 0, 3}, BLUE);
                }
                for (int entity_index = 0; entity_index < static_cast<int>(editor.scene.entities.size()); ++entity_index) {
                    auto& e = editor.scene.entities[entity_index];
                    MeshComponent* mesh = e.get_mesh_component();
                    TransformComponent* transform = e.get_transform_component();
                    MaterialComponent* mat = e.get_material_component();
                    if (!mesh || !mesh->enabled || !transform) continue;

                    const bool has_materials = mesh->model.materialCount > 0 && mesh->model.materials != nullptr;
                    const bool shader_missing = has_materials &&
                        (mesh->model.materials[0].shader == nullptr ||
                         mesh->model.materials[0].shader->id != lighting_shader.id);

                    if (!mesh->shader_assigned || shader_missing) {
                        set_model_shader(mesh->model, lighting_shader);
                    }
                    mesh->shader_assigned = true;

                    int use = (mat && mat->texture.id != 0) ? 1 : 0;
                    SetShaderValue(lighting_shader, use_tex_loc, &use, SHADER_UNIFORM_INT);
                    draw_entity_with_texture(e, compose_entity_transform(editor.scene, entity_index));
                    if (g_editor_preferences.show_selection_visualization &&
                        editor.scene.is_selected(entity_index) && mesh->model.meshCount > 0) {
                        PushMatrix();
                        MultMatrix(compose_entity_transform(editor.scene, entity_index));
                        const bool primary_selection = entity_index == editor.scene.selected;
                        DrawBoundingBox(GetModelBoundingBox(mesh->model), Color{
                            static_cast<unsigned char>(g_editor_preferences.selection_red),
                            static_cast<unsigned char>(g_editor_preferences.selection_green),
                            static_cast<unsigned char>(g_editor_preferences.selection_blue),
                            static_cast<unsigned char>(primary_selection ? 255 : 150)
                        });
                        PopMatrix();
                    }
                    if (g_editor_preferences.show_bounding_boxes && mesh->model.meshCount > 0) {
                        PushMatrix();
                        MultMatrix(compose_entity_transform(editor.scene, entity_index));
                        DrawBoundingBox(GetModelBoundingBox(mesh->model), Color{
                            static_cast<unsigned char>(g_editor_preferences.bounds_red),
                            static_cast<unsigned char>(g_editor_preferences.bounds_green),
                            static_cast<unsigned char>(g_editor_preferences.bounds_blue), 255
                        });
                        PopMatrix();
                    }
                }
                if (g_editor_preferences.show_light_helpers) {
                    for (auto& entity : editor.scene.entities) {
                        LightComponent* light = entity.get_light_component();
                        TransformComponent* transform = entity.get_transform_component();
                        if (!light || !transform || !light->enabled) continue;
                        DrawSphere(transform->position, 0.15f, light->light.color);
                        DrawLine3D(transform->position, light->light.target, light->light.color);
                    }
                }
                if (g_editor_preferences.show_cameras) {
                    const Camera3D& editor_camera = camera.get_camera();
                    const Vec3 forward = (editor_camera.target - editor_camera.position).normalized();
                    const Vec3 right = forward.cross(editor_camera.up).normalized();
                    const Vec3 up = right.cross(forward).normalized();
                    const float length = 1.5f;
                    const float half_width = tanf(editor_camera.fovy * DEG2RAD * 0.5f) * length;
                    const Vec3 center = editor_camera.position + forward * length;
                    const Vec3 corners[4] = {
                        center + up * half_width - right * half_width,
                        center + up * half_width + right * half_width,
                        center - up * half_width + right * half_width,
                        center - up * half_width - right * half_width
                    };
                    for (int i = 0; i < 4; ++i) {
                        DrawLine3D(editor_camera.position, corners[i], YELLOW);
                        DrawLine3D(corners[i], corners[(i + 1) % 4], YELLOW);
                    }
                }
            EndMode3D();
            EndTextureMode();
        }

        qcImGuiBegin();

        const bool gizmo_busy = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
        if (!gizmo_busy && (IsCursorHidden() || g_is_scene_hovered)) {
            if (SDL_Window* window = GetNativeWindow()) {
                if (camera.active && g_scene_window_size.x > 0.0f && g_scene_window_size.y > 0.0f) {
                    const SDL_Rect scene_mouse_rect = {
                        static_cast<int>(g_scene_window_pos.x),
                        static_cast<int>(g_scene_window_pos.y),
                        static_cast<int>(g_scene_window_size.x),
                        static_cast<int>(g_scene_window_size.y)
                    };
                    SDL_SetWindowMouseRect(window, &scene_mouse_rect);
                } else if (!camera.active) {
                    SDL_SetWindowMouseRect(window, nullptr);
                }
            }
            camera.update(editor.scene);
        }

        editor.handle_input();

        editor.draw_ui(lighting_shader, camera, ctx);

        update_plugins(*g_plugin_manager, editor);

        qcImGuiEnd();
    EndDrawing();
}

void Application::run() {
    if (!ready_to_run)
        return;

    while (!WindowShouldClose()) {
        update_frame();
        render_frame();
    }
}

void Application::shutdown() {
    if (!ready_to_run)
        return;

    editor.scene.release_resources();
    g_editor_preferences.camera_fov = camera.get_camera().fovy;
    save_editor_preferences();
    unload_models();
    unload_textures();

    UnloadShader(lighting_shader);
    UnloadShader(shadow_shader);
    for (auto& shadow_map : shadow_maps)
        UnloadRenderTexture(shadow_map);
    g_plugin_manager->unload_all();
    qcImGuiShutdown();
    shutdown_freetype();
    CloseWindow();
}
