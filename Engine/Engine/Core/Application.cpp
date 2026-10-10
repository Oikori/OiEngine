#include "Application.h"

#include <mutex>
#include <filesystem>

#include "Timer.h"
#include "Engine/Renderer/Renderer.h"

namespace OiEngine {
	Application::~Application()
	{
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
	}

	void Application::PushLayer(Layer* layer)
	{
		m_layers.emplace_back(layer);
		layer->OnAttach();
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

		for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
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
				for (Layer* layer : m_layers)
					layer->OnUpdate();
			}

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
