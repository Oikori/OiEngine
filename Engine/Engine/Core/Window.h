#pragma once

#include <functional>

#include "Base.h"
#include "Engine/Events/Event.h"

namespace OiEngine {
	struct WindowProps {
		std::string title;
		uint32_t width = 1600;
		uint32_t height = 900;
	};

	// Interface representing a desktop system based Window
	class OI_ENGINE_API Window {
	public:
		using EventCallbackFn = std::function<void(Event&)>;

		virtual ~Window() = default;

		virtual void OnUpdate() = 0;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		// Window attributes
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
		virtual void SetVSync(bool enabled) = 0;
		virtual bool IsVSync() const = 0;

		// 是否让窗口接受输入法（IME）。**默认实现什么都不做**，Windows 平台在创建窗口时就把 IME 解绑：
		// 中文输入法激活时按键会先被组词流程吃掉，GLFW 收不到 WM_KEYDOWN，游戏里表现为「WASD / 1·2·3 没反应」。
		// 真要做文本输入时（ImGui InputText）再显式打开；注意：在 InputText 已经激活时挂回 IME，
		// 系统会重挂输入法上下文并让 ImGui 掉一次焦点（输入框需要再点一下），所以建议在显示输入框之前打开。
		virtual void SetImeEnabled(bool /*enabled*/) {}

		virtual void* GetNativeWindow() const = 0;

		// 原生窗口句柄（Win32 上是 HWND，其它平台可能为空）：IME 候选窗定位、系统级窗口操作要用它。
		// 注意与 GetNativeWindow() 区分——后者是 GLFW 的窗口对象
		virtual void* GetNativeDisplayHandle() const { return nullptr; }

		static Scope<Window> Create(const WindowProps& props = WindowProps());
	};
}
