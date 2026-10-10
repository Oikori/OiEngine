#pragma once

#include "Engine/Core/Window.h"
#include "Engine/Renderer/GraphicsContext.h"

namespace OiEngine {
	class OI_API WindowsWindow : public Window {
	public:
        explicit WindowsWindow(const WindowProps& props);
		~WindowsWindow() override;

		void OnUpdate() override;

		unsigned int GetWidth() const override { return m_width; }
		unsigned int GetHeight() const override { return m_height; }

		// Window attributes
		void SetEventCallback(const EventCallbackFn& callback) override { m_eventCallback = callback; }
		void SetVSync(bool enabled) override;
		bool IsVSync() const override;

		void* GetNativeWindow() const override { return m_window; }
	private:
		virtual void Init(const WindowProps& props);
		virtual void Shutdown();
	private:
		void* m_window = nullptr;
		Scope<GraphicsContext> m_context;
		std::string m_title;
		unsigned int m_width, m_height;
		bool m_vsync;

		EventCallbackFn m_eventCallback;
	};
}
