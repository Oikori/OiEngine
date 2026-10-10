#pragma once

#include "Engine/Renderer/Texture.h"

#include <glad/glad.h>

namespace OiEngine {
	class OpenGLTexture2D : public Texture2D {
	public:
		OpenGLTexture2D(const TextureSpecification& specification);
		OpenGLTexture2D(const std::string& path);
		~OpenGLTexture2D() override;

		const TextureSpecification& GetSpecification() const override { return m_specification; }

		uint32_t GetWidth() const override { return m_width;  }
		uint32_t GetHeight() const override { return m_height; }
		uint32_t GetRendererID() const override { return m_rendererID; }

		const std::string& GetPath() const override { return m_path; }
		
		void SetData(void* data, uint32_t size) override;

		void Bind(uint32_t slot = 0) const override;

		bool IsLoaded() const override { return m_loaded; }

		bool operator==(const Texture& other) const override
		{
			return m_rendererID == other.GetRendererID();
		}
	private:
		TextureSpecification m_specification;

		std::string m_path;
		bool m_loaded = false;
		uint32_t m_width, m_height;
		uint32_t m_rendererID;
		GLenum m_internalFormat, m_dataFormat;
	};

}
