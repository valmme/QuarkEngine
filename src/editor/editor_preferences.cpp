#include "editor/editor_preferences.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>

using json = nlohmann::json;

EditorPreferences g_editor_preferences;

static void read_preferences(const json& data) {
    const json* preferences = &data;
    if (data.contains("editor_preferences") && data["editor_preferences"].is_object())
        preferences = &data["editor_preferences"];

    if (preferences->contains("wireframe_enabled")) g_editor_preferences.wireframe_enabled = (*preferences)["wireframe_enabled"].get<bool>();
    if (preferences->contains("show_grid")) g_editor_preferences.show_grid = (*preferences)["show_grid"].get<bool>();
    if (preferences->contains("show_axes")) g_editor_preferences.show_axes = (*preferences)["show_axes"].get<bool>();
    if (preferences->contains("show_colliders")) g_editor_preferences.show_colliders = (*preferences)["show_colliders"].get<bool>();
    if (preferences->contains("limit_fps")) g_editor_preferences.limit_fps = (*preferences)["limit_fps"].get<bool>();
    if (preferences->contains("target_fps")) g_editor_preferences.target_fps = (*preferences)["target_fps"].get<int>();
    if (preferences->contains("camera_speed")) g_editor_preferences.camera_speed = (*preferences)["camera_speed"].get<float>();
    if (preferences->contains("camera_sensitivity")) g_editor_preferences.camera_sensitivity = (*preferences)["camera_sensitivity"].get<float>();
    if (preferences->contains("camera_zoom_sensitivity")) g_editor_preferences.camera_zoom_sensitivity = (*preferences)["camera_zoom_sensitivity"].get<float>();
    if (preferences->contains("camera_fov")) g_editor_preferences.camera_fov = (*preferences)["camera_fov"].get<float>();
    if (preferences->contains("background_red")) g_editor_preferences.background_red = (*preferences)["background_red"].get<int>();
    if (preferences->contains("background_green")) g_editor_preferences.background_green = (*preferences)["background_green"].get<int>();
    if (preferences->contains("background_blue")) g_editor_preferences.background_blue = (*preferences)["background_blue"].get<int>();
    if (preferences->contains("autosave_enabled")) g_editor_preferences.autosave_enabled = (*preferences)["autosave_enabled"].get<bool>();
    if (preferences->contains("autosave_backup_enabled")) g_editor_preferences.autosave_backup_enabled = (*preferences)["autosave_backup_enabled"].get<bool>();
    if (preferences->contains("autosave_interval_minutes")) g_editor_preferences.autosave_interval_minutes = (*preferences)["autosave_interval_minutes"].get<int>();
    if (preferences->contains("gizmo_snap_enabled")) g_editor_preferences.gizmo_snap_enabled = (*preferences)["gizmo_snap_enabled"].get<bool>();
    if (preferences->contains("gizmo_translation_snap")) g_editor_preferences.gizmo_translation_snap = (*preferences)["gizmo_translation_snap"].get<float>();
    if (preferences->contains("gizmo_rotation_snap")) g_editor_preferences.gizmo_rotation_snap = (*preferences)["gizmo_rotation_snap"].get<float>();
    if (preferences->contains("gizmo_scale_snap")) g_editor_preferences.gizmo_scale_snap = (*preferences)["gizmo_scale_snap"].get<float>();
    if (preferences->contains("show_bounding_boxes")) g_editor_preferences.show_bounding_boxes = (*preferences)["show_bounding_boxes"].get<bool>();
    if (preferences->contains("show_selection_visualization")) g_editor_preferences.show_selection_visualization = (*preferences)["show_selection_visualization"].get<bool>();
    if (preferences->contains("selection_red")) g_editor_preferences.selection_red = (*preferences)["selection_red"].get<int>();
    if (preferences->contains("selection_green")) g_editor_preferences.selection_green = (*preferences)["selection_green"].get<int>();
    if (preferences->contains("selection_blue")) g_editor_preferences.selection_blue = (*preferences)["selection_blue"].get<int>();
    if (preferences->contains("wireframe_red")) g_editor_preferences.wireframe_red = (*preferences)["wireframe_red"].get<int>();
    if (preferences->contains("wireframe_green")) g_editor_preferences.wireframe_green = (*preferences)["wireframe_green"].get<int>();
    if (preferences->contains("wireframe_blue")) g_editor_preferences.wireframe_blue = (*preferences)["wireframe_blue"].get<int>();
    if (preferences->contains("bounds_red")) g_editor_preferences.bounds_red = (*preferences)["bounds_red"].get<int>();
    if (preferences->contains("bounds_green")) g_editor_preferences.bounds_green = (*preferences)["bounds_green"].get<int>();
    if (preferences->contains("bounds_blue")) g_editor_preferences.bounds_blue = (*preferences)["bounds_blue"].get<int>();
    if (preferences->contains("confirm_delete")) g_editor_preferences.confirm_delete = (*preferences)["confirm_delete"].get<bool>();
    if (preferences->contains("focus_on_selection")) g_editor_preferences.focus_on_selection = (*preferences)["focus_on_selection"].get<bool>();
    if (preferences->contains("shadows_enabled")) g_editor_preferences.shadows_enabled = (*preferences)["shadows_enabled"].get<bool>();
    if (preferences->contains("shadow_map_size")) g_editor_preferences.shadow_map_size = (*preferences)["shadow_map_size"].get<int>();
    if (preferences->contains("shadow_bias")) g_editor_preferences.shadow_bias = (*preferences)["shadow_bias"].get<float>();
    if (preferences->contains("shadow_filter_quality")) g_editor_preferences.shadow_filter_quality = (*preferences)["shadow_filter_quality"].get<int>();
    if (preferences->contains("undo_history_limit")) g_editor_preferences.undo_history_limit = (*preferences)["undo_history_limit"].get<int>();
    if (preferences->contains("vsync_enabled")) g_editor_preferences.vsync_enabled = (*preferences)["vsync_enabled"].get<bool>();
    if (preferences->contains("interface_scale")) g_editor_preferences.interface_scale = (*preferences)["interface_scale"].get<float>();
    if (preferences->contains("light_theme")) g_editor_preferences.light_theme = (*preferences)["light_theme"].get<bool>();
    if (preferences->contains("show_light_helpers")) g_editor_preferences.show_light_helpers = (*preferences)["show_light_helpers"].get<bool>();
    if (preferences->contains("show_cameras")) g_editor_preferences.show_cameras = (*preferences)["show_cameras"].get<bool>();
    if (preferences->contains("renderer_backend")) g_editor_preferences.renderer_backend = (*preferences)["renderer_backend"].get<int>();
    if (preferences->contains("msaa_samples")) g_editor_preferences.msaa_samples = (*preferences)["msaa_samples"].get<int>();
    if (preferences->contains("texture_filter")) g_editor_preferences.texture_filter = (*preferences)["texture_filter"].get<int>();
    if (preferences->contains("confirm_exit")) g_editor_preferences.confirm_exit = (*preferences)["confirm_exit"].get<bool>();
    if (preferences->contains("open_last_project")) g_editor_preferences.open_last_project = (*preferences)["open_last_project"].get<bool>();
    if (preferences->contains("last_project_path")) g_editor_preferences.last_project_path = (*preferences)["last_project_path"].get<std::string>();
    if (preferences->contains("show_hierarchy")) g_editor_preferences.show_hierarchy = (*preferences)["show_hierarchy"].get<bool>();
    if (preferences->contains("show_inspector")) g_editor_preferences.show_inspector = (*preferences)["show_inspector"].get<bool>();
    if (preferences->contains("show_assets")) g_editor_preferences.show_assets = (*preferences)["show_assets"].get<bool>();
    if (preferences->contains("show_scene")) g_editor_preferences.show_scene = (*preferences)["show_scene"].get<bool>();
    if (preferences->contains("asset_preview_size")) g_editor_preferences.asset_preview_size = (*preferences)["asset_preview_size"].get<int>();
    if (preferences->contains("asset_filter")) g_editor_preferences.asset_filter = (*preferences)["asset_filter"].get<int>();

    if (g_editor_preferences.target_fps > 0)
        g_editor_preferences.target_fps = std::clamp(g_editor_preferences.target_fps, 30, 240);
    g_editor_preferences.camera_speed = std::clamp(g_editor_preferences.camera_speed, 0.1f, 20.0f);
    g_editor_preferences.camera_sensitivity = std::clamp(g_editor_preferences.camera_sensitivity, 0.0005f, 0.02f);
    g_editor_preferences.camera_zoom_sensitivity = std::clamp(g_editor_preferences.camera_zoom_sensitivity, 0.1f, 5.0f);
    g_editor_preferences.camera_fov = std::clamp(g_editor_preferences.camera_fov, 20.0f, 120.0f);
    if (g_editor_preferences.renderer_backend < 0 || g_editor_preferences.renderer_backend > 3) g_editor_preferences.renderer_backend = 0;
    if (g_editor_preferences.msaa_samples != 1 && g_editor_preferences.msaa_samples != 2 && g_editor_preferences.msaa_samples != 4 && g_editor_preferences.msaa_samples != 8) g_editor_preferences.msaa_samples = 1;
    if (g_editor_preferences.texture_filter < 0 || g_editor_preferences.texture_filter > 1) g_editor_preferences.texture_filter = 1;
    g_editor_preferences.interface_scale = std::clamp(g_editor_preferences.interface_scale, 0.75f, 2.0f);
    g_editor_preferences.background_red = std::clamp(g_editor_preferences.background_red, 0, 255);
    g_editor_preferences.background_green = std::clamp(g_editor_preferences.background_green, 0, 255);
    g_editor_preferences.background_blue = std::clamp(g_editor_preferences.background_blue, 0, 255);
    g_editor_preferences.shadow_bias = std::clamp(g_editor_preferences.shadow_bias, 0.0001f, 0.05f);
    g_editor_preferences.shadow_filter_quality = std::clamp(g_editor_preferences.shadow_filter_quality, 0, 2);
    g_editor_preferences.undo_history_limit = std::clamp(g_editor_preferences.undo_history_limit, 10, 500);
    g_editor_preferences.asset_preview_size = std::clamp(g_editor_preferences.asset_preview_size, 32, 128);
    g_editor_preferences.asset_filter = std::clamp(g_editor_preferences.asset_filter, 0, 3);
    g_editor_preferences.autosave_interval_minutes = std::clamp(g_editor_preferences.autosave_interval_minutes, 1, 60);
    g_editor_preferences.gizmo_translation_snap = std::clamp(g_editor_preferences.gizmo_translation_snap, 0.01f, 10.0f);
    g_editor_preferences.gizmo_rotation_snap = std::clamp(g_editor_preferences.gizmo_rotation_snap, 1.0f, 90.0f);
    g_editor_preferences.gizmo_scale_snap = std::clamp(g_editor_preferences.gizmo_scale_snap, 0.01f, 1.0f);
    if (g_editor_preferences.shadow_map_size != 512 &&
        g_editor_preferences.shadow_map_size != 1024 &&
        g_editor_preferences.shadow_map_size != 2048)
        g_editor_preferences.shadow_map_size = 1024;
}

void save_editor_preferences() {
    json preferences = {
        {"wireframe_enabled", g_editor_preferences.wireframe_enabled},
        {"show_grid", g_editor_preferences.show_grid},
        {"show_axes", g_editor_preferences.show_axes},
        {"show_colliders", g_editor_preferences.show_colliders},
        {"limit_fps", g_editor_preferences.limit_fps},
        {"target_fps", g_editor_preferences.target_fps},
        {"camera_speed", g_editor_preferences.camera_speed},
        {"camera_sensitivity", g_editor_preferences.camera_sensitivity},
        {"camera_zoom_sensitivity", g_editor_preferences.camera_zoom_sensitivity},
        {"camera_fov", g_editor_preferences.camera_fov},
        {"background_red", g_editor_preferences.background_red},
        {"background_green", g_editor_preferences.background_green},
        {"background_blue", g_editor_preferences.background_blue},
        {"autosave_enabled", g_editor_preferences.autosave_enabled},
        {"autosave_backup_enabled", g_editor_preferences.autosave_backup_enabled},
        {"autosave_interval_minutes", g_editor_preferences.autosave_interval_minutes},
        {"gizmo_snap_enabled", g_editor_preferences.gizmo_snap_enabled},
        {"gizmo_translation_snap", g_editor_preferences.gizmo_translation_snap},
        {"gizmo_rotation_snap", g_editor_preferences.gizmo_rotation_snap},
        {"gizmo_scale_snap", g_editor_preferences.gizmo_scale_snap},
        {"show_bounding_boxes", g_editor_preferences.show_bounding_boxes},
        {"show_selection_visualization", g_editor_preferences.show_selection_visualization},
        {"selection_red", g_editor_preferences.selection_red},
        {"selection_green", g_editor_preferences.selection_green},
        {"selection_blue", g_editor_preferences.selection_blue},
        {"wireframe_red", g_editor_preferences.wireframe_red},
        {"wireframe_green", g_editor_preferences.wireframe_green},
        {"wireframe_blue", g_editor_preferences.wireframe_blue},
        {"bounds_red", g_editor_preferences.bounds_red},
        {"bounds_green", g_editor_preferences.bounds_green},
        {"bounds_blue", g_editor_preferences.bounds_blue},
        {"confirm_delete", g_editor_preferences.confirm_delete},
        {"focus_on_selection", g_editor_preferences.focus_on_selection},
        {"shadows_enabled", g_editor_preferences.shadows_enabled},
        {"shadow_map_size", g_editor_preferences.shadow_map_size},
        {"shadow_bias", g_editor_preferences.shadow_bias},
        {"shadow_filter_quality", g_editor_preferences.shadow_filter_quality},
        {"undo_history_limit", g_editor_preferences.undo_history_limit},
        {"vsync_enabled", g_editor_preferences.vsync_enabled},
        {"interface_scale", g_editor_preferences.interface_scale},
        {"light_theme", g_editor_preferences.light_theme},
        {"show_light_helpers", g_editor_preferences.show_light_helpers},
        {"show_cameras", g_editor_preferences.show_cameras},
        {"renderer_backend", g_editor_preferences.renderer_backend},
        {"msaa_samples", g_editor_preferences.msaa_samples},
        {"texture_filter", g_editor_preferences.texture_filter},
        {"confirm_exit", g_editor_preferences.confirm_exit},
        {"open_last_project", g_editor_preferences.open_last_project},
        {"last_project_path", g_editor_preferences.last_project_path},
        {"show_hierarchy", g_editor_preferences.show_hierarchy},
        {"show_inspector", g_editor_preferences.show_inspector},
        {"show_assets", g_editor_preferences.show_assets},
        {"show_scene", g_editor_preferences.show_scene},
        {"asset_preview_size", g_editor_preferences.asset_preview_size},
        {"asset_filter", g_editor_preferences.asset_filter}
    };

    json config;
    std::ifstream in("config.json");
    if (in.is_open()) {
        try { in >> config; } catch (...) { config = json::object(); }
    }
    config["editor_preferences"] = preferences;

    std::ofstream out("config.json");
    if (out.is_open()) out << config.dump(4);
}

void load_editor_preferences() {
    std::ifstream config_file("config.json");
    if (config_file.is_open()) {
        try {
            json config;
            config_file >> config;
            if (config.contains("editor_preferences")) {
                read_preferences(config);
                std::filesystem::remove("editor_preferences.json");
                return;
            }
        } catch (...) {
        }
    }

    std::ifstream legacy_file("editor_preferences.json");
    if (legacy_file.is_open()) {
        try {
            json legacy_preferences;
            legacy_file >> legacy_preferences;
            read_preferences(legacy_preferences);
            save_editor_preferences();
            std::filesystem::remove("editor_preferences.json");
            return;
        } catch (...) {
        }
    }

    save_editor_preferences();
}