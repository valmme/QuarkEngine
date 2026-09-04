#ifndef __EDITOR_H__
#define __EDITOR_H__
#include "scene.h"
#include "camera.h"
#include "tex.h"
#include "models.h"
#include <stack>
#include <filesystem>
#include "plugins/plugin_manager.h"

struct SceneState {
    std::vector<Entity> entities;
    int selected;
    std::vector<int> selected_entities;
    bool light_only = false;
    std::vector<Lighting> lights;
    bool hierarchy_only = false;
    std::vector<int> parent_ids;
    std::vector<Vec3> positions;
    std::vector<Vec3> rotations;
    std::vector<Vec3> scales;
    bool duplicate_only = false;
    int duplicate_index = -1;
    int duplicate_source_index = -1;
    bool tags_only = false;
    std::vector<std::vector<std::string>> tags;
    bool material_only = false;
    struct MaterialSnapshot {
        TextureSource texture_source = TEXTURE_NONE;
        std::string albedo_texture_name;
        std::string texture_name;
        std::vector<std::string> material_slot_sources;
        Color color = WHITE;
        Color outline_color = LIGHTGRAY;
        bool auto_uv = false;
        bool texture_stretch = true;
        float texture_repeat_u = 1.0f;
        float texture_repeat_v = 1.0f;
        Vec2 uv_scale = {1, 1};
    };
    std::vector<MaterialSnapshot> materials;
};

struct Editor {
    Scene scene;
    std::string project_path = "projects/TestProject";
    int selected_asset_index = -1;
    std::string selected_asset_name;
    bool scene_dirty = false;

    std::stack<SceneState> undo_stack;
    std::stack<SceneState> redo_stack;

    PluginManager* plugin_manager = nullptr;

    std::filesystem::path current_asset_path;
    
    void draw_ui(Shader shader, FlyCamera& camera, PluginContext* ctx);
    void draw_assets_ui();
    void handle_input();
    void draw_entity_with_texture(Entity& e);
    void save_state();
    void save_light_state();
    void save_hierarchy_state();
    void save_transform_state(Entity* entity, const Vec3& position, const Vec3& rotation, const Vec3& scale);
    void save_duplicate_state(int source_index);
    void save_tags_state();
    void save_material_state();
    void save_material_state_before(Entity* entity, const MaterialComponent& material);
    void undo();
    void redo();
};

#endif // __EDITOR_H__
