#include "ShadowMapArray.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLShadowMapArray.h"

namespace OiEngine {
	Ref<ShadowMapArray> ShadowMapArray::Create(uint32_t size, uint32_t layerCount)
	{
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:    OI_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLShadowMapArray>(size, layerCount);
		}

		OI_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
