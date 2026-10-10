#include "WindowsWindow.h"

#include <GLFW/glfw3.h>

#include "Engine/Events/ApplicationEvent.h"
#include "Engine/Events/MouseEvent.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Renderer/Renderer.h"

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
}
