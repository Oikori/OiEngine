#pragma once

#include "Engine/Renderer/GraphicsContext.h"

namespace OiEngine {
	class OpenGLContext : public GraphicsContext {
	public:
        explicit OpenGLContext(void* window);

		void SwapBuffers() override;
	private:
		void* m_window;
	};
}
