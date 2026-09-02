#include "camera.h"
#include <cmath>
#include "imgui.h"
#include "ImGuizmo.h"
#include "editor/editor_preferences.h"
#include "SDL3/SDL_mouse.h"

namespace {

void set_camera_capture(bool enabled) {
    if (SDL_Window* window = GetNativeWindow()) {
        SDL_SetWindowRelativeMouseMode(window, enabled);
    }

    if (enabled) DisableCursor();
    else EnableCursor();
}

} // namespace

FlyCamera::FlyCamera() {
    cam.position = {5.0f, 5.0f, 5.0f};
    cam.target = {0.0f, 0.0f, 0.0f};
    cam.up = {0.0f, 1.0f, 0.0f};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    Vec3 dir = cam.target - cam.position;
    yaw = atan2f(dir.x, dir.z);
    pitch = asinf(dir.y / sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z));
}

void FlyCamera::update(Scene& scene) {
    if (!active && ImGuizmo::IsUsing())
        return;

    Entity* selected = scene.get_selected();
    MeshComponent* sel_mesh = selected ? selected->get_mesh_component() : nullptr;
    if (sel_mesh && sel_mesh->vertex_gizmo) return;

    const float wheel = GetMouseWheelMove();
    if (fabsf(wheel) > 0.001f) {
        cam.fovy -= wheel * zoom_sensitivity;
        if (cam.fovy < 20.0f) cam.fovy = 20.0f;
        if (cam.fovy > 120.0f) cam.fovy = 120.0f;
        g_editor_preferences.camera_fov = cam.fovy;
    }

    if (IsMouseButtonPressed(MouseButton::Left) &&
        !ImGuizmo::IsOver() && !ImGui::IsAnyItemActive()) {
        set_camera_capture(true);
        active = true;
    }

    if (IsKeyPressed(KeyboardKey::Escape) || !IsWindowFocused()) {
        set_camera_capture(false);
        active = false;
    }

    if (!active) return;

    float dt = GetDeltaTime();
    float rel_x = 0.0f;
    float rel_y = 0.0f;
    SDL_GetRelativeMouseState(&rel_x, &rel_y);
    Vec2 md = { rel_x, rel_y };
    if (fabs(md.x) > 100 || fabs(md.y) > 100) md = {0,0};

    yaw   -= md.x * sensitivity;
    pitch -= md.y * sensitivity;

    if (pitch > 1.5f) pitch = 1.5f;
    if (pitch < -1.5f) pitch = -1.5f;

    Vec3 forward = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };

    forward.normalized();
    Vec3 right = { sinf(yaw - PI/2), 0, cosf(yaw - PI/2) };
    if (IsKeyDown(KeyboardKey::W)) cam.position = cam.position + (forward * (speed * dt));
    if (IsKeyDown(KeyboardKey::S)) cam.position = cam.position - (forward * (speed * dt));
    if (IsKeyDown(KeyboardKey::A)) cam.position = cam.position - (right * (speed * dt));
    if (IsKeyDown(KeyboardKey::D)) cam.position = cam.position + (right * (speed * dt));

    cam.target = cam.position + forward;
}

void FlyCamera::focus_on(const Vec3& point) {
    cam.position = point + Vec3{5.0f, 5.0f, 5.0f};
    cam.target = point;

    Vec3 dir = cam.target - cam.position;
    yaw = atan2f(dir.x, dir.z);
    pitch = asinf(dir.y / sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z));
}

Camera3D& FlyCamera::get_camera() {
    return cam;
}
