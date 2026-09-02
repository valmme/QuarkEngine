#ifndef __EDITOR_PREFERENCES_H__
#define __EDITOR_PREFERENCES_H__

#include <string>

struct EditorPreferences {
    bool wireframe_enabled = false;
    bool show_grid = true;
    bool show_axes = false;
    bool show_colliders = false;
    bool limit_fps = true;
    int target_fps = 0;
    float camera_speed = 2.0f;
    float camera_sensitivity = 0.003f;
    float camera_zoom_sensitivity = 1.0f;
    float camera_fov = 45.0f;
    int background_red = 36;
    int background_green = 38;
    int background_blue = 42;
    bool autosave_enabled = false;
    bool autosave_backup_enabled = true;
    int autosave_interval_minutes = 5;
    bool gizmo_snap_enabled = false;
    float gizmo_translation_snap = 0.5f;
    float gizmo_rotation_snap = 15.0f;
    float gizmo_scale_snap = 0.1f;
    bool show_bounding_boxes = false;
    bool show_selection_visualization = true;
    int selection_red = 80;
    int selection_green = 140;
    int selection_blue = 255;
    int wireframe_red = 80;
    int wireframe_green = 80;
    int wireframe_blue = 80;
    int bounds_red = 255;
    int bounds_green = 220;
    int bounds_blue = 40;
    bool confirm_delete = true;
    bool focus_on_selection = false;
    bool shadows_enabled = true;
    int shadow_map_size = 1024;
    float shadow_bias = 0.004f;
    int shadow_filter_quality = 1;
    int undo_history_limit = 100;
    bool vsync_enabled = true;
    float interface_scale = 1.0f;
    bool light_theme = false;
    bool show_light_helpers = false;
    bool show_cameras = false;
    int renderer_backend = 1;
    int msaa_samples = 1;
    int texture_filter = 1;
    bool confirm_exit = true;
    bool open_last_project = false;
    std::string last_project_path;
    bool show_hierarchy = true;
    bool show_inspector = true;
    bool show_assets = true;
    bool show_scene = true;
    int asset_preview_size = 64;
    int asset_filter = 0;
};

extern EditorPreferences g_editor_preferences;

void load_editor_preferences();
void save_editor_preferences();

#endif // __EDITOR_PREFERENCES_H__
