#pragma once

#include "Base.h"
#include "KeyCodes.h"
#include "MouseCodes.h"
#include "Engine/Math.h"

namespace OiEngine {
	class OI_API Input {
	public:
		static bool IsKeyPressed(KeyCode key);

		static bool IsMouseButtonPressed(MouseCode button);
		static Math::Vector2 GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
	};
}
