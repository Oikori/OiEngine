#include "ImGuiLayer.h"

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "Engine/Core/Application.h"

namespace OiEngine {
    // 引擎要求 OpenGL 4.5 Core（见 OpenGLContext），GLSL 版本保持一致
    static const char* GLSL_VERSION = "#version 450";

    ImGuiLayer::ImGuiLayer(): Layer("ImGuiLayer") {}
    ImGuiLayer::~ImGuiLayer() = default;

    void ImGuiLayer::OnAttach()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // 键盘导航
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // 停靠布局（docking 分支）

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
