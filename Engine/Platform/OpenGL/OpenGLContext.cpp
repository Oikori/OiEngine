#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace OiEngine {
	OpenGLContext::OpenGLContext(void* window): m_window(window)
	{
	    OI_ASSERT(window, "Window handle is null!");
	    glfwMakeContextCurrent(static_cast<GLFWwindow *>(m_window));
	    int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	    OI_ASSERT(status, "Failed to initialize Glad!");

	    OI_INFO("OpenGL Info:");
	    OI_INFO("  Vendor: {0}", glGetString(GL_VENDOR));
	    OI_INFO("  Renderer: {0}", glGetString(GL_RENDERER));
	    OI_INFO("  Version: {0}", glGetString(GL_VERSION));

	    OI_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "Hazel requires at least OpenGL version 4.5!");
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(static_cast<GLFWwindow *>(m_window));
	}
}
