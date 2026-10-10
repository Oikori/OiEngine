#include "GraphicsContext.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLContext.h"

namespace OiEngine {
	Scope<GraphicsContext> GraphicsContext::Create(void* window)
	{
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:    OI_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateScope<OpenGLContext>(window);
		}

		OI_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
