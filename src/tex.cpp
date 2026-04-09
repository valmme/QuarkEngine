#include "headers/tex.h"
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;
std::vector<TextureOption> texture_options;

static bool is_image_file(const fs::path& p) {
    std::string ext = p.extension().string();
    for (auto& c : ext) c = (char)tolower(c);
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
}

void load_textures() {
    if (!fs::exists("assets")) fs::create_directories("assets");

    unload_textures();
    texture_options.clear();
    texture_options.push_back({ "None", {0} });

    for (const auto& entry : fs::directory_iterator("assets")) {
        if (!entry.is_regular_file()) continue;
        if (!is_image_file(entry.path())) continue;

        Texture2D tex = LoadTexture(entry.path().string().c_str());
        texture_options.push_back({ entry.path().filename().string(), tex });
    }
}

void unload_textures() {
    std::unordered_set<unsigned int> released_ids;
    for (const auto& opt : texture_options) {
        if (opt.texture.id == 0) continue;
        if (released_ids.insert(opt.texture.id).second) {
            UnloadTexture(opt.texture);
        }
    }
    texture_options.clear();
}

void apply_texture_repeat(Entity &e) {
    for (int m = 0; m < e.model.meshCount; m++) {
        Mesh &mesh = e.model.meshes[m];
        if (!mesh.texcoords) continue;

        for (int i = 0; i < mesh.vertexCount; i++) {
            float u, v;

            if (e.auto_uv) {
                Vector3 pos = {
                    mesh.vertices[i*3+0],
                    mesh.vertices[i*3+1],
                    mesh.vertices[i*3+2]
                };

                Vector3 normal = {
                    mesh.normals[i*3+0],
                    mesh.normals[i*3+1],
                    mesh.normals[i*3+2]
                };

                float ax = fabs(normal.x);
                float ay = fabs(normal.y);
                float az = fabs(normal.z);

                float sx = e.scale.x;
                float sy = e.scale.y;
                float sz = e.scale.z;

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

                u *= e.uv_scale_vec.x;
                v *= e.uv_scale_vec.y;
            } else {
                if (m >= e.original_texcoords.size()) continue;
                auto& base = e.original_texcoords[m];

                u = base[i*2+0] * e.texture_repeat_u * e.scale.x;
                v = base[i*2+1] * e.texture_repeat_v * e.scale.y;
            }

            mesh.texcoords[i*2+0] = u;
            mesh.texcoords[i*2+1] = v;
        }

        UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
    }
}

void store_uv(Entity* e) {
    e->original_texcoords.clear();

    for (int m = 0; m < e->model.meshCount; m++) {
        Mesh& mesh = e->model.meshes[m];

        if (!mesh.texcoords) {
            e->original_texcoords.push_back({});
            continue;
        }

        std::vector<float> uv(mesh.vertexCount * 2);
        memcpy(uv.data(), mesh.texcoords, uv.size() * sizeof(float));

        e->original_texcoords.push_back(uv);
    }
}

void draw_entity_with_texture(Entity& e) {
    if (e.texture.id != 0) {
        for (int i = 0; i < e.model.materialCount; i++)
            e.model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = e.texture;

        if (e.texture_stretch) {
            for (int m = 0; m < e.model.meshCount; m++) {
                Mesh& mesh = e.model.meshes[m];
                if (!mesh.texcoords || m >= e.original_texcoords.size()) continue;

                memcpy(mesh.texcoords, e.original_texcoords[m].data(), mesh.vertexCount * 2 * sizeof(float));
                UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
            }
        } 
        
        else {
            apply_texture_repeat(e);
        }
    }

    rlPushMatrix();
    rlTranslatef(e.position.x, e.position.y, e.position.z);

    rlRotatef(e.rotation.x, 1, 0, 0);
    rlRotatef(e.rotation.y, 0, 1, 0);
    rlRotatef(e.rotation.z, 0, 0, 1);
    
    rlScalef(e.scale.x, e.scale.y, e.scale.z);
    DrawModel(e.model, {0,0,0}, 1.0f, e.color);
    DrawModelWires(e.model, {0,0,0}, 1.0f, e.outline_color);
    rlPopMatrix();
}

void refresh_textures(Scene* scene) {
    if (!fs::exists("assets")) fs::create_directories("assets");

    std::unordered_map<std::string, Texture2D> old_by_name;
    for (const auto& opt : texture_options) {
        if (opt.texture.id != 0) {
            old_by_name[opt.name] = opt.texture;
        }
    }

    std::vector<TextureOption> next_options;
    next_options.push_back({ "None", {0} });

    for (auto& entry : fs::directory_iterator("assets")) {
        if (!entry.is_regular_file()) continue;

        fs::path path = entry.path();
        if (!is_image_file(path)) continue;

        const std::string texture_name = path.filename().string();
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
                if (entity.texture.id == removed_tex.id) {
                    entity.texture = {0};
                }
            }
        }
    }

    std::unordered_set<unsigned int> released_ids;
    for (const auto& [_, removed_tex] : old_by_name) {
        if (removed_tex.id == 0) continue;
        if (released_ids.insert(removed_tex.id).second) {
            UnloadTexture(removed_tex);
        }
    }

    texture_options = std::move(next_options);
}
