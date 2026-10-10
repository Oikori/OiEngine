#include "VertexArray.h"

#include "RendererAPI.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace OiEngine {
	Ref<VertexArray> VertexArray::Create()
	{
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:    OI_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLVertexArray>();
		}

		OI_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
