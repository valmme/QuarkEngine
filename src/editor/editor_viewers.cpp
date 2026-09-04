#include "editor/editor_viewers.h"
#include "editor/editor_utils.h"
#include "language_manager.h"
#include "tex.h"
#include "entity.h"
#include "imgui.h"
#include "qcImGui.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include "editor/editor_assets.h"

#define lang LanguageManager::get()

static bool show_model_viewer = false;
static Model viewer_model;
static RenderTexture2D viewer_rt = { 0 };

static bool show_material_viewer = false;
static int material_preview_primitive = 0;
static Color material_albedo = WHITE;
static float material_albedo_f[4] = {1,1,1,1};
static float material_brightness = 1.0f;
static Texture2D material_texture = {0};
static bool material_texture_owned = false;
static std::string material_texture_path = "";
static Model viewer_mat_sphere;
static RenderTexture2D viewer_mat_rt = { 0 };
static std::filesystem::path current_material_path = "";
static bool show_texture_picker = false;
static std::vector<std::string> texture_files_in_dir;
static std::string selected_texture_preview = "";

static bool material_auto_uv = false;
static bool material_texture_stretch = true;
static float material_texture_repeat_u = 1.0f;
static float material_texture_repeat_v = 1.0f;
static float material_uv_scale_x = 1.0f;
static float material_uv_scale_y = 1.0f;
static Color material_outline_color = LIGHTGRAY;
static float material_outline_color_f[4] = {0.827f, 0.827f, 0.827f, 1.0f};

static Vec3 viewer_target = { 0, 0, 0 };
static Vec3 viewer_model_center = { 0, 0, 0 };
static Vec3 viewer_model_rotation = { 0, 0, 0 };
static float viewer_phi = 20.0f, viewer_theta = 45.0f, viewer_radius = 5.0f;

static void release_model_preview() {
    if (viewer_model.meshCount > 0) {
        UnloadModel(viewer_model);
    } else {
        viewer_model = {};
    }
}

static void release_material_preview_model() {
    if (viewer_mat_sphere.meshCount <= 0) {
        return;
    }

    if (viewer_mat_sphere.materialCount > 0 && viewer_mat_sphere.materials && viewer_mat_sphere.materials[0].maps) {
        viewer_mat_sphere.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = {0};
    }

    UnloadModel(viewer_mat_sphere);
    viewer_mat_sphere = {};
}

bool open_model_viewer_for_asset(const ModelAsset& asset) {
    release_model_preview();

    if (!load_model_instance(asset, viewer_model)) return false;

    show_model_viewer = true;
    viewer_radius = 5.0f;
    viewer_phi = 20.0f;
    viewer_theta = 45.0f;
    viewer_target = { 0, 0, 0 };
    viewer_model_rotation = { 0, 0, 0 };

    const BoundingBox box = GetModelBoundingBox(viewer_model);
    viewer_model_center = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };

    return true;
}

bool open_material_viewer_for_path(const std::filesystem::path& material_path, std::unordered_map<std::string, Texture>& texture_cache) {
    std::ifstream material_file(material_path);
    if (!material_file.is_open()) return false;

    current_material_path = material_path;
    material_texture_path = "";

    if (material_texture_owned && material_texture.id != 0) {
        UnloadTexture(material_texture);
    }
    material_texture = {0};
    material_texture_owned = false;

    release_material_preview_model();

    viewer_mat_sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 64));
    Material& material = viewer_mat_sphere.materials[0];
    material_albedo = WHITE;
    material_albedo_f[0] = 1.0f;
    material_albedo_f[1] = 1.0f;
    material_albedo_f[2] = 1.0f;
    material_albedo_f[3] = 1.0f;
    material_brightness = 1.0f;

    std::string line;
    while (std::getline(material_file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "Kd") {
            float r = 1.0f;
            float g = 1.0f;
            float b = 1.0f;
            if (stream >> r >> g >> b) {
                material.maps[MATERIAL_MAP_DIFFUSE].color = {
                    static_cast<unsigned char>(r * 255),
                    static_cast<unsigned char>(g * 255),
                    static_cast<unsigned char>(b * 255),
                    255
                };
                material_albedo = material.maps[MATERIAL_MAP_DIFFUSE].color;
                material_albedo_f[0] = r;
                material_albedo_f[1] = g;
                material_albedo_f[2] = b;
                material_albedo_f[3] = 1.0f;
            }
        } else if (type == "map_Kd") {
            std::string texture_name;
            if (!(stream >> texture_name)) continue;

            material_texture_path = texture_name;
            const std::filesystem::path texture_path = material_path.parent_path() / texture_name;
            const std::string cache_key = texture_path.string();
            if (!texture_cache.count(cache_key) && std::filesystem::exists(texture_path)) {
                texture_cache[cache_key] = LoadTexture(cache_key.c_str());
            }

            if (texture_cache.count(cache_key)) {
                material.maps[MATERIAL_MAP_DIFFUSE].texture = texture_cache[cache_key];
                material_texture = texture_cache[cache_key];
                material_texture_owned = false;
            }
        }
    }

    rebuild_material_preview_mesh();
    show_material_viewer = true;
    viewer_radius = 2.5f;
    viewer_phi = 20.0f;
    viewer_theta = 45.0f;
    viewer_target = { 0, 0, 0 };
    viewer_model_rotation = { 0, 0, 0 };
    return true;
}

void load_material_to_entity(Entity* entity, const std::filesystem::path& mtl_path, int material_slot) {
    if (!entity || !std::filesystem::exists(mtl_path)) return;

    std::ifstream material_file(mtl_path);
    if (!material_file.is_open()) return;

    MeshComponent* mesh = entity->get_mesh_component();
    MaterialComponent* mat_comp = entity->get_material_component();
    if (!mesh || !mat_comp) return;
    const auto applies_to_slot = [mesh, material_slot](int material_index) {
        return material_slot < 0 || material_index == material_slot;
    };

    Color albedo = WHITE;
    std::string texture_name;
    std::string normal_texture_name;
    std::string roughness_texture_name;
    std::string metallic_texture_name;
    
    std::string line;
    while (std::getline(material_file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "Kd") {
            float r = 1.0f, g = 1.0f, b = 1.0f;
            if (stream >> r >> g >> b) {
                albedo = {
                    static_cast<unsigned char>(r * 255),
                    static_cast<unsigned char>(g * 255),
                    static_cast<unsigned char>(b * 255),
                    255
                };
            }
        } 
        else if (type == "map_Kd") {
            if (!(stream >> texture_name)) texture_name.clear();
        } else if (type == "map_Bump" || type == "bump" || type == "norm") {
            if (!(stream >> normal_texture_name)) normal_texture_name.clear();
        } else if (type == "map_Pr" || type == "map_roughness") {
            if (!(stream >> roughness_texture_name)) roughness_texture_name.clear();
        } else if (type == "map_Pm" || type == "map_metallic") {
            if (!(stream >> metallic_texture_name)) metallic_texture_name.clear();
        }
    }

    material_file.close();

    mat_comp->color = albedo;
    mat_comp->albedo_texture_name.clear();
    mat_comp->normal_texture_name = normal_texture_name;
    mat_comp->roughness_texture_name = roughness_texture_name;
    mat_comp->metallic_texture_name = metallic_texture_name;
    if (mesh->model.materials) {
        for (int i = 0; i < mesh->model.materialCount; i++) {
            if (!applies_to_slot(i)) continue;
            mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = albedo;
        }
    }

    if (!texture_name.empty()) {
        std::filesystem::path texture_path = mtl_path.parent_path() / texture_name;
        if (std::filesystem::exists(texture_path)) {
            Texture2D tex = LoadTexture(texture_path.string().c_str());
            if (tex.id != 0) {
                mat_comp->texture = tex;
                mat_comp->texture_name = texture_name;
                mat_comp->texture_source = TEXTURE_EXTERNAL;
                
                if (mesh->model.materials) {
                    for (int i = 0; i < mesh->model.materialCount; i++) {
                        if (!applies_to_slot(i)) continue;
                        mesh->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
                    }
                }
            }
        }
    }

    const auto load_map = [&](const std::string& path, int map_type) {
        if (path.empty() || !mesh->model.materials) return;
        const std::filesystem::path texture_path = mtl_path.parent_path() / path;
        if (!std::filesystem::exists(texture_path)) return;
        Texture2D texture = LoadTexture(texture_path.string().c_str());
        if (texture.id == 0) return;
        for (int i = 0; i < mesh->model.materialCount; i++) {
            if (!applies_to_slot(i)) continue;
            if (mesh->model.materials[i].maps)
                mesh->model.materials[i].maps[map_type].texture = texture;
        }
    };

    load_map(normal_texture_name, MATERIAL_MAP_NORMAL);
    load_map(roughness_texture_name, MATERIAL_MAP_ROUGHNESS);
    load_map(metallic_texture_name, MATERIAL_MAP_METALNESS);
    
    mat_comp->texture_name = mtl_path.string();
}

std::vector<std::string> get_all_materials_in_project() {
    std::vector<std::string> materials;
    try {
        std::filesystem::path current_path = std::filesystem::current_path();
        
        for (const auto& entry : std::filesystem::recursive_directory_iterator(current_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".mtl") {
                std::filesystem::path rel_path = std::filesystem::relative(entry.path(), current_path);
                materials.push_back(rel_path.generic_string());
            }
        }
        
        std::sort(materials.begin(), materials.end());
    } catch (const std::exception&) {
    }
    
    return materials;
}

bool is_model_viewer_visible() {
    return show_model_viewer;
}

bool is_material_viewer_visible() {
    return show_material_viewer;
}

void show_model_viewer_window(bool show) {
    show_model_viewer = show;
    if (!show) release_model_preview();
}

void show_material_viewer_window(bool show) {
    show_material_viewer = show;
}

void set_model_viewer_model(const Model& model) {
    if (viewer_model.meshCount > 0) {
        UnloadModel(viewer_model);
    }
    viewer_model = model;
}

void apply_material_settings() {
    if (viewer_mat_sphere.meshCount == 0) return;

    Material& mat = viewer_mat_sphere.materials[0];

    Color finalColor = {
        (unsigned char)(material_albedo.r * material_brightness),
        (unsigned char)(material_albedo.g * material_brightness),
        (unsigned char)(material_albedo.b * material_brightness),
        material_albedo.a
    };

    mat.maps[MATERIAL_MAP_DIFFUSE].color = finalColor;

    if (material_texture.id != 0) {
        mat.maps[MATERIAL_MAP_DIFFUSE].texture = material_texture;
    }
}

void rebuild_material_preview_mesh() {
    release_material_preview_model();

    Mesh mesh = {0};

    switch (material_preview_primitive) {
        case 0: mesh = GenMeshSphere(1.0f, 64, 64); break;
        case 1: mesh = GenMeshCube(2.0f, 2.0f, 2.0f); break;
        case 2: mesh = GenMeshPlane(3.0f, 3.0f, 1, 1); break;
    }

    viewer_mat_sphere = LoadModelFromMesh(mesh);
    apply_material_settings();
}

void save_material_to_file(Editor& editor) {
    if (current_material_path.empty()) return;

    std::ofstream material_file(current_material_path);
    if (!material_file.is_open()) return;

    material_file << "# Material exported from QuarkEngine\n";
    material_file << "Kd " << (material_albedo_f[0]) << " " << (material_albedo_f[1]) << " " << (material_albedo_f[2]) << "\n";
    
    if (!material_texture_path.empty()) {
        material_file << "map_Kd " << material_texture_path << "\n";
    }

    material_file.close();
    invalidate_material_previews();

    for (Entity& entity : editor.scene.entities) {
        if (!&entity) continue;

        MaterialComponent* mat = entity.get_material_component();
        if (!mat) continue;

        if (!mat->texture_name.empty() && std::filesystem::absolute(mat->texture_name) == std::filesystem::absolute(current_material_path))
        {
            load_material_to_entity(&entity, current_material_path);
        }
    }
}

void load_textures_in_directory() {
    texture_files_in_dir.clear();
    if (current_material_path.empty()) return;

    const auto material_dir = current_material_path.parent_path();
    if (!std::filesystem::exists(material_dir)) return;

    const std::vector<std::string> image_extensions = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds"};

    try {
        for (const auto& entry : std::filesystem::directory_iterator(material_dir)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                
                if (std::find(image_extensions.begin(), image_extensions.end(), ext) != image_extensions.end()) {
                    texture_files_in_dir.push_back(entry.path().filename().string());
                }
            }
        }
    } catch (...) {}
}

void load_material_texture(const std::string& texture_name) {
    if (current_material_path.empty()) return;

    const auto material_dir = current_material_path.parent_path();
    const auto texture_full_path = material_dir / texture_name;

    if (!std::filesystem::exists(texture_full_path)) return;

    if (material_texture_owned && material_texture.id != 0) {
        UnloadTexture(material_texture);
    }

    material_texture = LoadTexture(texture_full_path.string().c_str());
    material_texture_owned = material_texture.id != 0;
    material_texture_path = texture_name;
    apply_material_settings();
}



void draw_model_viewer_window() {
    if (!show_model_viewer) {
        release_model_preview();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(lang.word("model_preview"), &show_model_viewer)) {
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 1) size.x = 1;
        if (size.y < 1) size.y = 1;

        if (viewer_rt.id == 0 || viewer_rt.texture.width != (int)size.x || viewer_rt.texture.height != (int)size.y) {
            if (viewer_rt.id != 0) UnloadRenderTexture(viewer_rt);
            viewer_rt = LoadRenderTexture((int)size.x, (int)size.y);
        }

        ImVec2 viewport_pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("ModelViewport", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        bool is_hovered = ImGui::IsItemHovered();
        bool is_active = ImGui::IsItemActive();

        if (is_hovered) {
            viewer_radius -= ImGui::GetIO().MouseWheel * 1.5f;
            if (viewer_radius < 0.1f) viewer_radius = 0.1f;
        }

        if (is_active) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                viewer_model_rotation.y += delta.x * 0.5f;
                viewer_model_rotation.x += delta.y * 0.5f;
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                viewer_theta -= delta.x * 0.5f; 
                viewer_phi -= delta.y * 0.5f;
            }

            if (viewer_phi > 89.0f) viewer_phi = 89.0f;
            if (viewer_phi < -89.0f) viewer_phi = -89.0f;
        }

        Camera3D cam;
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        cam.target = viewer_target;
        cam.up = { 0, 1, 0 };
        cam.position.x = viewer_target.x + viewer_radius * cosf(viewer_phi * DEG2RAD) * sinf(viewer_theta * DEG2RAD);
        cam.position.y = viewer_target.y + viewer_radius * sinf(viewer_phi * DEG2RAD);
        cam.position.z = viewer_target.z + viewer_radius * cosf(viewer_phi * DEG2RAD) * cosf(viewer_theta * DEG2RAD);

        BeginTextureMode(viewer_rt);
        ClearBackground({ 40, 40, 45, 255 });
        BeginMode3D(cam);
        if (viewer_model.meshCount > 0) {
            Mat4 matCenter = Mat4::translation(
                -viewer_model_center.x,
                -viewer_model_center.y, 
                -viewer_model_center.z
            );

            Mat4 matRotation =
                Mat4::rotationX(viewer_model_rotation.x * DEG2RAD) *
                Mat4::rotationY(viewer_model_rotation.y * DEG2RAD);

            viewer_model.transform = matCenter * matRotation;

            DrawModel(viewer_model, { 0, 0, 0 }, 1.0f, WHITE);
            DrawModelWires(viewer_model, { 0, 0, 0 }, 1.0f, DARKGRAY);
        }
        DrawGrid(10, 1.0f);
        EndMode3D();
        EndTextureMode();

        ImGui::SetCursorScreenPos(viewport_pos);
        Rectangle src = { 0, 0, (float)viewer_rt.texture.width, -(float)viewer_rt.texture.height };
        qcImGuiImageRect(&viewer_rt.texture, (int)size.x, (int)size.y, src);
    }
    ImGui::End();
}

void draw_material_viewer_window(Editor& editor, Entity* selected_entity) {
    if (!show_material_viewer) {
        material_preview_primitive = 0;

        release_material_preview_model();
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(lang.word("material_editor"), &show_material_viewer)) {

        ImGui::Columns(2, nullptr, true);

        ImGui::BeginChild("MaterialSettings");

        ImGui::Text(lang.word("material"));

        if (ImGui::ColorEdit4(lang.word("albedo"), material_albedo_f)) {
            material_albedo = {
                (unsigned char)(material_albedo_f[0] * 255),
                (unsigned char)(material_albedo_f[1] * 255),
                (unsigned char)(material_albedo_f[2] * 255),
                (unsigned char)(material_albedo_f[3] * 255)
            };

            apply_material_settings();
        }

        if (ImGui::SliderFloat(lang.word("brightness"), &material_brightness, 0.1f, 2.0f)) {
            apply_material_settings();
        }

        ImGui::Separator();

        ImGui::Text(lang.word("texture"));
        ImGui::Text("%s: %s", lang.word("current"), material_texture_path.empty() ? lang.word("none") : material_texture_path.c_str());

        if (ImGui::Button(lang.word("select_texture"))) {
            load_textures_in_directory();
            show_texture_picker = !show_texture_picker;
        }

        ImGui::Separator();

        ImGui::Text(lang.word("uv_settings"));

        if (ImGui::Checkbox(lang.word("stretch_texture"), &material_texture_stretch)) {
        }

        if (!material_texture_stretch) {
            if (ImGui::SliderFloat(lang.word("repeat_u"), &material_texture_repeat_u, 0.1f, 10.0f)) {
            }

            if (ImGui::SliderFloat(lang.word("repeat_v"), &material_texture_repeat_v, 0.1f, 10.0f)) {
            }

            if (ImGui::SliderFloat(lang.word("uv_scale_x"), &material_uv_scale_x, 0.1f, 5.0f)) {
            }

            if (ImGui::SliderFloat(lang.word("uv_scale_y"), &material_uv_scale_y, 0.1f, 5.0f)) {
            }
        }

        ImGui::Separator();

        if (ImGui::ColorEdit4(lang.word("outline_color"), material_outline_color_f)) {
            material_outline_color = {
                (unsigned char)(material_outline_color_f[0] * 255),
                (unsigned char)(material_outline_color_f[1] * 255),
                (unsigned char)(material_outline_color_f[2] * 255),
                (unsigned char)(material_outline_color_f[3] * 255)
            };
        }

        ImGui::Separator();

        ImGui::Text(lang.word("primitive"));

        const char* primitives[] = { lang.word("sphere"), lang.word("cube"), lang.word("plane") };
        if (ImGui::Combo(lang.word("mesh"), &material_preview_primitive, primitives, 3)) {
            rebuild_material_preview_mesh();
        }

        ImGui::Separator();

        if (ImGui::Button(lang.word("save_material"), ImVec2(-1, 0))) {
            save_material_to_file(editor);
        }

        ImGui::EndChild();

        ImGui::NextColumn();

        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 1) size.x = 1;
        if (size.y < 1) size.y = 1;

        if (viewer_mat_rt.id == 0 ||
            viewer_mat_rt.texture.width != (int)size.x ||
            viewer_mat_rt.texture.height != (int)size.y) {

            if (viewer_mat_rt.id != 0) UnloadRenderTexture(viewer_mat_rt);
            viewer_mat_rt = LoadRenderTexture((int)size.x, (int)size.y);
        }

        ImVec2 viewport_pos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("Viewport", size,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        if (ImGui::IsItemHovered()) {
            viewer_radius -= ImGui::GetIO().MouseWheel * 0.5f;
            if (viewer_radius < 0.1f) viewer_radius = 0.1f;
        }

        if (ImGui::IsItemActive()) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;

            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                viewer_model_rotation.y += delta.x * 0.5f;
                viewer_model_rotation.x += delta.y * 0.5f;
            }

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                viewer_theta -= delta.x * 0.5f; 
                viewer_phi -= delta.y * 0.5f;
            }

            viewer_phi = Clamp(viewer_phi, -89.0f, 89.0f);
        }

        Camera3D cam;
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        cam.target = viewer_target;
        cam.up = { 0, 1, 0 };

        cam.position.x = viewer_target.x + viewer_radius * cosf(viewer_phi * DEG2RAD) * sinf(viewer_theta * DEG2RAD);
        cam.position.y = viewer_target.y + viewer_radius * sinf(viewer_phi * DEG2RAD);
        cam.position.z = viewer_target.z + viewer_radius * cosf(viewer_phi * DEG2RAD) * cosf(viewer_theta * DEG2RAD);

        BeginTextureMode(viewer_mat_rt);
        ClearBackground({ 40, 40, 45, 255 });

        BeginMode3D(cam);

        if (viewer_mat_sphere.meshCount > 0) {
            Mat4 rot =
                Mat4::rotationX(viewer_model_rotation.x * DEG2RAD) *
                Mat4::rotationY(viewer_model_rotation.y * DEG2RAD);

            viewer_mat_sphere.transform = rot;
            DrawModel(viewer_mat_sphere, {0,0,0}, 1.0f, WHITE);
        }

        EndMode3D();
        EndTextureMode();

        ImGui::SetCursorScreenPos(viewport_pos);

        Rectangle src = {
            0, 0,
            (float)viewer_mat_rt.texture.width,
            -(float)viewer_mat_rt.texture.height
        };

        qcImGuiImageRect(&viewer_mat_rt.texture, (int)size.x, (int)size.y, src);

        ImGui::Columns(1);
    }

    ImGui::End();

    if (show_texture_picker) {
        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(lang.word("select_texture"), &show_texture_picker)) {
            ImGui::Text(lang.word("textures_in_directory"));
            ImGui::Separator();
            
            for (const auto& texture_name : texture_files_in_dir) {
                if (ImGui::Selectable(texture_name.c_str(), selected_texture_preview == texture_name)) {
                    selected_texture_preview = texture_name;
                    load_material_texture(texture_name);
                    show_texture_picker = false;
                }
            }

            if (texture_files_in_dir.empty()) {
                ImGui::TextDisabled(lang.word("no_textures_found"));
            }

            ImGui::End();
        }
    }
}

void cleanup_viewers() {
    release_model_preview();

    if (viewer_rt.id != 0) {
        UnloadRenderTexture(viewer_rt);
        viewer_rt = { 0 };
    }

    release_material_preview_model();

    if (viewer_mat_rt.id != 0) {
        UnloadRenderTexture(viewer_mat_rt);
        viewer_mat_rt = { 0 };
    }

    if (material_texture_owned && material_texture.id != 0) {
        UnloadTexture(material_texture);
    }
    material_texture = {0};
    material_texture_owned = false;
}
