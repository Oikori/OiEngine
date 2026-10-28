#include <GLFW/glfw3.h>

#include "Engine/Core/Input.h"
#include "Engine/Core/Application.h"

#include "WindowsKeyboard.h"

namespace OiEngine {
	bool Input::IsKeyPressed(const KeyCode key)
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		const int glfwKey = static_cast<int>(key);

		// 游戏按键由物理通道接管（见 WindowsKeyboard.h）：GLFW 的状态可能因为输入法吞键而残留，
		// 两路混用会出现「松开了还在走」「按了没反应」，所以这里只读物理状态
		if (Detail::IsPhysicalKey(glfwKey)) {
			if (glfwGetWindowAttrib(window, GLFW_FOCUSED) != GLFW_TRUE) {
				return false;
			}
			return Detail::IsPhysicallyDown(glfwKey);
		}

		return glfwGetKey(window, static_cast<int32_t>(key)) == GLFW_PRESS;
	}

	bool Input::IsMouseButtonPressed(const MouseCode button)
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	glm::vec2 Input::GetMousePosition()
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		return { (float)xpos, (float)ypos };
	}

	float Input::GetMouseX()
	{
		return GetMousePosition().x;
	}

	float Input::GetMouseY()
	{
		return GetMousePosition().y;
	}

}
