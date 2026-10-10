#include "OpenGLTexture.h"

#include <stb_image.h>

namespace OiEngine {
	namespace Utils {
		static GLenum HazelImageFormatToGLDataFormat(ImageFormat format)
		{
			switch (format) {
				case ImageFormat::RGB8:  return GL_RGB;
				case ImageFormat::RGBA8: return GL_RGBA;
			}

			OI_ASSERT(false);
			return 0;
		}
		
		static GLenum HazelImageFormatToGLInternalFormat(ImageFormat format)
		{
			switch (format) {
			    case ImageFormat::RGB8:  return GL_RGB8;
			    case ImageFormat::RGBA8: return GL_RGBA8;
			}

			OI_ASSERT(false);
			return 0;
		}

	}

	OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification)
		: m_specification(specification), m_width(m_specification.width), m_height(m_specification.height)
	{
		m_internalFormat = Utils::HazelImageFormatToGLInternalFormat(m_specification.format);
		m_dataFormat = Utils::HazelImageFormatToGLDataFormat(m_specification.format);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererID);
		glTextureStorage2D(m_rendererID, 1, m_internalFormat, m_width, m_height);

		glTextureParameteri(m_rendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_rendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path): m_path(path)
	{
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
			
		if (data) {
			m_loaded = true;

			m_width = width;
			m_height = height;

			GLenum internalFormat = 0, dataFormat = 0;
			if (channels == 4) {
				internalFormat = GL_RGBA8;
				dataFormat = GL_RGBA;
			} else if (channels == 3) {
				internalFormat = GL_RGB8;
				dataFormat = GL_RGB;
			}

			m_internalFormat = internalFormat;
			m_dataFormat = dataFormat;

			OI_ASSERT(internalFormat & dataFormat, "Format not supported!");

			glCreateTextures(GL_TEXTURE_2D, 1, &m_rendererID);
			glTextureStorage2D(m_rendererID, 1, internalFormat, m_width, m_height);

			glTextureParameteri(m_rendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_rendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(m_rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTextureSubImage2D(m_rendererID, 0, 0, 0, m_width, m_height, dataFormat, GL_UNSIGNED_BYTE, data);

			stbi_image_free(data);
		}
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		glDeleteTextures(1, &m_rendererID);
	}

	void OpenGLTexture2D::SetData(void* data, uint32_t size)
	{
		uint32_t bpp = m_dataFormat == GL_RGBA ? 4 : 3;
		OI_ASSERT(size == m_width * m_height * bpp, "Data must be entire texture!");
		glTextureSubImage2D(m_rendererID, 0, 0, 0, m_width, m_height, m_dataFormat, GL_UNSIGNED_BYTE, data);
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_rendererID);
	}
}
