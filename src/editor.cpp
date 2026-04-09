#include "rlImGui.h"
#include "imgui.h"
#include "raymath.h"
#include "headers/editor.h"
#include "headers/entity.h"
#include "headers/ImGuizmo.h"

namespace fs = std::filesystem;

static ImGuizmo::OPERATION gizmo_mode = ImGuizmo::TRANSLATE;
static int renaming_index = -1;
static char rename_buf[128] = "";

void Editor::handle_input() {
    float speed = 0.1f;
    Entity* e = scene.get_selected();

    if (e) {
        if (IsKeyDown(KEY_RIGHT)) e->position.x += speed;
        if (IsKeyDown(KEY_LEFT))  e->position.x -= speed;
        if (IsKeyDown(KEY_UP))    e->position.z -= speed;
        if (IsKeyDown(KEY_DOWN))  e->position.z += speed;
    }

    if (IsKeyPressed(KEY_P)) gizmo_mode = ImGuizmo::TRANSLATE;
    if (IsKeyPressed(KEY_R)) gizmo_mode = ImGuizmo::ROTATE;
    if (IsKeyPressed(KEY_S)) gizmo_mode = ImGuizmo::SCALE;


    if (IsFileDropped()) {
        FilePathList dropped = LoadDroppedFiles();

        if (!fs::exists("assets")) fs::create_directories("assets");

        for (unsigned int i = 0; i < dropped.count; i++) {
            fs::path src(dropped.paths[i]);
            fs::path dst = fs::path("assets") / src.filename();
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        }

        UnloadDroppedFiles(dropped);
        refresh_textures();
    }

}

void Editor::draw_gizmo(Camera3D camera) {
    Entity* e = scene.get_selected();
    if (!e) return;

    ImGuizmo::BeginFrame();
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    Matrix view = GetCameraMatrix(camera);
    Matrix proj = MatrixPerspective(camera.fovy * DEG2RAD, io.DisplaySize.x / io.DisplaySize.y, 0.1f, 1000.0f);

    view = MatrixTranspose(view);
    proj = MatrixTranspose(proj);

    float view_mat[16], proj_mat[16];

    memcpy(view_mat, &view, sizeof(view_mat));
    memcpy(proj_mat, &proj, sizeof(proj_mat));

    float t[3] = { e->position.x, e->position.y, e->position.z };
    float r[3] = { e->rotation.x, e->rotation.y, e->rotation.z };
    float s[3] = { e->scale.x, e->scale.y, e->scale.z };
    float matrix[16];

    ImGuizmo::RecomposeMatrixFromComponents(t, r, s, matrix);
    ImGuizmo::Manipulate(view_mat, proj_mat, gizmo_mode, ImGuizmo::WORLD, matrix);

    if (ImGuizmo::IsUsing()) {
        float nt[3], nr[3], ns[3];
        ImGuizmo::DecomposeMatrixToComponents(matrix, nt, nr, ns);
        e->position = { nt[0], nt[1], nt[2] };
        e->rotation = { nr[0], nr[1], nr[2] };
        e->scale    = { ns[0], ns[1], ns[2] };
    }
}

void Editor::draw_ui() {
    ImGui::SetNextWindowSize(ImVec2(150, 540), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Once);
    ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    for (int i = 0; i < scene.entities.size(); i++) {
        bool selected = (scene.selected == i);
        if (ImGui::Selectable(scene.entities[i].name.c_str(), selected)) scene.selected = i;

        ImGui::PushID(i);
        if (ImGui::BeginPopupContextItem("context", 1)) {
            if (ImGui::MenuItem("Delete")) {
                scene.entities.erase(scene.entities.begin() + i);
                if (scene.selected == i) scene.selected = -1;
                else if (scene.selected > i) scene.selected--;
                ImGui::EndPopup(); ImGui::PopID(); break;
            }
            if (ImGui::MenuItem("Rename")) {
                renaming_index = i;
                const size_t copied = scene.entities[i].name.copy(rename_buf, sizeof(rename_buf) - 1);
                rename_buf[copied] = '\0';
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::BeginMenu("Create")) {
            for (auto& a : assets) {
                if (ImGui::MenuItem(a.name.c_str())) {
                    Entity e;
                    e.name = a.name;
                    e.type = a.type;
                    e.asset = &a;
                    e.segments = 16;

                    if (a.isProcedural) {
                        e.model = a.generator(e.segments);
                        store_uv(&e);
                    }

                    else e.model = a.loadedModel;
                    e.texture = {0};
                    scene.entities.push_back(e);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::End();

    if (renaming_index != -1) { 
        ImGui::OpenPopup("Rename"); 
        renaming_index = -2; 
    }
    
    if (ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("##rename", rename_buf, IM_ARRAYSIZE(rename_buf));
        if (ImGui::Button("OK")) {
            if (renaming_index == -2) {
                scene.entities[scene.selected].name = rename_buf;
            }
            renaming_index = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            renaming_index = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(225, 540), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(1050, 5), ImGuiCond_Once);

    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::Text("Mode"); ImGui::SameLine();

    if (ImGui::Button("P")) gizmo_mode = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::Button("R")) gizmo_mode = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::Button("S")) gizmo_mode = ImGuizmo::SCALE;

    Entity* e = scene.get_selected();
    if (e) {
        ImGui::Separator();
        ImGui::Text("Transform");
        float pos[3] = { e->position.x, e->position.y, e->position.z };
        float rot[3] = { e->rotation.x, e->rotation.y, e->rotation.z };
        float scl[3] = { e->scale.x, e->scale.y, e->scale.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f)) e->position = { pos[0], pos[1], pos[2] };
        if (ImGui::DragFloat3("Rotation", rot, 1.0f)) e->rotation = { rot[0], rot[1], rot[2] };
        if (ImGui::DragFloat3("Scale", scl, 0.1f)) e->scale = { scl[0], scl[1], scl[2] };

        ImGui::Separator();
        ImGui::Text("Mesh");


        if (e->asset && e->asset->isProcedural)
            if (ImGui::DragInt("Segments", &e->segments, 1, 3, 125)) {
                update_model(e);
                store_uv(e);
            }

        static int current_model_index = 0;
        std::vector<const char*> model_names;
        model_names.reserve(assets.size());
        for (int i = 0; i < static_cast<int>(assets.size()); i++) {
            model_names.push_back(assets[i].name.c_str());
            model_names[i] = assets[i].name.c_str();
            if (e->asset && e->asset->name == assets[i].name) current_model_index = i;
        }

        if (!model_names.empty() && ImGui::Combo("Model", &current_model_index, model_names.data(), static_cast<int>(model_names.size()))) {
            e->asset = &assets[current_model_index];
            e->type  = e->asset->type;

            if (e->model.meshCount > 0) {
                UnloadModel(e->model);
                e->model = {0};
            }

            if (e->asset->isProcedural) { 
                e->segments = 16; 
                update_model(e); 
                store_uv(e);
            } else {
                e->model = e->asset->loadedModel;
            }
        }

        static int current_texture_index = 0;
        for (int i = 0; i < static_cast<int>(texture_options.size()); i++)
            if (e->texture.id == texture_options[i].texture.id) current_texture_index = i;

        std::vector<const char*> texture_names;
        texture_names.reserve(texture_options.size());
        for (int i = 0; i < static_cast<int>(texture_options.size()); i++)
            texture_names.push_back(texture_options[i].name.c_str());

        ImGui::Separator();
        ImGui::Text("Material");

        if (ImGui::Combo("Texture", &current_texture_index, texture_names.data(), texture_names.size())) {
            if (current_texture_index == 0) {
                static Texture2D white_tex = {0};

                if (white_tex.id == 0) {
                    Image img = GenImageColor(1, 1, WHITE);
                    white_tex = LoadTextureFromImage(img);
                    UnloadImage(img);
                }

                e->texture = white_tex;

                for (int i = 0; i < e->model.materialCount; i++) {
                    e->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = e->texture;
                    e->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                }
            } 
            
            else {
                e->texture = texture_options[current_texture_index].texture;
                for (int i = 0; i < e->model.materialCount; i++)
                    e->model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = e->texture;
            }
        }

        ImGui::Checkbox("Stretch Texture", &e->texture_stretch);

        if (!e->texture_stretch) {

            ImGui::Checkbox("Auto UV", &e->auto_uv);

            if (e->auto_uv) {
                ImGui::InputFloat("Scale X", &e->uv_scale_vec.x, 0.1f, 1.0f, "%.2f");
                ImGui::InputFloat("Scale Y", &e->uv_scale_vec.y, 0.1f, 1.0f, "%.2f");

                if (e->uv_scale_vec.x < 0.01f) e->uv_scale_vec.x = 0.01f;
                if (e->uv_scale_vec.y < 0.01f) e->uv_scale_vec.y = 0.01f;
            }

            else {
                ImGui::InputFloat("Repeat U", &e->texture_repeat_u, 0.1f, 1.0f, "%.2f");
                ImGui::InputFloat("Repeat V", &e->texture_repeat_v, 0.1f, 1.0f, "%.2f");
                
                if (e->texture_repeat_u < 0.01f) e->texture_repeat_u = 0.01f;
                if (e->texture_repeat_v < 0.01f) e->texture_repeat_v = 0.01f;
            }

        }



        float color[4] = { e->color.r / 255.f, e->color.g / 255.f, e->color.b / 255.f, e->color.a / 255.f };
        if (ImGui::ColorEdit4("Color", color)) e->color = {
            (unsigned char)(color[0]*255),
            (unsigned char)(color[1]*255),
            (unsigned char)(color[2]*255),
            (unsigned char)(color[3]*255)
        };

        float outline[4] = { e->outline_color.r / 255.f, e->outline_color.g / 255.f, e->outline_color.b / 255.f, e->outline_color.a / 255.f };
        if (ImGui::ColorEdit4("Outline", outline)) e->outline_color = {
            (unsigned char)(outline[0]*255),
            (unsigned char)(outline[1]*255),
            (unsigned char)(outline[2]*255),
            (unsigned char)(outline[3]*255)
        };
    }

    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(1270, 165), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(5, 550), ImGuiCond_Once);
    ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    

    ImGui::End();
}