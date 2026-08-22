#pragma once

#include "imgui.h"
#include "QuarkCore/QuarkCore.hpp"

namespace qc {

#if defined(_WIN32)
    #if defined(BUILD_LIBTYPE_SHARED)
        #define QCIMGUI_API __declspec(dllexport)
    #else
        #define QCIMGUI_API __declspec(dllimport)
    #endif
#else
    #define QCIMGUI_API
#endif

QCIMGUI_API bool qcImGuiSetup(bool darkTheme);
QCIMGUI_API void qcImGuiShutdown();
QCIMGUI_API void qcImGuiBegin();
QCIMGUI_API void qcImGuiEnd();
QCIMGUI_API void qcImGuiProcessEvent(const SDL_Event* event);
QCIMGUI_API ImTextureID qcImGuiGetTextureId(const Texture2D* texture);
QCIMGUI_API void qcImGuiImage(const Texture2D* texture, const ImVec2& size, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f));
QCIMGUI_API void qcImGuiAddImage(ImDrawList* drawList, const Texture2D* texture, const ImVec2& pMin, const ImVec2& pMax, const ImVec2& uv0 = ImVec2(0.0f, 0.0f), const ImVec2& uv1 = ImVec2(1.0f, 1.0f), ImU32 color = IM_COL32_WHITE);
QCIMGUI_API void qcImGuiImageRect(const Texture2D* texture, int width, int height, Rectangle sourceRect);

} // namespace qc
