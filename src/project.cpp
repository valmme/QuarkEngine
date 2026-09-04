#include "project.h"
#include "entity.h"
#include "component.h"
#include "lighting.h"
#include "models.h"
#include "tex.h"
#include "version.h"
#include "editor.h"
#include "QuarkCore/QuarkCore.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>
#include "editor/editor_viewers.h"

using namespace qc;

namespace fs = std::filesystem;
using json = nlohmann::json;

static const char* QUARK_PROJECT_EXTENSION = ".quarkproj";

static std::string color_to_hex(Color color) {
    char buf[12];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return buf;
}

static Color hex_to_color(const std::string& hex) {
    Color color = WHITE;
    if (hex.size() < 7) return color;

    color.r = (unsigned char)strtol(hex.substr(1, 2).c_str(), nullptr, 16);
    color.g = (unsigned char)strtol(hex.substr(3, 2).c_str(), nullptr, 16);
    color.b = (unsigned char)strtol(hex.substr(5, 2).c_str(), nullptr, 16);
    color.a = hex.size() >= 9 ? (unsigned char)strtol(hex.substr(7, 2).c_str(), nullptr, 16) : 255;
    return color;
}

static fs::path project_manifest_path_for_root(const fs::path& root_path) {
    const std::string root_name = root_path.filename().string();
    const std::string manifest_name = (root_name.empty() ? std::string("project") : root_name) + QUARK_PROJECT_EXTENSION;
    return root_path / manifest_name;
}

static fs::path find_project_manifest(const fs::path& root_path) {
    std::error_code ec;

    if (!fs::exists(root_path, ec) || !fs::is_directory(root_path, ec))
        return {};

    const fs::path preferred = project_manifest_path_for_root(root_path);
    if (fs::exists(preferred)) return preferred;

    for (const auto& entry : fs::directory_iterator(root_path, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == QUARK_PROJECT_EXTENSION)
            return entry.path();
    }

    return {};
}

static fs::path resolve_project_root_path(const fs::path& input_path) {
    if (input_path.empty()) return {};

    fs::path p = fs::absolute(input_path);

    std::error_code ec;

    if (fs::is_directory(p, ec)) {
        return p;
    }

    if (fs::is_regular_file(p, ec)) {
        if (p.extension() == QUARK_PROJECT_EXTENSION) {
            return p.parent_path();
        }

        return p.parent_path();
    }

    return p;
}

static bool write_project_manifest(const fs::path& root_path) {
    fs::create_directories(root_path);

    json manifest;
    manifest["format"] = "quark-project";
    manifest["version"] = QUARK_ENGINE_VERSION;
    manifest["name"] = root_path.filename().string();
    manifest["scene"] = "scene.json";
    manifest["resources"] = "resources";

    const fs::path manifest_path = project_manifest_path_for_root(root_path);
    std::ofstream manifest_file(manifest_path);
    if (!manifest_file.is_open()) return false;
    manifest_file << manifest.dump(4);
    return true;
}

static fs::path resolve_scene_json_path(const fs::path& input_path) {
    const fs::path root_path = resolve_project_root_path(input_path);
    if (root_path.empty()) return {};

    if (fs::is_regular_file(input_path) && input_path.extension() == QUARK_PROJECT_EXTENSION) {
        std::ifstream manifest_file(input_path);
        if (manifest_file.is_open()) {
            json manifest;
            try {
                manifest_file >> manifest;
                if (manifest.contains("scene") && manifest["scene"].is_string()) {
                    return root_path / manifest["scene"].get<std::string>();
                }
            } catch (...) {}
        }
    }

    const fs::path manifest_path = find_project_manifest(root_path);
    if (!manifest_path.empty()) {
        std::ifstream manifest_file(manifest_path);
        if (manifest_file.is_open()) {
            json manifest;
            try {
                manifest_file >> manifest;
                if (manifest.contains("scene") && manifest["scene"].is_string()) {
                    return root_path / manifest["scene"].get<std::string>();
                }
            } catch (...) {}
        }
    }

    return root_path / "scene.json";
}

std::string project_resolve_root(const std::string& path) {
    const fs::path root_path = resolve_project_root_path(fs::path(path));
    if (root_path.empty()) return {};
    return fs::absolute(root_path).string();
}

bool project_is_valid(const std::string& path) {
    const fs::path input_path(path);
    const fs::path root_path = resolve_project_root_path(input_path);
    if (root_path.empty() || !fs::exists(root_path)) return false;
    return fs::exists(resolve_scene_json_path(input_path));
}

void project_new(const std::string& folder_path, Scene& scene) {
    const fs::path root_path = resolve_project_root_path(fs::path(folder_path));
    fs::create_directories(root_path);
    fs::create_directories(root_path / "resources");

    json j;
    j["entities"] = json::array();
    
    std::ofstream f(root_path / "scene.json");
    f << j.dump(4);
    write_project_manifest(root_path);

    scene.release_resources();
}

void project_save(const std::string& folder_path, const Scene& scene) {
    const fs::path root_path = resolve_project_root_path(fs::path(folder_path));
    fs::create_directories(root_path / "resources");

    json j;
    j["entities"] = json::array();
    j["version"] = QUARK_ENGINE_VERSION;

    for (const auto& e : scene.entities) {
        json ej;
        ej["name"] = e.name;
        ej["tags"] = e.tags;
        ej["is_group"] = e.is_group;
        ej["parent_id"] = e.parent_id;

        if (e.components) {
            e.components->serialize(ej);
        }

        j["entities"].push_back(ej);
    }

    std::ofstream f(root_path / "scene.json");
    if (!f.is_open()) {
        TraceLog(LogLevel::Error, "PROJECT", TextFormat("Failed to open scene.json for writing: %s", folder_path.c_str()));
        return;
    }
    f << j.dump(4);
    f.close();
    write_project_manifest(root_path);
}

bool project_load(const std::string& folder_path, Scene& scene, Shader shader) {
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("project_load called: %s", folder_path.c_str()));
    const fs::path root_path = resolve_project_root_path(fs::path(folder_path));
    const fs::path json_path = resolve_scene_json_path(fs::path(folder_path));
    if (!fs::exists(json_path)) return false;
    const fs::path manifest_path = find_project_manifest(root_path);
    std::string project_name = root_path.filename().string();
    std::string project_version = "unknown";

    if (!manifest_path.empty()) {
        std::ifstream mf(manifest_path);
        if (mf.is_open()) {
            json manifest;
            try {
                mf >> manifest;
                if (manifest.contains("name") && manifest["name"].is_string())
                    project_name = manifest["name"].get<std::string>();
                if (manifest.contains("version") && manifest["version"].is_string())
                    project_version = manifest["version"].get<std::string>();
            } catch (...) {}
        }
    }

    TraceLog(LogLevel::Info, "PROJECT", "=== Loading project ===");
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Name:    %s", project_name.c_str()));
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Version: %s", project_version.c_str()));
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Scene:   %s", json_path.string().c_str()));
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Engine:  %s", QUARK_ENGINE_VERSION));
    TraceLog(LogLevel::Info, "PROJECT", TextFormat("  Root:    %s", root_path.string().c_str()));
    TraceLog(LogLevel::Info, "PROJECT", "======================");

    scene.release_resources();
    reset_light_registry();
    refresh_textures(nullptr, root_path.string());
    refresh_assets(root_path.string());
    refresh_models(root_path.string(), scene);

    std::ifstream f(json_path);
    json j;
    f >> j;

    for (auto& ej : j["entities"]) {
        Entity e;
        e.name = ej["name"].get<std::string>();
        e.id   = static_cast<int>(scene.entities.size());

        if (ej.contains("tags") && ej["tags"].is_array()) {
            e.tags = ej["tags"].get<std::vector<std::string>>();
        }
        
        if (ej.contains("is_group")) {
            e.is_group = ej["is_group"].get<bool>();
        }
        if (ej.contains("parent_id")) {
            e.parent_id = ej["parent_id"].get<int>();
        }

        if (!ej.contains("components")) {
            TraceLog(LogLevel::Warn, "PROJECT", TextFormat("Skipping legacy entity '%s': project must use component data only.", e.name.c_str()));
            continue;
        }

        e.components->deserialize(ej);

        if (e.is_group) {
            scene.entities.push_back(std::move(e));
            continue;
        }

        if (!e.components->get_transform()) e.components->add_component(std::make_shared<TransformComponent>());
        if (!e.components->get_mesh()) e.components->add_component(std::make_shared<MeshComponent>());
        if (!e.components->get_material()) e.components->add_component(std::make_shared<MaterialComponent>());

        MeshComponent* mesh = e.get_mesh_component();
        TransformComponent* transform = e.get_transform_component();
        MaterialComponent* mat = e.get_material_component();
        LightComponent* light = e.get_light_component();
        if (!mesh || !transform) continue;

        if (!mesh->asset_name.empty()) {
            for (auto& a : assets) {
                if (a.name != mesh->asset_name) continue;

                mesh->asset = &a;
                mesh->type = a.type;
                if (a.is_procedural) {
                    mesh->model = a.generator(mesh->segments);
                    mesh->owns_model_instance = true;
                } 
                
                else {
                    if (!load_model_instance(a, mesh->model)) {
                        mesh->asset = nullptr;
                        mesh->asset_name.clear();
                        mesh->model;
                        mesh->owns_model_instance = false;
                        break;
                    }
                    mesh->owns_model_instance = true;
                }

                store_uv(&e);
                store_material_textures(&e);
                apply_mesh_overrides(e);

                if (mat && !mat->albedo_texture_name.empty()) {
                    for (const auto& option : texture_options) {
                        if (option.name == mat->albedo_texture_name) {
                            mat->texture = option.texture;
                            break;
                        }
                    }
                    mat->texture_source = TEXTURE_EXTERNAL;
                }

                else if (mat && !mat->texture_name.empty()) {
                    load_material_to_entity(&e, mat->texture_name);
                    mat->texture_source = TEXTURE_EXTERNAL;
                }

                else if (mat && mat->texture_source == TEXTURE_MODEL) {
                    restore_model_textures(&e);
                }

                else {
                    clear_material_textures(&e);
                }

                break;
            }
        }

        if (!mesh->asset || (mesh->asset_name.size() > 0 && (mesh->model.meshCount <= 0 || mesh->model.meshes == nullptr))) {
            continue;
        }

        if (mesh->is_editable_mesh) {
            rebuild_mesh_from_editable(mesh->model, mesh->editable_mesh);
        }

        for (int material_slot = 0;
             material_slot < static_cast<int>(mat ? mat->material_slot_sources.size() : 0);
             ++material_slot) {
            const std::string& source = mat->material_slot_sources[material_slot];
            if (!source.empty() && fs::exists(source))
                load_material_to_entity(&e, source, material_slot);
        }

        for (int i = 0; i < mesh->model.materialCount; i++) {
            mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            if (mat->texture.id != 0) {
                mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = mat->texture;
            }
            mesh->model.materials[i].shader = &shader;
        }

        mesh->shader_assigned = true;

        if (light) {
            const int light_type = light->light.light.type;
            light->created = false;
            light->light.id = -1;
            light->light.light = {};
            light->light.light.type = light_type;
            if (light->enabled) {
                light->light.enabled = true;
                if (light->light.position.x == 0.0f && light->light.position.y == 0.0f && light->light.position.z == 0.0f) {
                    light->light.position = transform->position;
                }
            }
        }

        scene.entities.push_back(std::move(e));
    }

    return true;
}

std::string get_project_version(const std::string& path) {
    const fs::path root_path = resolve_project_root_path(fs::path(path));
    const fs::path manifest_path = find_project_manifest(root_path);
    if (manifest_path.empty()) return "";

    std::ifstream f(manifest_path);
    if (!f.is_open()) return "";

    json manifest;
    try {
        f >> manifest;
        if (manifest.contains("version")) {
            if (manifest["version"].is_string())
                return manifest["version"].get<std::string>();
            return manifest["version"].dump();
        }
    } catch (...) {}
    return "";
}
