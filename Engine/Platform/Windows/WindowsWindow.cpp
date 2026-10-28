#include "WindowsWindow.h"

#include <GLFW/glfw3.h>

// 取 HWND 需要 GLFW 的平台扩展（必须在包含前定义），IME 相关 API 在 imm.h
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <imm.h>

#include "Engine/Events/ApplicationEvent.h"
#include "Engine/Events/MouseEvent.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Renderer/Renderer.h"

#include "WindowsKeyboard.h"

namespace OiEngine {
	static uint8_t s_GLFWWindowCount = 0;

	static void GLFWErrorCallback(int error, const char* description)
	{
		OI_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	WindowsWindow::WindowsWindow(const WindowProps& props)
        : m_title(props.title), m_width(props.width), m_height(props.height), m_vsync(true)
	{
        WindowsWindow::Init(props);
	}

	WindowsWindow::~WindowsWindow()
	{
        WindowsWindow::Shutdown();
	}

	void WindowsWindow::Init(const WindowProps& props)
	{
		OI_INFO("Creating window {0} ({1}, {2})", props.title, props.width, props.height);

		if (s_GLFWWindowCount == 0) {
			int success = glfwInit();
			OI_ASSERT(success, "Could not initialize GLFW!");
			glfwSetErrorCallback(GLFWErrorCallback);
		}

#ifdef OI_DEBUG
	    if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL) {
	        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
	    }
#endif
		GLFWwindow* win = glfwCreateWindow((int)props.width, (int)props.height, m_title.c_str(), nullptr, nullptr);
	    m_window = win;
		++s_GLFWWindowCount;

		// 游戏窗口默认不挂输入法：中文输入法激活时，WASD / 数字键会先被组词流程吃掉，
		// GLFW 收不到 WM_KEYDOWN，游戏里就表现为「按键没反应」。
		// 真要做文本输入的工程自己调 SetImeEnabled(true)（见 Window.h 的说明与 docs/ImGui与输入.md）
		m_hwnd = glfwGetWin32Window(win);
		SetImeEnabled(false);
		OI_INFO("窗口已禁用 IME：中文输入法不会再吞掉 WASD / 数字键");

		m_context = GraphicsContext::Create(win);

		glfwSetWindowUserPointer(win, this);
		SetVSync(true);

		// Set GLFW callbacks
		glfwSetWindowSizeCallback(win, [](GLFWwindow* window, int width, int height) {
			auto* pWin = (WindowsWindow*)glfwGetWindowUserPointer(window);
			pWin->m_width = width;
			pWin->m_height = height;

		    if (pWin->m_eventCallback) {
			    WindowResizeEvent event(width, height);
			    pWin->m_eventCallback(event);
		    }
		});

		glfwSetWindowCloseCallback(win, [](GLFWwindow* window) {
			auto* pWin = (WindowsWindow*)glfwGetWindowUserPointer(window);
		    if (pWin->m_eventCallback) {
			    WindowCloseEvent event;
		        pWin->m_eventCallback(event);
		    }
		});

		glfwSetKeyCallback(win, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			auto* pWin = (WindowsWindow*)glfwGetWindowUserPointer(window);

			// 游戏按键由物理通道（WindowsKeyboard）统一上报：GLFW 这条会漏收（输入法吞键）、
			// 也可能把「漏收抬起后的再次按下」判成 REPEAT，所以整类键的事件一律丢弃，避免双份 / 错判
			if (Detail::IsPhysicalKey(key)) {
				return;
			}

			switch (action) {
				case GLFW_PRESS: {
		            if (pWin->m_eventCallback) {
					    KeyPressedEvent event(key, false);
		                pWin->m_eventCallback(event);
		            }
					break;
				}
				case GLFW_RELEASE: {
				    if (pWin->m_eventCallback) {
				        KeyReleasedEvent event(key);
                        pWin->m_eventCallback(event);
				    }
					break;
				}
				case GLFW_REPEAT: {
				    if (pWin->m_eventCallback) {
				        KeyPressedEvent event(key, true);
                        pWin->m_eventCallback(event);
				    }
					break;
				}
			}
		});

		glfwSetCharCallback(win, [](GLFWwindow* window, unsigned int keycode) {
			auto* pWin = (WindowsWindow*)glfwGetWindowUserPointer(window);
		    if (pWin->m_eventCallback) {
		        KeyTypedEvent event(keycode);
                pWin->m_eventCallback(event);
		    }
		});

		glfwSetMouseButtonCallback(win, [](GLFWwindow* window, int button, int action, int mods) {
			auto* pWin = (WindowsWindow*)glfwGetWindowUserPointer(window);

			switch (action) {
				case GLFW_PRESS: {
				    if (pWin->m_eventCallback) {
				        MouseButtonPressedEvent event(button);
                        pWin->m_eventCallback(event);
				    }
					break;
				}
				case GLFW_RELEASE: {
				    if (pWin->m_eventCallback) {
				        MouseButtonReleasedEvent event(button);
                        pWin->m_eventCallback(event);
				    }
					break;
				}
			}
		});

		glfwSetScrollCallback(win, [](GLFWwindow* window, double xOffset, double yOffset) {
			auto* pWin = (WindowsWindow*)glfwGetWindowUserPointer(window);
		    if (pWin->m_eventCallback) {
		        MouseScrolledEvent event((float)xOffset, (float)yOffset);
                pWin->m_eventCallback(event);
		    }
		});

		glfwSetCursorPosCallback(win, [](GLFWwindow* window, double xPos, double yPos) {
			auto* pWin = (WindowsWindow*)glfwGetWindowUserPointer(window);
		    if (pWin->m_eventCallback) {
		        MouseMovedEvent event((float)xPos, (float)yPos);
                pWin->m_eventCallback(event);
		    }
		});
	}

	void WindowsWindow::Shutdown()
	{
		glfwDestroyWindow(static_cast<GLFWwindow *>(m_window));
		--s_GLFWWindowCount;

		if (s_GLFWWindowCount == 0) {
			glfwTerminate();
		}
	}

	void WindowsWindow::OnUpdate()
	{
		glfwPollEvents();

		// 输入法（或别的钩子）可能把按键吃掉，GLFW 收不到 WM_KEYDOWN：
		// 每帧用物理按键状态产生按键事件（见 WindowsKeyboard.h）
		Detail::SyncPhysicalKeys(glfwGetWindowAttrib(static_cast<GLFWwindow*>(m_window), GLFW_FOCUSED) == GLFW_TRUE,
		                         m_eventCallback);

		m_context->SwapBuffers();
	}

	void WindowsWindow::SetVSync(bool enabled)
	{
		if (enabled) {
		    glfwSwapInterval(1);
		} else {
		    glfwSwapInterval(0);
		}

		m_vsync = enabled;
	}

	bool WindowsWindow::IsVSync() const
	{
		return m_vsync;
	}

	void WindowsWindow::SetImeEnabled(bool enabled)
	{
		if (m_hwnd == nullptr) {
			return;
		}

		// enabled = IACE_DEFAULT：把默认输入法上下文挂回窗口（可以打中文，候选窗由 ImGui 定位）；
		// 否则传 0：把上下文从窗口上解绑，输入法完全不接触这个窗口的按键
		ImmAssociateContextEx(static_cast<HWND>(m_hwnd), nullptr, enabled ? IACE_DEFAULT : 0);
		OI_TRACE("窗口 IME {0}", enabled ? "已启用" : "已禁用");
	}
}
