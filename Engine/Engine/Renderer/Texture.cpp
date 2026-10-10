#include "Texture.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace OiEngine {
	Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification)
	{
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:    OI_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(specification);
		}

		OI_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:    OI_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLTexture2D>(path);
		}

		OI_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
