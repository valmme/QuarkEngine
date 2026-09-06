#include "tex.h"
#include "models.h"
#include "editor/editor_preferences.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

std::vector<TextureOption> texture_options;
std::vector<AssetEntry> asset_entries;
bool g_wireframe_enabled = false;

static std::vector<fs::directory_entry> collect_resource_entries(const fs::path& resource_dir) {
    std::vector<fs::directory_entry> result;
    std::error_code ec;
    fs::recursive_directory_iterator it(resource_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return result;

    for (const auto& entry : it) {
        result.push_back(entry);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const fs::directory_entry& lhs, const fs::directory_entry& rhs) {
            std::error_code lhs_ec;
            std::error_code rhs_ec;
            const bool lhs_is_dir = lhs.is_directory(lhs_ec) && !lhs_ec;
            const bool rhs_is_dir = rhs.is_directory(rhs_ec) && !rhs_ec;

            if (lhs_is_dir != rhs_is_dir) return lhs_is_dir > rhs_is_dir;
            return lhs.path().generic_string() < rhs.path().generic_string();
        }
    );

    return result;
}

static std::vector<fs::path> collect_resource_files(const fs::path& resource_dir) {
    std::vector<fs::path> result;
    for (const auto& entry : collect_resource_entries(resource_dir)) {
        std::error_code ec;
        if (!entry.is_regular_file(ec) || ec) {
            continue;
        }

        result.push_back(entry.path());
    }

    return result;
}

bool is_image_file(const fs::path& p) {
    std::string ext = p.extension().string();
    for (auto& c : ext) c = (char)tolower(c);
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
}

namespace {

std::string make_texture_guid() {
    std::random_device device;
    std::mt19937_64 generator(device());
    std::uniform_int_distribution<unsigned long long> distribution;
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << distribution(generator)
           << std::setw(16) << distribution(generator);
    return stream.str();
}

bool parse_meta_value(const fs::path& texture_path, const char* key, std::string& value) {
    std::ifstream file(texture_path.string() + ".meta");
    if (!file.is_open()) return false;
    std::string line;
    const std::string prefix = std::string(key) + ":";
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.rfind(prefix, 0) != 0) continue;
        value = line.substr(prefix.size());
        value.erase(0, value.find_first_not_of(" \t"));
        if (value.empty()) continue;
        return true;
    }
    return false;
}

}

fs::path texture_path_from_meta(const fs::path& path) {
    if (path.extension() == ".meta" && path.stem().extension() != ".meta") {
        fs::path texture = path;
        texture.replace_extension("");
        return texture;
    }
    return path;
}

bool save_texture_meta(const fs::path& texture_path, const TextureMeta& meta) {
    std::ofstream file(texture_path.string() + ".meta", std::ios::trunc);
    if (!file.is_open()) return false;
    file << "quark_texture_meta: 1\n"
         << "asset_id: " << meta.guid << "\n"
         << "image:\n"
         << "  color_space: " << (meta.srgb_texture ? "srgb" : "linear") << "\n"
         << "  alpha_mode: " << (meta.alpha_is_transparency ? "transparency" : "straight") << "\n"
         << "  readable: " << (meta.is_readable ? "true" : "false") << "\n"
         << "sampling:\n"
         << "  mipmaps: " << (meta.enable_mip_map ? "true" : "false") << "\n"
         << "  filter: " << (meta.filter_mode == 1 ? "linear" : "nearest") << "\n"
         << "  wrap_u: " << (meta.wrap_u == 0 ? "repeat" : "clamp") << "\n"
         << "  wrap_v: " << (meta.wrap_v == 0 ? "repeat" : "clamp") << "\n"
         << "limits:\n"
         << "  max_size: " << meta.max_texture_size << "\n"
         << "  compression: " << meta.compression_quality << "\n"
         << "sprite:\n"
         << "  mode: " << meta.sprite_mode << "\n"
         << "  type: " << meta.texture_type << "\n";
    return file.good();
}

bool load_texture_meta(const fs::path& texture_path, TextureMeta& meta) {
    if (!fs::exists(texture_path.string() + ".meta")) return false;
    std::string value;
    if (parse_meta_value(texture_path, "asset_id", value)) meta.guid = value;
    if (meta.guid.empty()) parse_meta_value(texture_path, "guid", meta.guid);
    if (meta.guid.empty()) meta.guid = make_texture_guid();
    if (parse_meta_value(texture_path, "mipmaps", value)) meta.enable_mip_map = value == "true";
    if (parse_meta_value(texture_path, "color_space", value)) meta.srgb_texture = value == "srgb";
    if (parse_meta_value(texture_path, "readable", value)) meta.is_readable = value == "true";
    if (parse_meta_value(texture_path, "filter", value)) meta.filter_mode = value == "linear" ? 1 : 0;
    if (parse_meta_value(texture_path, "wrap_u", value)) meta.wrap_u = value == "repeat" ? 0 : 1;
    if (parse_meta_value(texture_path, "wrap_v", value)) meta.wrap_v = value == "repeat" ? 0 : 1;
    if (parse_meta_value(texture_path, "max_size", value)) { try { meta.max_texture_size = std::stoi(value); } catch (...) {} }
    if (parse_meta_value(texture_path, "compression", value)) { try { meta.compression_quality = std::stoi(value); } catch (...) {} }
    if (parse_meta_value(texture_path, "mode", value)) { try { meta.sprite_mode = std::stoi(value); } catch (...) {} }
    if (parse_meta_value(texture_path, "type", value)) { try { meta.texture_type = std::stoi(value); } catch (...) {} }
    if (parse_meta_value(texture_path, "alpha_mode", value)) meta.alpha_is_transparency = value == "transparency";
    return true;
}

bool ensure_texture_meta(const fs::path& texture_path) {
    if (!is_image_file(texture_path)) return false;
    TextureMeta meta;
    if (!load_texture_meta(texture_path, meta)) {
        meta.guid = make_texture_guid();
        return save_texture_meta(texture_path, meta);
    }
    return save_texture_meta(texture_path, meta);
}

void apply_texture_meta(Texture2D& texture, const TextureMeta& meta) {
    if (texture.id == 0) return;
    SetTextureFilter(texture, meta.filter_mode == 1 ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
    SetTextureWrap(texture, meta.wrap_u == 0 ? TEXTURE_WRAP_REPEAT : TEXTURE_WRAP_CLAMP);
    if (meta.enable_mip_map) GenTextureMipmaps(&texture);
}

void load_textures(std::string project_path) {
    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    unload_textures();
    texture_options.clear();
    texture_options.push_back({ "None", {0} });

    for (const auto& path : collect_resource_files(resource_dir)) {
        if (!is_image_file(path)) continue;

        ensure_texture_meta(path);
        Texture2D tex = LoadTexture(path.string().c_str());
        TextureMeta meta;
        if (load_texture_meta(path, meta)) apply_texture_meta(tex, meta);
        texture_options.push_back({ fs::relative(path, resource_dir).generic_string(), tex });
    }
}

void unload_textures() {
    std::unordered_set<unsigned int> released_ids;
    for (auto& opt : texture_options) {
        if (opt.texture.id == 0) continue;
        if (released_ids.insert(opt.texture.id).second) {
            UnloadTexture(opt.texture);
        }
    }
    texture_options.clear();
}

void apply_texture_repeat(Entity &e) {
    MeshComponent* mesh_component = e.get_mesh_component();
    MaterialComponent* mat_component = e.get_material_component();
    const TransformComponent* transform = e.get_transform_component();
    if (!mesh_component || !transform || !mat_component) return;

    for (int m = 0; m < mesh_component->model.meshCount; m++) {
        Mesh &mesh = mesh_component->model.meshes[m];
        if (!mesh.texcoords) continue;

        for (int i = 0; i < mesh.vertexCount; i++) {
            float u, v;

            if (mat_component->auto_uv) {
                Vec3 pos = {
                    mesh.vertices[i*3+0],
                    mesh.vertices[i*3+1],
                    mesh.vertices[i*3+2]
                };

                Vec3 normal = {
                    mesh.normals[i*3+0],
                    mesh.normals[i*3+1],
                    mesh.normals[i*3+2]
                };

                float ax = fabs(normal.x);
                float ay = fabs(normal.y);
                float az = fabs(normal.z);

                float sx = transform->scale.x;
                float sy = transform->scale.y;
                float sz = transform->scale.z;

                if (ay > ax && ay > az) {
                    u = pos.x * sx;
                    v = pos.z * sz;
                } 
                
                else if (ax > az) {
                    u = pos.z * sz;
                    v = pos.y * sy;
                } 
                
                else {
                    u = pos.x * sx;
                    v = pos.y * sy;
                }

                u *= mat_component->uv_scale.x;
                v *= mat_component->uv_scale.y;
            } else {
                if (m >= mat_component->original_texcoords.size()) continue;
                auto& base = mat_component->original_texcoords[m];

                u = base[i*2+0] * mat_component->texture_repeat_u * transform->scale.x;
                v = base[i*2+1] * mat_component->texture_repeat_v * transform->scale.y;
            }

            mesh.texcoords[i*2+0] = u;
            mesh.texcoords[i*2+1] = v;
        }

        UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
    }
}

void store_uv(Entity* e) {
    if (!e) return;
    MeshComponent* mesh_component = e->get_mesh_component();
    MaterialComponent* mat_component = e->get_material_component();
    if (!mesh_component || !mat_component) return;
    mat_component->original_texcoords.clear();

    for (int m = 0; m < mesh_component->model.meshCount; m++) {
        Mesh& mesh = mesh_component->model.meshes[m];

        if (!mesh.texcoords) {
            mat_component->original_texcoords.push_back({});
            continue;
        }

        std::vector<float> uv(mesh.vertexCount * 2);
        memcpy(uv.data(), mesh.texcoords, uv.size() * sizeof(float));

        mat_component->original_texcoords.push_back(uv);
    }

    mesh_component->uv_dirty = true;
    mesh_component->bounds_dirty = true;
}

void mark_entity_uv_dirty(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (mesh) mesh->uv_dirty = true;
}

void mark_entity_bounds_dirty(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (mesh) mesh->bounds_dirty = true;
}

void store_material_textures(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    MaterialComponent* mat = e->get_material_component();
    if (!mesh || !mat) return;
    mat->original_material_textures.clear();
    mat->original_material_textures.reserve(mesh->model.materialCount);

    for (int i = 0; i < mesh->model.materialCount; i++) {
        mat->original_material_textures.push_back(
            mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture
        );
    }
}

void restore_model_textures(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    MaterialComponent* mat = e->get_material_component();
    if (!mesh) return;
    if (mat->original_material_textures.size() != static_cast<size_t>(mesh->model.materialCount)) return;

    for (int i = 0; i < mesh->model.materialCount; i++) {
        mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = mat->original_material_textures[i];
    }
}

void clear_material_textures(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (!mesh) return;
    for (int i = 0; i < mesh->model.materialCount; i++) {
        mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = {0};
    }
}

void refresh_entity_render_state(Entity& e) {
    MeshComponent* mesh = e.get_mesh_component();
    MaterialComponent* mat = e.get_material_component();
    if (!mesh || !mesh->uv_dirty || !mat) return;

    if (mat->texture.id != 0) {
        for (int i = 0; i < mesh->model.materialCount; i++) {
            mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = mat->texture;
        }
    }

    if (mat->texture_stretch) {
        for (int m = 0; m < mesh->model.meshCount; m++) {
            Mesh& model_mesh = mesh->model.meshes[m];
            if (!model_mesh.texcoords || m >= mat->original_texcoords.size()) continue;

            memcpy(model_mesh.texcoords, mat->original_texcoords[m].data(), model_mesh.vertexCount * 2 * sizeof(float));
            UpdateMeshBuffer(model_mesh, 1, model_mesh.texcoords, model_mesh.vertexCount * 2 * sizeof(float), 0);
        }
    } else {
        apply_texture_repeat(e);
    }

    mesh->uv_dirty = false;
}

void draw_collision_debug(Entity& entity) {
    CollisionComponent* collision = entity.get_collision_component();
    MeshComponent* mesh_component = entity.get_mesh_component();
    TransformComponent* transform = entity.get_transform_component();

    if (!collision || !transform || !collision->visualize || !g_editor_preferences.show_colliders) return;

    Vec3 worldPos = transform->position + collision->center;

    Mat4 matScale = Mat4::scale(
        transform->scale.x,
        transform->scale.y,
        transform->scale.z
    );
    Mat4 matRotX = Mat4::rotationX(DEG2RAD * transform->rotation.x);
    Mat4 matRotY = Mat4::rotationY(DEG2RAD * transform->rotation.y);
    Mat4 matRotZ = Mat4::rotationZ(DEG2RAD * transform->rotation.z);
    Mat4 matTrans = Mat4::translation(worldPos.x, worldPos.y, worldPos.z);

    Mat4 world =
        matScale *
        matRotX *
        matRotY *
        matRotZ *
        matTrans;

    Color lineColor = GREEN;
    Color pointColor = LIME;

    PushMatrix();
    MultMatrix(world);

    switch (collision->collider_type) {

        case COLLIDER_BOX: {
            DrawCubeWires({0,0,0},
                collision->size.x,
                collision->size.y,
                collision->size.z,
                lineColor);
            break;
        }

        case COLLIDER_SPHERE: {
            DrawSphereWires({0,0,0},
                collision->radius,
                16, 16,
                lineColor);
            break;
        }

        case COLLIDER_CAPSULE: {
            float r = collision->radius;
            float h = collision->height;

            float cylinderH = std::max(0.0f, h - r * 2.0f);

            DrawCylinderWires({0,0,0}, r, r, cylinderH, 16, lineColor);

            DrawSphereWires({0, cylinderH * 0.5f, 0}, r, 12, 12, lineColor);
            DrawSphereWires({0,-cylinderH * 0.5f, 0}, r, 12, 12, lineColor);
            break;
        }

        case COLLIDER_MESH: {
            if (!mesh_component) break;

            DrawModelWires(mesh_component->model, {0,0,0}, 1.0f, lineColor);
            break;
        }
    }

    PopMatrix();
}

void draw_entity_with_texture(Entity& e) {
    const TransformComponent* transform = e.get_transform_component();
    if (!transform) return;

    Mat4 local_transform = Mat4::translation(transform->position.x, transform->position.y, transform->position.z) *
        Mat4::rotationX(transform->rotation.x * DEG2RAD) *
        Mat4::rotationY(transform->rotation.y * DEG2RAD) *
        Mat4::rotationZ(transform->rotation.z * DEG2RAD) *
        Mat4::scale(transform->scale.x, transform->scale.y, transform->scale.z);
    draw_entity_with_texture(e, local_transform);
}

void draw_entity_with_texture(Entity& e, const Mat4& world_transform) {
    refresh_entity_render_state(e);
    const MeshComponent* mesh = e.get_mesh_component();
    const MaterialComponent* mat = e.get_material_component();
    if (!mesh || !mat) return;

    PushMatrix();
    MultMatrix(world_transform);

    const bool edited_mesh_is_double_sided = entity_has_mesh_overrides(e) || mesh->mesh_triangles_detached;
    if (edited_mesh_is_double_sided) DisableBackfaceCulling();

    DrawModel(mesh->model, {0,0,0}, 1.0f, mat->color);
    if (g_wireframe_enabled && mat->outline_color.a > 0) {
        Color wireframe_color = {
            static_cast<unsigned char>(g_editor_preferences.wireframe_red),
            static_cast<unsigned char>(g_editor_preferences.wireframe_green),
            static_cast<unsigned char>(g_editor_preferences.wireframe_blue), 255
        };
        DrawModelWires(mesh->model, {0,0,0}, 1.0f, wireframe_color);
    }

    if (edited_mesh_is_double_sided) EnableBackfaceCulling();

    PopMatrix();
    draw_collision_debug(e);
}

void refresh_textures(Scene* scene, const std::string& project_path) {
    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    std::unordered_map<std::string, Texture2D> old_by_name;
    for (const auto& opt : texture_options) {
        if (opt.texture.id != 0) {
            old_by_name[opt.name] = opt.texture;
        }
    }

    std::vector<TextureOption> next_options;
    next_options.push_back({ "None", {0} });

    for (const auto& path : collect_resource_files(resource_dir)) {
        if (!is_image_file(path)) continue;

        const std::string texture_name = fs::relative(path, resource_dir).generic_string();
        auto old_it = old_by_name.find(texture_name);
        if (old_it != old_by_name.end()) {
            next_options.push_back({ texture_name, old_it->second });
            old_by_name.erase(old_it);
            continue;
        }

        Texture2D tex = LoadTexture(path.string().c_str());
        next_options.push_back({ texture_name, tex });
    }

    if (scene) {
        for (const auto& [_, removed_tex] : old_by_name) {
            for (auto& entity : scene->entities) {
                MeshComponent* mesh = entity.get_mesh_component();
                MaterialComponent* mat = entity.get_material_component();
                if (mesh && mat->texture.id == removed_tex.id) {
                    mat->texture = {0};
                }
            }
        }
    }

    std::unordered_set<unsigned int> released_ids;
    for (auto& [_, removed_tex] : old_by_name) {
        if (removed_tex.id == 0) continue;
        if (released_ids.insert(removed_tex.id).second) {
            UnloadTexture(removed_tex);
        }
    }

    texture_options = std::move(next_options);
}

void load_assets(std::string project_path) {
    asset_entries.clear();
    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    for (const auto& entry : collect_resource_entries(resource_dir)) {
        std::error_code ec;
        AssetEntry asset_entry;
        asset_entry.filename = fs::relative(entry.path(), resource_dir).generic_string();
        asset_entry.is_directory = entry.is_directory(ec) && !ec;
        asset_entry.is_image = !asset_entry.is_directory && is_image_file(entry.path());

        asset_entries.push_back(asset_entry);
    }
}

void refresh_assets(std::string project_path) {
    if (project_path.empty()) return;

    for (auto& asset : asset_entries) {
        if (asset.is_image && asset.texture.id != 0) {
            UnloadTexture(asset.texture);
        }
    }

    asset_entries.clear();
    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    for (const auto& entry : collect_resource_entries(resource_dir)) {
        std::error_code ec;
        AssetEntry a;
        a.filename = fs::relative(entry.path(), resource_dir).generic_string();
        a.is_directory = entry.is_directory(ec) && !ec;
        a.is_image = !a.is_directory && is_image_file(entry.path());

        if (a.is_image) ensure_texture_meta(entry.path());

        asset_entries.push_back(a);
    }
}

void clone_model_materials(Entity* e) {
    if (!e) return;
    MeshComponent* mesh = e->get_mesh_component();
    if (!mesh || mesh->model.materialCount <= 0) return;

    if (mesh->owns_materials) {
        if (mesh->model.materials)    free(mesh->model.materials);
        if (mesh->model.meshMaterial) free(mesh->model.meshMaterial);
        mesh->model.materials    = nullptr;
        mesh->model.meshMaterial = nullptr;
        mesh->owns_materials     = false;
    }

    if (!mesh->asset || !mesh->asset->loaded_model.materials) return;

    Material* cloned = (Material*)malloc(mesh->model.materialCount * sizeof(Material));
    memcpy(cloned, mesh->asset->loaded_model.materials, mesh->model.materialCount * sizeof(Material));
    mesh->model.materials = cloned;

    mesh->model.meshMaterial = (int*)malloc(mesh->model.meshCount * sizeof(int));
    memcpy(mesh->model.meshMaterial, mesh->asset->loaded_model.meshMaterial, mesh->model.meshCount * sizeof(int));

    mesh->owns_materials = true;
}


