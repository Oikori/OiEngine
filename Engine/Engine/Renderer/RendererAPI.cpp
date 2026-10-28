#include "RendererAPI.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace OiEngine {
    RendererAPI::API RendererAPI::GetAPI()
    {
        return API::OpenGL;
    }

	Scope<RendererAPI> RendererAPI::Create()
	{
		switch (GetAPI()) {
			case RendererAPI::API::None:    OI_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateScope<OpenGLRendererAPI>();
		}

		OI_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
