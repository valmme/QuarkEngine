#ifndef __TEX_H__
#define __TEX_H__
#include "entity.h"
#include "scene.h"
#include <functional>
#include <string>
#include <filesystem>
#include <cmath>
#include <cstring>

struct TextureOption {
    std::string name;
    Texture2D texture;
};

struct AssetEntry {
    std::string filename;
    bool is_directory = false;
    bool is_image;
    Texture2D texture = {0};
};

struct TextureMeta {
    std::string guid;
    int mip_map_mode = 0;
    bool enable_mip_map = false;
    bool srgb_texture = true;
    bool is_readable = false;
    int filter_mode = 0;
    int wrap_u = 1;
    int wrap_v = 1;
    int max_texture_size = 2048;
    int compression_quality = 50;
    int sprite_mode = 1;
    int texture_type = 8;
    bool alpha_is_transparency = true;
};

extern std::vector<TextureOption> texture_options;
extern std::vector<AssetEntry> asset_entries;
extern bool g_wireframe_enabled;

void load_textures(std::string project_path);
void unload_textures();
void apply_texture_repeat(Entity& e);
void store_uv(Entity* e);
void mark_entity_uv_dirty(Entity* e);
void mark_entity_bounds_dirty(Entity* e);
void refresh_entity_render_state(Entity& e);
void store_material_textures(Entity* e);
void restore_model_textures(Entity* e);
void clear_material_textures(Entity* e);
void draw_entity_with_texture(Entity& e);
void draw_entity_with_texture(Entity& e, const Mat4& world_transform);
void refresh_textures(Scene* scene, const std::string& project_path);
void load_assets(std::string project_path);
void refresh_assets(std::string project_path);
void clone_model_materials(Entity* e);
bool is_image_file(const std::filesystem::path& p);
bool ensure_texture_meta(const std::filesystem::path& texture_path);
bool load_texture_meta(const std::filesystem::path& texture_path, TextureMeta& meta);
bool save_texture_meta(const std::filesystem::path& texture_path, const TextureMeta& meta);
std::filesystem::path texture_path_from_meta(const std::filesystem::path& path);
void apply_texture_meta(Texture2D& texture, const TextureMeta& meta);

#endif // __TEX_H__
