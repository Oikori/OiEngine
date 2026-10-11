#include "Application.h"

#include <mutex>
#include <filesystem>

#include "Timer.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/UI/ImGuiLayer.h"

namespace OiEngine {
	Application::~Application()
	{
	    for (Layer* layer : m_layerStack) {
	        layer->OnDetach();
	        delete layer;
	    }
		Renderer::Shutdown();
	}

    void Application::Init(const ApplicationSpecification& specification)
	{
	    m_specification = specification;
	    // Set working directory here
	    if (!m_specification.workingDirectory.empty())
	        std::filesystem::current_path(m_specification.workingDirectory);

	    m_window = Window::Create(WindowProps(m_specification.name));
	    m_window->SetEventCallback(OI_BIND_EVENT_FN(Application::OnEvent));

	    Renderer::Init();

	    // UI 调试层：作为 overlay 恒居栈顶，业务层可直接用 ImGui:: API 绘制调试界面
	    m_imguiLayer = new ImGuiLayer();
	    PushOverlay(m_imguiLayer);
	}

	void Application::PushLayer(Layer* layer)
	{
		m_layerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* overlay)
	{
		m_layerStack.PushOverlay(overlay);
		overlay->OnAttach();
	}

	void Application::Close()
	{
		m_running = false;
	}

    Application& Application::Get()
	{
	    static Application app;
	    return app;
	}

	void Application::SubmitToMainThread(const std::function<void()>& function)
	{
		std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);

		m_MainThreadQueue.emplace_back(function);
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(OI_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(OI_BIND_EVENT_FN(Application::OnWindowResize));

		for (auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it) {
			if (e.handled) {
				break;
			}
			(*it)->OnEvent(e);
		}
	}

	void Application::Run()
	{
	    Timer::Init();
		while (m_running) {
			ExecuteMainThreadQueue();

			if (!m_minimized) {
				for (Layer* layer : m_layerStack)
					layer->OnUpdate();

				// UI 在所有场景之后、SwapBuffers 之前绘制
				m_imguiLayer->Begin();
				for (Layer* layer : m_layerStack)
					layer->OnImGuiRender();
				m_imguiLayer->End();
			}

		    Timer::Update();
			m_window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		if (e.GetWidth() == 0 || e.GetHeight() == 0) {
			m_minimized = true;
			return false;
		}

		m_minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());

		return false;
	}

	void Application::ExecuteMainThreadQueue()
	{
		std::scoped_lock<std::mutex> lock(m_MainThreadQueueMutex);

		for (auto& func : m_MainThreadQueue)
			func();

		m_MainThreadQueue.clear();
	}
}
