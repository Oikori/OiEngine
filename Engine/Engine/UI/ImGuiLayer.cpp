#include "ImGuiLayer.h"

#include <filesystem>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Engine/Core/Application.h"

namespace OiEngine {
    // 引擎要求 OpenGL 4.5 Core（见 OpenGLContext），GLSL 版本保持一致
    static const char* GLSL_VERSION = "#version 450";

    // 中文界面：默认字体不含中文字形（显示为 ?），加载系统中文字体
    static const char* CHINESE_FONT_PATH = "C:/Windows/Fonts/msyh.ttc";
    static constexpr float FONT_SIZE = 18.0f;

    ImGuiLayer::ImGuiLayer(): Layer("ImGuiLayer") {}
    ImGuiLayer::~ImGuiLayer() = default;

    void ImGuiLayer::OnAttach()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // 键盘导航
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // 停靠布局（docking 分支）

        if (std::filesystem::exists(CHINESE_FONT_PATH)) {
            io.Fonts->AddFontFromFileTTF(CHINESE_FONT_PATH, FONT_SIZE, nullptr,
                                         io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        } else {
            OI_WARN("中文字体不存在，ImGui 中文将显示为 ?: {0}", CHINESE_FONT_PATH);
        }

        ImGui::StyleColorsDark();

        auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
        const bool glfwOk = ImGui_ImplGlfw_InitForOpenGL(window, true);
        const bool glOk = ImGui_ImplOpenGL3_Init(GLSL_VERSION);

        OI_ASSERT(glfwOk, "Failed to initialize ImGui GLFW backend!");
        OI_ASSERT(glOk, "Failed to initialize ImGui OpenGL3 backend!");
        m_initialized = glfwOk && glOk;
    }

    void ImGuiLayer::OnDetach()
    {
        if (!m_initialized) {
            return;
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_initialized = false;
    }

    void ImGuiLayer::Begin()
    {
        if (!m_initialized) {
            return;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {
        if (!m_initialized) {
            return;
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiLayer::OnEvent(Event& event)
    {
        if (!m_blockEvents || !m_initialized) {
            return;
        }

        // UI 正在占用鼠标/键盘时标记事件已处理，阻止其进入下层游戏逻辑
        const ImGuiIO& io = ImGui::GetIO();
        event.handled |= event.IsInCategory(EventCategoryMouse) && io.WantCaptureMouse;
        event.handled |= event.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard;
    }
}
