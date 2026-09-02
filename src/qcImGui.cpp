#include "qcImGui.h"

#include "imgui.h"
#if defined(_WIN32)
#include "imgui_impl_dx11.h"
#endif
#include "imgui_impl_opengl3.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_sdl3.h"

#include <cmath>
#include <utility>

namespace qc {

namespace {

bool g_qc_imgui_initialized = false;
enum class ImGuiBackendKind {
    None,
    OpenGL,
    Vulkan,
    D3D11
};

ImGuiBackendKind g_qc_imgui_backend = ImGuiBackendKind::None;

void qcImGuiEventBridge(const SDL_Event* event) {
    qcImGuiProcessEvent(event);
}

void qcImGuiVulkanRenderCallback(VkCommandBuffer commandBuffer) {
    if (!g_qc_imgui_initialized || g_qc_imgui_backend != ImGuiBackendKind::Vulkan) {
        return;
    }

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

#if defined(_WIN32)
void qcImGuiD3D11RenderCallback(ID3D11DeviceContext* deviceContext) {
    if (!g_qc_imgui_initialized || g_qc_imgui_backend != ImGuiBackendKind::D3D11 ||
        deviceContext == nullptr) {
        return;
    }

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
#endif

ImTextureID qcImGuiTextureIdFor(const Texture2D* texture) {
    if (texture == nullptr || texture->id == 0) {
        return 0;
    }

    if (GetCurrentBackend() == RendererType::Vulkan) {
        const VkDescriptorSet descriptor = GetVulkanTextureDescriptorSet(texture->id);
        if (descriptor == VK_NULL_HANDLE) {
            return 0;
        }
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(descriptor));
    }

#if defined(_WIN32)
    if (GetCurrentBackend() == RendererType::D3D11) {
        ID3D11ShaderResourceView* shaderResourceView =
            GetD3D11TextureShaderResourceView(texture->id);
        return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(shaderResourceView));
    }
#endif

    return static_cast<ImTextureID>(static_cast<uintptr_t>(texture->id));
}

} // namespace

bool qcImGuiSetup(bool darkTheme) {
    if (g_qc_imgui_initialized) {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (darkTheme) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsClassic();
    }

    SDL_Window* window = GetNativeWindow();
    if (window == nullptr) {
        ImGui::DestroyContext();
        return false;
    }

    g_qc_imgui_backend = ImGuiBackendKind::None;

    const RendererType backend = GetCurrentBackend();
    bool initOk = false;

    if (backend == RendererType::OpenGL) {
        SDL_GLContext context = GetNativeContext();
        if (context == nullptr) {
            ImGui::DestroyContext();
            return false;
        }

        if (!ImGui_ImplSDL3_InitForOpenGL(window, context)) {
            ImGui::DestroyContext();
            return false;
        }
        if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        g_qc_imgui_backend = ImGuiBackendKind::OpenGL;
        initOk = true;
    } else if (backend == RendererType::Vulkan) {
        const VkInstance instance = GetVulkanInstance();
        const VkPhysicalDevice physicalDevice = GetVulkanPhysicalDevice();
        const VkDevice device = GetVulkanDevice();
        const VkQueue queue = GetVulkanGraphicsQueue();
        const uint32_t queueFamily = GetVulkanGraphicsQueueFamily();
        const VkDescriptorPool descriptorPool = GetVulkanDescriptorPool();
        const VkRenderPass renderPass = GetVulkanMainRenderPass();
        const uint32_t minImageCount = GetVulkanMinImageCount();
        const uint32_t imageCount = GetVulkanImageCount();

        if (instance == VK_NULL_HANDLE || physicalDevice == VK_NULL_HANDLE || device == VK_NULL_HANDLE ||
            queue == VK_NULL_HANDLE || queueFamily == UINT32_MAX || descriptorPool == VK_NULL_HANDLE ||
            renderPass == VK_NULL_HANDLE || minImageCount == 0 || imageCount == 0) {
            ImGui::DestroyContext();
            return false;
        }

        if (!ImGui_ImplSDL3_InitForVulkan(window)) {
            ImGui::DestroyContext();
            return false;
        }

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion = VK_API_VERSION_1_2;
        initInfo.Instance = instance;
        initInfo.PhysicalDevice = physicalDevice;
        initInfo.Device = device;
        initInfo.QueueFamily = queueFamily;
        initInfo.Queue = queue;
        initInfo.DescriptorPool = descriptorPool;
        initInfo.DescriptorPoolSize = 0;
        initInfo.MinImageCount = minImageCount;
        initInfo.ImageCount = imageCount;
        initInfo.PipelineInfoMain.RenderPass = renderPass;
        initInfo.PipelineInfoMain.Subpass = 0;
        initInfo.PipelineInfoMain.MSAASamples = GetVulkanMSAASamples();
        initInfo.UseDynamicRendering = false;
        initInfo.MinAllocationSize = 1024 * 1024;

        initOk = ImGui_ImplVulkan_Init(&initInfo);
        if (initOk) {
            SetVulkanRenderCallback(qcImGuiVulkanRenderCallback);
            g_qc_imgui_backend = ImGuiBackendKind::Vulkan;
        } else {
            ImGui_ImplSDL3_Shutdown();
        }
#if defined(_WIN32)
    } else if (backend == RendererType::D3D11) {
        ID3D11Device* device = GetD3D11Device();
        ID3D11DeviceContext* deviceContext = GetD3D11ImmediateContext();
        if (!ImGui_ImplSDL3_InitForD3D(window)) {
            ImGui::DestroyContext();
            return false;
        }
        if (device == nullptr || deviceContext == nullptr ||
            !ImGui_ImplDX11_Init(device, deviceContext)) {
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        SetD3D11RenderCallback(qcImGuiD3D11RenderCallback);
        g_qc_imgui_backend = ImGuiBackendKind::D3D11;
        initOk = true;
#endif
    }

    if (!initOk) {
        ImGui::DestroyContext();
        return false;
    }

    SetNativeEventCallback(qcImGuiEventBridge);
    g_qc_imgui_initialized = true;
    return true;
}

void qcImGuiShutdown() {
    if (!g_qc_imgui_initialized) {
        return;
    }

    SetNativeEventCallback(nullptr);
    SetVulkanRenderCallback(nullptr);
#if defined(_WIN32)
    SetD3D11RenderCallback(nullptr);
#endif

    if (g_qc_imgui_backend == ImGuiBackendKind::OpenGL) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    } else if (g_qc_imgui_backend == ImGuiBackendKind::Vulkan) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
#if defined(_WIN32)
    } else if (g_qc_imgui_backend == ImGuiBackendKind::D3D11) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplSDL3_Shutdown();
#endif
    }
    ImGui::DestroyContext();
    g_qc_imgui_initialized = false;
    g_qc_imgui_backend = ImGuiBackendKind::None;
}

void qcImGuiBegin() {
    if (!g_qc_imgui_initialized) {
        return;
    }

    ImGui_ImplSDL3_NewFrame();
    if (g_qc_imgui_backend == ImGuiBackendKind::OpenGL) {
        ImGui_ImplOpenGL3_NewFrame();
    } else if (g_qc_imgui_backend == ImGuiBackendKind::Vulkan) {
        ImGui_ImplVulkan_NewFrame();
#if defined(_WIN32)
    } else if (g_qc_imgui_backend == ImGuiBackendKind::D3D11) {
        ImGui_ImplDX11_NewFrame();
#endif
    }
    ImGui::NewFrame();
}

void qcImGuiEnd() {
    if (!g_qc_imgui_initialized) {
        return;
    }

    ImGui::Render();
    if (g_qc_imgui_backend == ImGuiBackendKind::OpenGL) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

void qcImGuiProcessEvent(const SDL_Event* event) {
    if (!g_qc_imgui_initialized || event == nullptr) {
        return;
    }

    ImGui_ImplSDL3_ProcessEvent(event);
}

ImTextureID qcImGuiGetTextureId(const Texture2D* texture) {
    return qcImGuiTextureIdFor(texture);
}

void qcImGuiImage(const Texture2D* texture, const ImVec2& size, const ImVec2& uv0, const ImVec2& uv1) {
    const ImTextureID textureId = qcImGuiTextureIdFor(texture);
    if (textureId == 0) {
        return;
    }
    ImGui::Image(textureId, size, uv0, uv1);
}

void qcImGuiAddImage(ImDrawList* drawList, const Texture2D* texture, const ImVec2& pMin, const ImVec2& pMax, const ImVec2& uv0, const ImVec2& uv1, ImU32 color) {
    if (drawList == nullptr) {
        return;
    }

    const ImTextureID textureId = qcImGuiTextureIdFor(texture);
    if (textureId == 0) {
        return;
    }

    drawList->AddImage(textureId, pMin, pMax, uv0, uv1, color);
}

void qcImGuiImageRect(const Texture2D* texture, int width, int height, Rectangle sourceRect) {
    const ImTextureID textureId = qcImGuiTextureIdFor(texture);
    if (textureId == 0) {
        return;
    }

    const float tex_width = static_cast<float>(texture->width);
    const float tex_height = static_cast<float>(texture->height);

    const float src_w = fabsf(sourceRect.width);
    const float src_h = fabsf(sourceRect.height);

    float u0 = sourceRect.x / tex_width;
    float v0 = sourceRect.y / tex_height;
    float u1 = (sourceRect.x + src_w) / tex_width;
    float v1 = (sourceRect.y + src_h) / tex_height;

    if (sourceRect.width < 0.0f) {
        std::swap(u0, u1);
    }
    if (sourceRect.height < 0.0f) {
        std::swap(v0, v1);
    }

    ImGui::Image(
        textureId,
        ImVec2(static_cast<float>(width), static_cast<float>(height)),
        ImVec2(u0, v0),
        ImVec2(u1, v1)
    );
}

} // namespace qc
