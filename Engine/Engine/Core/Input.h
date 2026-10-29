#pragma once

#include "Base.h"
#include "KeyCodes.h"
#include "MouseCodes.h"
#include "Engine/Math.h"

namespace OiEngine {
	class OI_ENGINE_API Input {
	public:
		// 轮询查询：窗口失焦时一律返回 false（游戏在后台不该跟着按键动）。
		// 注意它**不看 ImGui 是否占用键盘**——ImGui 窗口有焦点时事件会被 ImGuiLayer 拦下，
		// 但这里读的是物理按键状态，所以「ESC 之类的系统键」在游戏层轮询比等事件稳（见 ImGui与输入.md）
		static bool IsKeyPressed(KeyCode key);

		static bool IsMouseButtonPressed(MouseCode button);
		static glm::vec2 GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
	};
}
