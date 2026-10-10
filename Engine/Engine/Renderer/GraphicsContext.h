#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine {
	class GraphicsContext {
	public:
		virtual ~GraphicsContext() = default;

		virtual void SwapBuffers() = 0;

		static Scope<GraphicsContext> Create(void* window);
	};
}
