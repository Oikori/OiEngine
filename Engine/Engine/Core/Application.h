#pragma once

#include <functional>
#include <vector>

#include "Base.h"
#include "Window.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/ApplicationEvent.h"
#include "Engine/UI/ImGuiLayer.h"

namespace OiEngine {
    struct ApplicationCommandLineArgs {
        int count = 0;
        char** args = nullptr;

        const char* operator[](int index) const
        {
            OI_ASSERT(index < Count);
            return args[index];
        }
    };

    struct ApplicationSpecification
    {
        std::string name = "Application";
        std::string workingDirectory;
        ApplicationCommandLineArgs commandLineArgs;
        int maxFixedUpdatePerFrame = 5;
    };

    class OI_API Application final {
    public:
        ~Application();

        void OnEvent(Event& e);

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);

        Window& GetWindow() const { return *m_window; }

        void Init(const ApplicationSpecification& specification);
        void Run();
        void Close();

        static Application& Get();

        const ApplicationSpecification& GetSpecification() const { return m_specification; }

        void SubmitToMainThread(const std::function<void()>& function);
    private:
        Application() = default;

        bool OnWindowClose(WindowCloseEvent& e);
        bool OnWindowResize(WindowResizeEvent& e);

        void ExecuteMainThreadQueue();

    private:
        ApplicationSpecification m_specification;
        Scope<Window> m_window;
        bool m_running = true;
        bool m_minimized = false;
        LayerStack m_layerStack;
        ImGuiLayer* m_imguiLayer = nullptr;  // 由 m_layerStack 持有，随 Application 析构统一释放
        float m_fixedUpdateAccumulator = 0.0f;

        std::vector<std::function<void()>> m_MainThreadQueue;
        std::mutex m_MainThreadQueueMutex;
    };
}
