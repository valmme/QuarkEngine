#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "raymath.h"
#include "rlgl.h"
#include "headers/editor.h"
#include "headers/camera.h"

int main() {
    if (!std::filesystem::exists("assets")) std::filesystem::create_directory("assets");

    InitWindow(1280, 720, "Quark Engine");

    SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));
    rlImGuiSetup(true);
    SetExitKey(0);

    Editor editor;
    FlyCamera camera;

    load_models();
    load_textures();

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(DARKGRAY);

        rlImGuiBegin();

        editor.draw_gizmo(camera.get_camera());
        camera.update();
        editor.handle_input();

        BeginMode3D(camera.get_camera());

        DrawGrid(20, 1.0f);

        for (auto& e : editor.scene.entities) {
            draw_entity_with_texture(e);
        }

        EndMode3D();

        editor.draw_ui();

        rlImGuiEnd();

        EndDrawing();
    }

    editor.scene.release_resources();
    unload_models();
    unload_textures();

    rlImGuiShutdown();
    CloseWindow();
}
