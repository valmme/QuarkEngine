#include "editor/editor_assets.h"

#include "editor/editor.h"
#include "editor/editor_entity.h"
#include "editor/editor_utils.h"
#include "editor/editor_viewers.h"
#include "qcImGui.h"
#include "project.h"
#include "tex.h"
#include "language_manager.h"
#include "editor/editor_preferences.h"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#define lang LanguageManager::get()

namespace fs = std::filesystem;

extern bool show_assets;

namespace {

std::unordered_map<std::string, Texture> model_preview_cache;
std::unordered_map<std::string, RenderTexture2D> model_render_cache;
static std::unordered_map<std::string, Texture> material_preview_cache;
static std::unordered_map<std::string, RenderTexture2D> material_render_cache;
Texture icon_file_tex = {0};
Texture icon_folder_tex = {0};
Texture icon_full_folder_tex = {0};

constexpr float kIconSize = 64.0f;

void open_in_system_file_explorer(const fs::path& path, bool is_directory) {
#ifdef _WIN32
    if (is_directory) {
        ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, 1);
    } else {
        const std::wstring arguments = L"/select,\"" + path.wstring() + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", arguments.c_str(), nullptr, 1);
    }
#elif defined(__APPLE__)
    const std::string command = "open \"" + path.string() + "\"";
    std::system(command.c_str());
#elif defined(__linux__)
    const std::string command = "xdg-open \"" + path.string() + "\"";
    std::system(command.c_str());
#endif
}

}

ModelAsset* find_asset_by_name(const std::string& asset_name) {
    for (auto& asset : assets) {
        if (asset.name == asset_name) return &asset;
    }
    return nullptr;
}

ModelAsset* find_asset_by_path(const fs::path& full_path, const fs::path& project_path_value) {
    return find_asset_by_name(get_asset_name_for_path(project_path_value, full_path));
}

std::string build_resource_signature(const fs::path& resource_dir) {
    std::vector<std::string> entries;
    std::error_code ec;
    fs::recursive_directory_iterator iterator(resource_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return {};

    for (const auto& entry : iterator) {
        std::error_code entry_ec;
        const fs::path relative = fs::relative(entry.path(), resource_dir, entry_ec);
        if (entry_ec) continue;

        std::string row = relative.generic_string();
        if (entry.is_regular_file(entry_ec) && !entry_ec) {
            std::error_code size_ec;
            std::error_code time_ec;
            row += "|f|";
            row += size_ec ? "0" : std::to_string(fs::file_size(entry.path(), size_ec));
            row += "|";
            row += time_ec ? "0" : std::to_string(static_cast<long long>(fs::last_write_time(entry.path(), time_ec).time_since_epoch().count()));
        } else {
            row += "|d";
        }

        entries.push_back(std::move(row));
    }

    std::sort(entries.begin(), entries.end());

    std::string signature;
    for (const auto& entry : entries) {
        signature += entry;
        signature.push_back('\n');
    }

    return signature;
}

Texture create_model_preview(const ModelAsset& asset, const std::string& cache_key, int preview_size) {
    Texture result = {0};

    Model preview_model;
    if (!load_model_instance(asset, preview_model)) return result;
    if (!has_valid_model_data(preview_model)) {
        UnloadModel(preview_model);
        return result;
    }

    RenderTexture2D render_texture = LoadRenderTexture(preview_size, preview_size);
    if (render_texture.id == 0) {
        UnloadModel(preview_model);
        return result;
    }

    Vec3 min_bound = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vec3 max_bound = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool has_vertices = false;

    for (int mesh_index = 0; mesh_index < preview_model.meshCount; mesh_index++) {
        Mesh& mesh = preview_model.meshes[mesh_index];
        if (!mesh.vertices) continue;

        for (int vertex_index = 0; vertex_index < mesh.vertexCount; vertex_index++) {
            const float vx = mesh.vertices[vertex_index * 3 + 0];
            const float vy = mesh.vertices[vertex_index * 3 + 1];
            const float vz = mesh.vertices[vertex_index * 3 + 2];

            min_bound.x = fminf(min_bound.x, vx);
            min_bound.y = fminf(min_bound.y, vy);
            min_bound.z = fminf(min_bound.z, vz);
            max_bound.x = fmaxf(max_bound.x, vx);
            max_bound.y = fmaxf(max_bound.y, vy);
            max_bound.z = fmaxf(max_bound.z, vz);
            has_vertices = true;
        }
    }

    Vec3 center = {0, 0, 0};
    float distance = 3.0f;
    if (has_vertices) {
        center = {
            (min_bound.x + max_bound.x) * 0.5f,
            (min_bound.y + max_bound.y) * 0.5f,
            (min_bound.z + max_bound.z) * 0.5f
        };

        const Vec3 size = {
            max_bound.x - min_bound.x,
            max_bound.y - min_bound.y,
            max_bound.z - min_bound.z
        };

        float max_size = fmaxf(fmaxf(size.x, size.y), size.z);
        if (max_size < 0.1f) max_size = 1.0f;
        distance = max_size * 2.0f;
    }

    Camera3D camera = {};
    camera.position = { center.x + distance * 0.6f, center.y + distance * 0.5f, center.z + distance * 0.6f };
    camera.target = center;
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    BeginTextureMode(render_texture);
    ClearBackground({ 32, 32, 40, 255 });
    BeginMode3D(camera);
    DrawModel(preview_model, { 0, 0, 0 }, 1.0f, WHITE);
    EndMode3D();
    EndTextureMode();

    result = render_texture.texture;
    model_render_cache[cache_key] = render_texture;
    UnloadModel(preview_model);
    return result;
}

bool import_path_to_resources(const fs::path& src, const fs::path& resource_dir) {
    std::error_code ec;

    if (!fs::exists(src, ec) || ec) {
        TraceLog(LogLevel::Warn, "ASSETS", TextFormat("Dropped path does not exist: %s", src.string().c_str()));
        return false;
    }

    if (fs::is_regular_file(src, ec)) {
        fs::copy_file(src, resource_dir / src.filename(), fs::copy_options::overwrite_existing, ec);
        if (ec) {
            TraceLog(LogLevel::Warn, "ASSETS", TextFormat("Failed to import file: %s", src.string().c_str()));
            return false;
        }
        return true;
    }

    if (fs::is_directory(src, ec)) {
        bool imported_any = false;
        const fs::path dst_root = resource_dir / src.filename();
        fs::create_directories(dst_root, ec);
        ec.clear();

        fs::recursive_directory_iterator iterator(src, fs::directory_options::skip_permission_denied, ec);
        if (ec) {
            TraceLog(LogLevel::Warn, "ASSETS", TextFormat("Failed to open dropped directory: %s", src.string().c_str()));
            return false;
        }

        for (const auto& entry : iterator) {
            if (!entry.is_regular_file(ec) || ec) {
                ec.clear();
                continue;
            }

            const fs::path relative = fs::relative(entry.path(), src, ec);
            if (ec) {
                ec.clear();
                continue;
            }

            const fs::path dst = dst_root / relative;
            fs::create_directories(dst.parent_path(), ec);
            if (ec) {
                ec.clear();
                continue;
            }

            fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                ec.clear();
                continue;
            }

            imported_any = true;
        }

        return imported_any;
    }

    TraceLog(LogLevel::Warn, "ASSETS", TextFormat("Unsupported dropped path: %s", src.string().c_str()));
    return false;
}

Texture create_material_preview(const std::string& mtl_path)
{
    if (material_preview_cache.count(mtl_path))
        return material_preview_cache[mtl_path];

    std::ifstream file(mtl_path);
    if (!file.is_open()) return {0};

    Model sphere = LoadModelFromMesh(GenMeshSphere(1.0f, 64, 64));

    Color albedo = WHITE;
    float brightness = 1.0f;
    Texture2D tex = {0};
    std::string tex_path;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "Kd")
        {
            float r=1,g=1,b=1;
            ss >> r >> g >> b;
            albedo = {
                (unsigned char)(r * 255),
                (unsigned char)(g * 255),
                (unsigned char)(b * 255),
                255
            };
        }
        else if (type == "map_Kd")
        {
            ss >> tex_path;
        }
    }

    Material& mat = sphere.materials[0];

    Color finalColor = {
        (unsigned char)(albedo.r * brightness),
        (unsigned char)(albedo.g * brightness),
        (unsigned char)(albedo.b * brightness),
        255
    };

    mat.maps[MATERIAL_MAP_DIFFUSE].color = finalColor;

    if (!tex_path.empty())
    {
        std::filesystem::path full = std::filesystem::path(mtl_path).parent_path() / tex_path;
        if (std::filesystem::exists(full))
        {
            tex = LoadTexture(full.string().c_str());
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
        }
    }

    RenderTexture2D rt = LoadRenderTexture(128, 128);
    Camera3D cam;
    cam.fovy = 45;
    cam.projection = CAMERA_PERSPECTIVE;
    cam.target = {0,0,0};
    cam.up = {0,1,0};
    cam.position = {2,2,2};

    BeginTextureMode(rt);
    ClearBackground({40,40,45,255});
    BeginMode3D(cam);

    DrawModel(sphere, {0,0,0}, 1.0f, WHITE);
    DrawModelWires(sphere, {0,0,0}, 1.0f, DARKGRAY);

    EndMode3D();
    EndTextureMode();

    Texture result = rt.texture;

    material_preview_cache[mtl_path] = result;
    material_render_cache[mtl_path] = rt;

    return result;
}

void draw_assets_ui(Editor& editor) {
    ImGui::Begin(lang.word("assets"), &show_assets, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    if (icon_file_tex.id == 0) icon_file_tex = LoadTexture("assets/file.png");
    if (icon_folder_tex.id == 0) icon_folder_tex = LoadTexture("assets/folder.png");
    if (icon_full_folder_tex.id == 0) icon_full_folder_tex = LoadTexture("assets/full_folder.png");

    std::string popup_name = std::string(lang.word("create_new")) + "##CreateNew";

    static ImVec2 selection_start = {};
    static ImVec2 selection_end = {};
    static bool selecting = false;
    static int rename_target = -1;
    static fs::path dragged_file_path;
    static std::string dragged_file_name;
    static ImVec2 drag_start_pos;

    static bool show_create_popup = false;
    static bool show_duplicate_popup = false;
    static std::string duplicate_name;

    static bool creating_folder = false;
    static char new_item_name[128] = "";

    if (editor.current_asset_path.empty() && !editor.project_path.empty()) {
        editor.current_asset_path = fs::path(editor.project_path) / "resources";
        std::error_code ec;
        fs::create_directories(editor.current_asset_path, ec);
    }

    const ImVec2 window_size = ImGui::GetWindowSize();
    const fs::path project_root = fs::path(editor.project_path);
    const fs::path relative_path = fs::relative(editor.current_asset_path, project_root.parent_path());

    std::vector<fs::path> crumbs;
    for (const auto& part : relative_path) crumbs.push_back(part);

    fs::path rebuilt = project_root.parent_path();
    for (int index = 0; index < static_cast<int>(crumbs.size()); index++) {
        rebuilt /= crumbs[index];
        const std::string label = crumbs[index].string() + "/";

        ImGui::PushID(index);

        const ImVec2 btn_min = ImGui::GetCursorScreenPos();
        bool clicked = ImGui::SmallButton(label.c_str());
        const ImVec2 btn_max = ImVec2(
            btn_min.x + ImGui::GetItemRectSize().x,
            btn_min.y + ImGui::GetItemRectSize().y
        );

        ImGui::PopID();

        if (editor_internal::file_dragging && ImGui::IsMouseHoveringRect(btn_min, btn_max)) {
            ImGui::GetWindowDrawList()->AddRectFilled(
                btn_min,
                btn_max,
                IM_COL32(100, 200, 100, 120)
            );

            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        if (editor_internal::file_dragging && ImGui::IsMouseReleased(0) && ImGui::IsMouseHoveringRect(btn_min, btn_max) && !dragged_file_path.empty())
        {
            fs::path dest = rebuilt / dragged_file_path.filename();

            if (fs::exists(dest)) {
                show_duplicate_popup = true;
                duplicate_name = dragged_file_path.filename().string();
            } else {
                std::error_code ec;
                fs::rename(dragged_file_path, dest, ec);

                if (!ec) {
                    if (is_image_file(dragged_file_path) && fs::exists(dragged_file_path.string() + ".meta")) {
                        fs::rename(dragged_file_path.string() + ".meta", dest.string() + ".meta", ec);
                    }
                    refresh_assets(editor.project_path);
                    refresh_textures(&editor.scene, editor.project_path);
                    refresh_models(editor.project_path, editor.scene);
                    editor.selected_asset_index = -1;
                }
            }

            editor_internal::file_dragging = false;
            dragged_file_path.clear();
            editor_internal::dragged_file_index = -1;
        }

        if (clicked) {
            editor.current_asset_path = rebuilt;
            editor.selected_asset_index = -1;

            editor_internal::tex_cache.clear();
            model_preview_cache.clear();
            for (auto& pair : model_render_cache) UnloadRenderTexture(pair.second);
            model_render_cache.clear();
        }
        ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::Separator();

    std::vector<LocalEntry> directories;
    std::vector<LocalEntry> files;
    std::error_code dir_error;
    for (const auto& path : fs::directory_iterator(editor.current_asset_path, dir_error)) {
        LocalEntry entry = {};
        entry.filename = path.path().filename().string();
        entry.is_directory = path.is_directory();
        entry.is_image = is_image_file(path.path());
        entry.is_model = is_model_file(path.path());

        std::string ext = path.path().extension().string();
        if (!ext.empty() && ext[0] == '.') ext.erase(ext.begin());
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        entry.extension = ext;
        entry.is_material = entry.extension == "mtl";
        if (!entry.is_directory && entry.extension == "meta") continue;
        entry.is_texture_meta = entry.extension == "meta" && is_image_file(path.path().stem());
        if (entry.is_texture_meta && !fs::exists(path.path().parent_path() / path.path().stem())) continue;

        if (entry.is_directory) {
            directories.push_back(entry);
        } else {
            const bool matches_filter = g_editor_preferences.asset_filter == 0 ||
                (g_editor_preferences.asset_filter == 1 && (entry.is_image || entry.is_texture_meta)) ||
                (g_editor_preferences.asset_filter == 2 && entry.is_model) ||
                (g_editor_preferences.asset_filter == 3 && entry.is_material);
            if (!matches_filter) continue;
            files.push_back(entry);
        }
    }

    std::vector<LocalEntry> entries;
    entries.insert(entries.end(), directories.begin(), directories.end());
    entries.insert(entries.end(), files.begin(), files.end());

    if (entries.empty()) {
        ImGui::BeginChild("AssetScrollEmpty", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x < 1.0f) avail.x = 1.0f;
        if (avail.y < 1.0f) avail.y = 1.0f;

        ImGui::InvisibleButton("##empty_drop_zone", avail);
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_INDEX")) {
                int idx = *(const int*)payload->Data;
                Entity e = editor.scene.entities[idx];

                make_prefab(e, editor.current_asset_path);
            }

            ImGui::EndDragDropTarget();
        }

        const char* text = lang.word("empty_folder");
        const ImVec2 ts   = ImGui::CalcTextSize(text);
        const ImVec2 wp   = ImGui::GetWindowPos();
        const ImVec2 ws   = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(wp.x + (ws.x - ts.x) * 0.5f, wp.y + (ws.y - ts.y) * 0.5f),
            IM_COL32(255, 255, 255, 255), 
            text
        );

        if (ImGui::BeginPopupContextItem("AssetBgContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem(lang.word("new_file"))) {
                creating_folder = false;
                new_item_name[0] = '\0';
                show_create_popup = true;
            }

            if (ImGui::MenuItem(lang.word("new_folder"))) {
                creating_folder = true;
                new_item_name[0] = '\0';
                show_create_popup = true;
            }

            ImGui::EndPopup();
        }

        ImGui::EndChild();
    }

    ImGui::BeginChild("AssetScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    if (ImGui::IsWindowHovered() && !editor_internal::file_dragging) {
        if (ImGui::IsMouseClicked(0)) {
            selection_start = ImGui::GetMousePos();
            selection_end = selection_start;
            selecting = true;
        }
        if (ImGui::IsMouseDown(0) && selecting) selection_end = ImGui::GetMousePos();
        if (ImGui::IsMouseReleased(0)) selecting = false;
    }

    const float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    editor_internal::dragged_target_folder_index = -1;

    bool navigated = false;
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        auto& entry = entries[i];
        ImGui::PushID(i);
        ImGui::BeginGroup();

        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(kIconSize, kIconSize + 20.0f);
        ImGui::InvisibleButton("asset_btn", size);

        const bool item_active = ImGui::IsItemActive();
        const bool item_hovered = ImGui::IsItemHovered();
        if (item_hovered) ImGui::SetTooltip("%s", entry.filename.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsMouseDragging(0)) {
            editor.selected_asset_index = i;
            editor.selected_asset_name = entry.filename;
            editor.scene.selected = -1;
            editor.scene.selected_entities.clear();
        }

        if (entry.is_directory && item_hovered && ImGui::IsMouseDoubleClicked(0)) {
            editor.current_asset_path /= entry.filename;
            editor.selected_asset_index = -1;
            editor.selected_asset_name.clear();
            editor_internal::tex_cache.clear();
            model_preview_cache.clear();

            for (auto& pair : model_render_cache) UnloadRenderTexture(pair.second);
            
            model_render_cache.clear();
            ImGui::EndGroup();
            ImGui::PopID();
            navigated = true;
            break;
        }

        if (!entry.is_directory && item_hovered && ImGui::IsMouseDoubleClicked(0)) {
            const fs::path full_path = editor.current_asset_path / entry.filename;
            if (entry.is_model) {
                ModelAsset* asset = find_asset_by_path(full_path, editor.project_path);
                if (asset) open_model_viewer_for_asset(*asset);
            } 
            
            else if (entry.is_material) {
                open_material_viewer_for_path(full_path, editor_internal::tex_cache);
            } 
            
            else {
                #ifdef _WIN32
                    ShellExecuteA(nullptr, "open", full_path.string().c_str(), nullptr, nullptr, 1);
                #elif defined(__APPLE__)
                    std::string cmd = "open \"" + full_path.string() + "\"";
                    std::system(cmd.c_str());
                #elif defined(__linux__)
                    std::string cmd = "xdg-open \"" + full_path.string() + "\"";
                    std::system(cmd.c_str());
                #endif
            }
        }

        if (ImGui::BeginPopupContextItem("AssetContext")) {
            if (ImGui::MenuItem(lang.word("open_file_explorer"))) {
                open_in_system_file_explorer(editor.current_asset_path / entry.filename, entry.is_directory);
            }

            ImGui::Separator();

            if (ImGui::MenuItem(lang.word("delete"))) {
                editor.save_state();

                const fs::path target = editor.current_asset_path / entry.filename;
                std::error_code ec;
                if (entry.is_directory) fs::remove_all(target, ec);
                else fs::remove(target, ec);
                if (entry.is_image) fs::remove(target.string() + ".meta", ec);
                if (entry.is_texture_meta) fs::remove(texture_path_from_meta(target), ec);

                refresh_textures(&editor.scene, editor.project_path);
                refresh_assets(editor.project_path);
                refresh_models(editor.project_path, editor.scene);
                editor.selected_asset_index = -1;
                editor.selected_asset_name.clear();

                ImGui::EndPopup();
                ImGui::EndGroup();
                ImGui::PopID();
                break;
            }

            if (ImGui::MenuItem(lang.word("rename"))) {
                rename_target = i;
                const size_t copied = entry.filename.copy(editor_internal::rename_buf, sizeof(editor_internal::rename_buf) - 1);
                editor_internal::rename_buf[copied] = '\0';
                ImGui::OpenPopup(("%s##RenameAsset", lang.word("rename_asset")));
            }

            ImGui::Separator();

            if (ImGui::MenuItem(lang.word("new_file"))) {
                creating_folder = false;
                new_item_name[0] = '\0';
                show_create_popup = true;
            }

            if (ImGui::MenuItem(lang.word("new_folder"))) {
                creating_folder = true;
                new_item_name[0] = '\0';
                show_create_popup = true;
            }

            ImGui::EndPopup();
        }

        bool selected = editor.selected_asset_index == i;
        if (selecting && (fabsf(selection_start.x - selection_end.x) > 5.0f || fabsf(selection_start.y - selection_end.y) > 5.0f)) {
            const ImVec2 min(std::min(selection_start.x, selection_end.x), std::min(selection_start.y, selection_end.y));
            const ImVec2 max(std::max(selection_start.x, selection_end.x), std::max(selection_start.y, selection_end.y));
            if (!(pos.x + size.x < min.x || pos.x > max.x || pos.y + size.y < min.y || pos.y > max.y)) {
                selected = true;
            }
        }

        if (selected) {
            ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(80, 140, 255, 100));
        }

        ImGui::SetCursorScreenPos(pos);
        if (entry.is_directory) {
            const Texture texture = fs::is_empty(editor.current_asset_path / entry.filename) ? icon_folder_tex : icon_full_folder_tex;
            if (texture.id != 0) qcImGuiImage(&texture, ImVec2(kIconSize, kIconSize));
            else ImGui::Button(lang.word("folder"), ImVec2(kIconSize, kIconSize));
        } else if (entry.is_image) {
            const std::string full = (editor.current_asset_path / entry.filename).string();
            if (ImGui::IsRectVisible(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize)) &&
                !editor_internal::tex_cache.count(full)) {
                editor_internal::tex_cache[full] = LoadTexture(full.c_str());
            }
            if (editor_internal::tex_cache.count(full)) {
                qcImGuiImage(&editor_internal::tex_cache[full], ImVec2(kIconSize, kIconSize));
            }
        } 
        
        else if (entry.is_model) {
            const std::string full = (editor.current_asset_path / entry.filename).string();
            Texture preview_texture = {0};
            bool load_failed = false;

            if (model_preview_cache.count(full)) {
                preview_texture = model_preview_cache[full];
            } else if (ImGui::IsRectVisible(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize))) {
                ModelAsset* asset = find_asset_by_path(editor.current_asset_path / entry.filename, editor.project_path);
                if (asset) {
                    preview_texture = create_model_preview(*asset, full, g_editor_preferences.asset_preview_size);
                    if (preview_texture.id != 0) model_preview_cache[full] = preview_texture;
                    else load_failed = true;
                } else {
                    load_failed = true;
                }
            }

            if (preview_texture.id != 0) {
                qcImGuiImage(&preview_texture, ImVec2(kIconSize, kIconSize));
            } else {
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                draw_list->AddRectFilled(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize), load_failed ? IM_COL32(80, 80, 90, 255) : IM_COL32(100, 100, 120, 255));
                if (load_failed) {
                    draw_list->AddLine(ImVec2(pos.x + 10, pos.y + 10), ImVec2(pos.x + kIconSize - 10, pos.y + kIconSize - 10), IM_COL32(255, 100, 100, 200), 2.0f);
                    draw_list->AddLine(ImVec2(pos.x + kIconSize - 10, pos.y + 10), ImVec2(pos.x + 10, pos.y + kIconSize - 10), IM_COL32(255, 100, 100, 200), 2.0f);
                }
            }
        } 

        else if (entry.is_material)
        {
            const std::string full = (editor.current_asset_path / entry.filename).string();

            Texture preview = {0};

            if (material_preview_cache.count(full)) preview = material_preview_cache[full];
            else if (ImGui::IsRectVisible(pos, ImVec2(pos.x + kIconSize, pos.y + kIconSize))) preview = create_material_preview(full);

            if (preview.id != 0) qcImGuiImage(&preview, ImVec2(kIconSize, kIconSize));
            else ImGui::Button("MAT", ImVec2(kIconSize, kIconSize));
        }
        
        else {
            if (icon_file_tex.id != 0) qcImGuiImage(&icon_file_tex, ImVec2(kIconSize, kIconSize));
            else ImGui::Button(entry.extension.empty() ? "file" : entry.extension.c_str(), ImVec2(kIconSize, kIconSize));
        }

        if (!entry.is_directory && !entry.extension.empty()) {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            std::string ext_display = entry.extension;
            if (ext_display.size() > 3) ext_display.resize(3);
            const ImVec2 ext_text_size = ImGui::CalcTextSize(ext_display.c_str());
            const ImVec2 ext_pos(pos.x + kIconSize - ext_text_size.x - 2.0f, pos.y + kIconSize - ext_text_size.y - 2.0f);
            draw_list->AddRectFilled(ImVec2(ext_pos.x - 2.0f, ext_pos.y - 1.0f), ImVec2(pos.x + kIconSize, pos.y + kIconSize), IM_COL32(0, 0, 0, 180));
            draw_list->AddText(ext_pos, IM_COL32(255, 255, 255, 255), ext_display.c_str());
        }

        if (item_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !editor_internal::file_dragging) {
            dragged_file_name = entry.filename;
            drag_start_pos = ImGui::GetMousePos();

            editor_internal::dragged_file_index = i;
            dragged_file_path = editor.current_asset_path / entry.filename;
        }
        
        if (dragged_file_name == entry.filename && ImGui::IsMouseDown(0)) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            ImVec2 delta = ImVec2(mouse_pos.x - drag_start_pos.x, mouse_pos.y - drag_start_pos.y);
            if (fabsf(delta.x) > 5.0f || fabsf(delta.y) > 5.0f) {
                editor_internal::file_dragging = true;
                editor_internal::dragged_file_index = i;

                if (entry.is_material) {
                    editor_internal::scene_asset_dragging = true;
                    editor_internal::dragged_scene_asset_name = (editor.current_asset_path / entry.filename).string();
                }

                if (entry.extension == "prefab") {
                    editor_internal::scene_asset_dragging = true;
                    editor_internal::dragged_scene_asset_name = (editor.current_asset_path / entry.filename).string();
                }
                
                if (is_model_file(fs::path(entry.filename))) {
                    const std::string asset_name = get_asset_name_for_path(fs::path(editor.project_path), editor.current_asset_path / entry.filename);
                    editor_internal::scene_asset_dragging = true;
                    editor_internal::dragged_scene_asset_name = asset_name;
                }
            }
        }

        const bool has_valid_dragged_file = editor_internal::dragged_file_index >= 0 &&
            editor_internal::dragged_file_index < static_cast<int>(entries.size());
        if (entry.is_directory && editor_internal::file_dragging && has_valid_dragged_file &&
            entries[editor_internal::dragged_file_index].filename != entry.filename) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            bool mouse_over_folder = mouse_pos.x >= pos.x && mouse_pos.x <= pos.x + size.x &&
                                    mouse_pos.y >= pos.y && mouse_pos.y <= pos.y + size.y;
            
            if (mouse_over_folder) {
                ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(100, 200, 100, 150));
                editor_internal::dragged_target_folder_index = i;
            }
        }

        if (!entry.is_directory && (is_model_file(fs::path(entry.filename)) || entry.is_material)) {
            const std::string asset_name = get_asset_name_for_path(fs::path(editor.project_path), editor.current_asset_path / entry.filename);

            if (editor_internal::scene_asset_dragging && editor_internal::dragged_scene_asset_name == asset_name) {
                if (is_model_file(fs::path(entry.filename))) ImGui::SetTooltip(lang.word("spawn"), editor_internal::dragged_scene_asset_name.c_str());
            }
        }

        std::string label = entry.filename;
        if (label.size() > 10) label = label.substr(0, 8) + "..";
        const ImVec2 label_size = ImGui::CalcTextSize(label.c_str());
        ImGui::SetCursorScreenPos(ImVec2(pos.x + (kIconSize - label_size.x) * 0.5f, pos.y + kIconSize + 2.0f));
        ImGui::TextUnformatted(label.c_str());
        ImGui::EndGroup();

        const float next_x2 = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + kIconSize;
        if (i + 1 < static_cast<int>(entries.size()) && next_x2 < window_visible_x2) ImGui::SameLine();
        ImGui::PopID();
    }

    ImGui::SetNextItemAllowOverlap();
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImVec2 bg_size = ImGui::GetContentRegionMax();
    if (bg_size.x < 1.f) bg_size.x = 1.f;
    if (bg_size.y < 1.f) bg_size.y = 1.f;

    ImGui::InvisibleButton("##bg_drop_zone", bg_size);

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_INDEX")) {
            int idx = *(const int*)payload->Data;
            Entity e = editor.scene.entities[idx];

            make_prefab(e, editor.current_asset_path);
        }
        ImGui::EndDragDropTarget();
    }

    if (selecting && (fabsf(selection_start.x - selection_end.x) > 5.0f || fabsf(selection_start.y - selection_end.y) > 5.0f)) {
        const ImVec2 min(std::min(selection_start.x, selection_end.x), std::min(selection_start.y, selection_end.y));
        const ImVec2 max(std::max(selection_start.x, selection_end.x), std::max(selection_start.y, selection_end.y));
        ImGui::GetForegroundDrawList()->AddRectFilled(min, max, IM_COL32(80, 140, 255, 40));
    }

    if (editor_internal::file_dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (editor_internal::dragged_target_folder_index >= 0 && 
            editor_internal::dragged_file_index >= 0 && 
            editor_internal::dragged_file_index < static_cast<int>(entries.size()) && 
            editor_internal::dragged_target_folder_index < static_cast<int>(entries.size()) &&
            editor_internal::dragged_file_index != editor_internal::dragged_target_folder_index &&
            entries[editor_internal::dragged_target_folder_index].is_directory) {
            
            editor.save_state();
            const fs::path source_path = editor.current_asset_path / entries[editor_internal::dragged_file_index].filename;
            const fs::path dest_dir = editor.current_asset_path / entries[editor_internal::dragged_target_folder_index].filename;
            const fs::path dest_path = dest_dir / fs::path(source_path).filename();

            if (fs::exists(dest_path)) {
                show_duplicate_popup = true;
                duplicate_name = source_path.filename().string();
            }
            
            else {
                editor.save_state();

                std::error_code ec;
                fs::rename(source_path, dest_path, ec);

                if (!ec) {
                    if (is_image_file(source_path) && fs::exists(source_path.string() + ".meta")) {
                        fs::rename(source_path.string() + ".meta", dest_path.string() + ".meta", ec);
                    }
                    refresh_textures(&editor.scene, editor.project_path);
                    refresh_assets(editor.project_path);
                    refresh_models(editor.project_path, editor.scene);
                    editor.selected_asset_index = -1;
                }
            }
        }
        editor_internal::file_dragging = false;
        editor_internal::dragged_file_index = -1;
        editor_internal::dragged_target_folder_index = -1;
        dragged_file_name.clear();
        drag_start_pos = ImVec2(0, 0);
    }

    if (rename_target >= 0) {
        ImGui::OpenPopup(("%s##RenameAsset", lang.word("rename_asset")));
        rename_target = -2;
    }

    static std::string last_filename;
    if (rename_target == -2 && ImGui::BeginPopupModal(("%s##RenameAsset", lang.word("rename_asset")), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##rename", editor_internal::rename_buf, IM_ARRAYSIZE(editor_internal::rename_buf));

        if (ImGui::Button(lang.word("ok"))) {
            const std::string new_filename = editor_internal::rename_buf;
            if (last_filename != new_filename) {
                editor.save_state();
                last_filename = new_filename;

                if (editor.selected_asset_index >= 0 && editor.selected_asset_index < static_cast<int>(entries.size())) {
                    const fs::path old_path = editor.current_asset_path / entries[editor.selected_asset_index].filename;
                    const fs::path new_path = editor.current_asset_path / editor_internal::rename_buf;

                    if (editor_internal::rename_buf[0] != '\0' && old_path != new_path && fs::exists(old_path)) {
                        try {
                            fs::rename(old_path, new_path);
                            if (is_image_file(old_path) && fs::exists(old_path.string() + ".meta")) {
                                fs::rename(old_path.string() + ".meta", new_path.string() + ".meta");
                            }
                            refresh_textures(&editor.scene, editor.project_path);
                            refresh_assets(editor.project_path);
                            refresh_models(editor.project_path, editor.scene);
                            editor.selected_asset_index = -1;
                            editor.selected_asset_name.clear();
                        } catch (...) {
                        }
                    }
                }
            }

            rename_target = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button(lang.word("cancel"))) {
            rename_target = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupContextItem("AssetBgContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem(lang.word("new_file"))) {
            creating_folder = false;
            new_item_name[0] = '\0';
            show_create_popup = true;
        }

        if (ImGui::MenuItem(lang.word("new_folder"))) {
            creating_folder = true;
            new_item_name[0] = '\0';
            show_create_popup = true;
        }

        ImGui::EndPopup();
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        editor_internal::file_dragging = false;
        editor_internal::scene_asset_dragging = false;
        editor_internal::dragged_file_index = -1;
        editor_internal::dragged_target_folder_index = -1;
        dragged_file_name.clear();
        dragged_file_path.clear();
        drag_start_pos = ImVec2(0, 0);
    }

    if (show_duplicate_popup) {
        ImGui::OpenPopup(("%s##DuplicateName", lang.word("move_error")));
        show_duplicate_popup = false;
    }

    if (ImGui::BeginPopupModal(("%s##DuplicateName", lang.word("move_error")), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(lang.word("unable_to_move"));
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", duplicate_name.c_str());
        ImGui::Spacing();
        ImGui::Text(lang.word("path_already_exists"));

        ImGui::Spacing();

        if (ImGui::Button(lang.word("ok"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild();

    if (show_create_popup && !navigated) {
        ImGui::OpenPopup(popup_name.c_str());
        show_create_popup = false;
    }

    if (ImGui::BeginPopupModal(popup_name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(creating_folder ? lang.word("new_folder") : lang.word("new_file"));
        ImGui::InputText(lang.word("name"), new_item_name, IM_ARRAYSIZE(new_item_name));
        ImGui::Spacing();

        if (ImGui::Button(lang.word("ok"), ImVec2(120, 0))) {
            if (new_item_name[0] != '\0') {
                fs::path path;

                if (creating_folder) {
                    path = editor.current_asset_path / new_item_name;

                    int suffix = 1;
                    while (fs::exists(path)) {
                        path = editor.current_asset_path / (std::string(new_item_name) + "_" + std::to_string(suffix++));
                    }

                    std::error_code ec;
                    fs::create_directory(path, ec);
                }

                else {
                    std::string filename = new_item_name;
                    if (filename.find('.') == std::string::npos) filename += ".txt";

                    path = editor.current_asset_path / filename;

                    int suffix = 1;
                    while (fs::exists(path)) {
                        path = editor.current_asset_path / (std::string(new_item_name) + "_" + std::to_string(suffix++) + ".txt");
                    }

                    std::ofstream f(path);
                    f.close();
                }

                refresh_assets(editor.project_path);
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button(lang.word("cancel"), ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

void draw_selected_texture_inspector(Editor& editor) {
    if (editor.selected_asset_name.empty()) return;

    const fs::path selected = editor.current_asset_path / editor.selected_asset_name;
    const fs::path texture_path = texture_path_from_meta(selected);
    if (!is_image_file(texture_path) || !fs::exists(texture_path)) return;

    TextureMeta meta;
    if (!load_texture_meta(texture_path, meta)) {
        ensure_texture_meta(texture_path);
        load_texture_meta(texture_path, meta);
    }

    ImGui::Separator();
    ImGui::Text("Texture Importer");
    ImGui::TextWrapped("%s", texture_path.filename().string().c_str());
    ImGui::TextDisabled("GUID: %s", meta.guid.c_str());

    bool changed = false;
    changed |= ImGui::Checkbox("Enable mip maps", &meta.enable_mip_map);
    changed |= ImGui::Checkbox("sRGB texture", &meta.srgb_texture);
    changed |= ImGui::Checkbox("Readable", &meta.is_readable);
    changed |= ImGui::Checkbox("Alpha is transparency", &meta.alpha_is_transparency);
    const char* filter_modes[] = { "Nearest", "Linear" };
    const char* wrap_modes[] = { "Repeat", "Clamp" };
    const char* sprite_modes[] = { "None", "Single", "Multiple" };
    const char* texture_types[] = { "Default", "Normal", "Sprite", "Cursor", "Cookie", "Lightmap", "Shadowmask", "Directional", "Single channel" };
    changed |= ImGui::Combo("Filter mode", &meta.filter_mode, filter_modes, IM_ARRAYSIZE(filter_modes));
    changed |= ImGui::Combo("Wrap U", &meta.wrap_u, wrap_modes, IM_ARRAYSIZE(wrap_modes));
    changed |= ImGui::Combo("Wrap V", &meta.wrap_v, wrap_modes, IM_ARRAYSIZE(wrap_modes));
    changed |= ImGui::SliderInt("Max texture size", &meta.max_texture_size, 32, 8192);
    changed |= ImGui::SliderInt("Compression quality", &meta.compression_quality, 0, 100);
    changed |= ImGui::Combo("Sprite mode", &meta.sprite_mode, sprite_modes, IM_ARRAYSIZE(sprite_modes));
    changed |= ImGui::Combo("Texture type", &meta.texture_type, texture_types, IM_ARRAYSIZE(texture_types));

    if (changed) {
        save_texture_meta(texture_path, meta);
        const std::string relative_name = fs::relative(texture_path, fs::path(editor.project_path) / "resources").generic_string();
        for (auto& option : texture_options) {
            if (option.name == relative_name) apply_texture_meta(option.texture, meta);
        }
    }
}

void Editor::draw_assets_ui() {
    ::draw_assets_ui(*this);
}

void cleanup_assets_ui() {
    for (auto& pair : model_render_cache)
        UnloadRenderTexture(pair.second);

    for (auto& p : material_render_cache)
        UnloadRenderTexture(p.second);

    material_render_cache.clear();
    material_preview_cache.clear();

    model_render_cache.clear();
    model_preview_cache.clear();
    editor_internal::tex_cache.clear();
}

void invalidate_material_previews() {
    for (auto& p : material_render_cache)
        UnloadRenderTexture(p.second);

    material_render_cache.clear();
    material_preview_cache.clear();
}
