#ifndef __EDITOR_UI_H__
#define __EDITOR_UI_H__

#include "../editor.h"
#include "../camera.h"
#include "../entity.h"
#include "editor_hierarchy_utils.h"
#include "editor_file_utils.h"

struct MeshEditState {
    bool enabled = false;
    int entity_index = -1;
    int mesh_index = 0;
    int triangle_index = 0;
    int vertex_corner = 0;
    bool was_using_gizmo = false;
};

enum PolygonEditMode {
    POLY_NONE,
    POLY_CREATE,
    POLY_MOVE
};

extern MeshEditState g_mesh_edit_state;
extern PolygonEditMode g_poly_mode;
extern std::vector<int> g_selected_vertices;
extern bool show_hierarchy;
extern bool show_inspector;
extern bool show_assets;
extern bool show_scene;

void copy_entity(Entity* entity);
void paste_entity(Editor& editor);
void dublicate_entity(Editor& editor, Entity* entity);
Entity clone_entity_instance(const Entity& source, Scene& scene);
void erase_entity_after_hierarchy(Editor& editor, int index);
void delete_entity(Editor& editor, Entity* entity);

void draw_ui(Editor& editor, Shader shader, FlyCamera& camera, PluginContext* ctx);

void draw_gizmo(Editor& editor, FlyCamera camera);
void handle_scene_asset_drop(Editor& editor, Camera3D camera);

void draw_mesh_vertex_overlay(Editor& editor, Camera3D camera);
void reset_mesh_edit_model(Entity& entity);

#endif // __EDITOR_UI_H__
