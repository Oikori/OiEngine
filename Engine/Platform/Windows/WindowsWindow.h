#pragma once

#include "Engine/Core/Window.h"
#include "Engine/Renderer/GraphicsContext.h"

namespace OiEngine {
	class OI_ENGINE_API WindowsWindow : public Window {
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

		// IME：初始化时把输入法从窗口上解绑，避免中文输入法吞掉游戏按键
		void SetImeEnabled(bool enabled) override;

		void* GetNativeWindow() const override { return m_window; }
		void* GetNativeDisplayHandle() const override { return m_hwnd; }
	private:
		virtual void Init(const WindowProps& props);
		virtual void Shutdown();
	private:
		void* m_window = nullptr;
		void* m_hwnd = nullptr;   // Win32 HWND（glfwGetWin32Window），IME 相关 API 要用
		Scope<GraphicsContext> m_context;
		std::string m_title;
		unsigned int m_width, m_height;
		bool m_vsync;

		EventCallbackFn m_eventCallback;
	};
}
