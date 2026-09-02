#ifndef __EDITOR_VIEWERS_H__
#define __EDITOR_VIEWERS_H__

#include "editor.h"
#include "../models.h"
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>

void draw_model_viewer_window();
void set_model_viewer_model(const Model& model);
bool open_model_viewer_for_asset(const ModelAsset& asset);

void draw_material_viewer_window(Editor& editor, Entity* selected_entity = nullptr);
void apply_material_settings();
void rebuild_material_preview_mesh();
void save_material_to_file(Editor& editor);
void load_textures_in_directory();
void load_material_texture(const std::string& texture_name);
void load_material_to_entity(Entity* entity, const std::filesystem::path& mtl_path, int material_slot = -1);
std::vector<std::string> get_all_materials_in_project();
bool open_material_viewer_for_path(const std::filesystem::path& material_path, std::unordered_map<std::string, Texture>& texture_cache);

bool is_model_viewer_visible();
bool is_material_viewer_visible();

void show_model_viewer_window(bool show);
void show_material_viewer_window(bool show);

void cleanup_viewers();

#endif // __EDITOR_VIEWERS_H__
