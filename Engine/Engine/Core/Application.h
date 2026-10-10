#pragma once

#include <functional>
#include <vector>

#include "Base.h"
#include "Window.h"
#include "Layer.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/ApplicationEvent.h"

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
    };

    class OI_API Application final {
    public:
        ~Application();

        void OnEvent(Event& e);

        void PushLayer(Layer* layer);

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
        std::vector<Layer*> m_layers;

        std::vector<std::function<void()>> m_MainThreadQueue;
        std::mutex m_MainThreadQueueMutex;
    };
}
