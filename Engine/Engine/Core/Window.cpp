#include "Window.h"

#ifdef OI_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsWindow.h"
#endif

namespace OiEngine {
	Scope<Window> Window::Create(const WindowProps& props)
	{
	#ifdef OI_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>(props);
	#else
		OI_ASSERT(false, "Unknown platform!");
		return nullptr;
	#endif
	}
}
