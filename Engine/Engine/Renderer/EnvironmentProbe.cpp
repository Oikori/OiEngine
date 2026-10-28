#include "EnvironmentProbe.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLEnvironmentProbe.h"

namespace OiEngine {
	Ref<EnvironmentProbe> EnvironmentProbe::Create(uint32_t captureSize, uint32_t irradianceSize, uint32_t prefilterSize)
	{
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:    OI_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLEnvironmentProbe>(captureSize, irradianceSize, prefilterSize);
		}

		OI_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
