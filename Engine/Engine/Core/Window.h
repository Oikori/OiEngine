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
	class OI_API Window {
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

		virtual void* GetNativeWindow() const = 0;

		static Scope<Window> Create(const WindowProps& props = WindowProps());
	};
}
