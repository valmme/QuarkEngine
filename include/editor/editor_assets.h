#ifndef __EDITOR_ASSETS_H__
#define __EDITOR_ASSETS_H__

#include "../editor.h"
#include "../models.h"
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

struct LocalEntry {
    std::string filename;
    bool is_directory;
    bool is_image;
    bool is_model;
    bool is_material;
    bool is_texture_meta;
    Texture texture;
    std::string extension;
};

ModelAsset* find_asset_by_name(const std::string& asset_name);
ModelAsset* find_asset_by_path(const fs::path& full_path, const fs::path& project_path_value);

std::string build_resource_signature(const fs::path& resource_dir);
Texture create_model_preview(const ModelAsset& asset, const std::string& cache_key, int preview_size = 64);

void draw_assets_ui(Editor& editor);
void draw_selected_texture_inspector(Editor& editor);

bool import_path_to_resources(const fs::path& src, const fs::path& resource_dir);

void cleanup_assets_ui();
void invalidate_material_previews();

#endif // __EDITOR_ASSETS_H__
