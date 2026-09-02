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
    void undo();
    void redo();
};

#endif // __EDITOR_H__
