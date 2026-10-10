#include "OpenGLFramebuffer.h"

#include <glad/glad.h>

namespace OiEngine {
	static const uint32_t s_MaxFramebufferSize = 8192;

	namespace Utils {
		static GLenum TextureTarget(bool multisampled)
		{
			return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		}

		static void CreateTextures(bool multisampled, uint32_t* outID, uint32_t count)
		{
			glCreateTextures(TextureTarget(multisampled), count, outID);
		}

		static void BindTexture(bool multisampled, uint32_t id)
		{
			glBindTexture(TextureTarget(multisampled), id);
		}

		static void AttachColorTexture(uint32_t id, int samples, GLenum internalFormat, GLenum format, uint32_t width, uint32_t height, int index)
		{
			bool multisampled = samples > 1;
			if (multisampled) {
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
			} else {
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);
		}

		static void AttachDepthTexture(uint32_t id, int samples, GLenum format, GLenum attachmentType, uint32_t width, uint32_t height)
		{
			bool multisampled = samples > 1;
			if (multisampled) {
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);
			} else {
				glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, TextureTarget(multisampled), id, 0);
		}

		static bool IsDepthFormat(FramebufferTextureFormat format)
		{
			switch (format) {
				case FramebufferTextureFormat::DEPTH24STENCIL8:  return true;
			}

			return false;
		}

		static GLenum HazelFBTextureFormatToGL(FramebufferTextureFormat format)
		{
			switch (format) {
				case FramebufferTextureFormat::RGBA8:       return GL_RGBA8;
				case FramebufferTextureFormat::RED_INTEGER: return GL_RED_INTEGER;
			}

			OI_ASSERT(false);
			return 0;
		}

	}

	OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec): m_specification(spec)
	{
		for (const auto &spec : m_specification.attachments.attachments) {
			if (!Utils::IsDepthFormat(spec.textureFormat)) {
			    m_colorAttachmentSpecifications.emplace_back(spec);
			} else {
			    m_depthAttachmentSpecification = spec;
			}
		}

		Invalidate();
	}

	OpenGLFramebuffer::~OpenGLFramebuffer()
	{
		glDeleteFramebuffers(1, &m_rendererID);
		glDeleteTextures(m_colorAttachments.size(), m_colorAttachments.data());
		glDeleteTextures(1, &m_depthAttachment);
	}

	void OpenGLFramebuffer::Invalidate()
	{
		if (m_rendererID) {
			glDeleteFramebuffers(1, &m_rendererID);
			glDeleteTextures(m_colorAttachments.size(), m_colorAttachments.data());
			glDeleteTextures(1, &m_depthAttachment);
			
			m_colorAttachments.clear();
			m_depthAttachment = 0;
		}

		glCreateFramebuffers(1, &m_rendererID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_rendererID);

		bool multisample = m_specification.samples > 1;

		// Attachments
		if (m_colorAttachmentSpecifications.size()) {
			m_colorAttachments.resize(m_colorAttachmentSpecifications.size());
			Utils::CreateTextures(multisample, m_colorAttachments.data(), m_colorAttachments.size());

			for (size_t i = 0; i < m_colorAttachments.size(); i++) {
				Utils::BindTexture(multisample, m_colorAttachments[i]);
				switch (m_colorAttachmentSpecifications[i].textureFormat) {
					case FramebufferTextureFormat::RGBA8:
						Utils::AttachColorTexture(m_colorAttachments[i], m_specification.samples, GL_RGBA8, GL_RGBA, m_specification.width, m_specification.height, i);
						break;
					case FramebufferTextureFormat::RED_INTEGER:
						Utils::AttachColorTexture(m_colorAttachments[i], m_specification.samples, GL_R32I, GL_RED_INTEGER, m_specification.width, m_specification.height, i);
						break;
				}
			}
		}

		if (m_depthAttachmentSpecification.textureFormat != FramebufferTextureFormat::None)
		{
			Utils::CreateTextures(multisample, &m_depthAttachment, 1);
			Utils::BindTexture(multisample, m_depthAttachment);
			switch (m_depthAttachmentSpecification.textureFormat) {
				case FramebufferTextureFormat::DEPTH24STENCIL8:
					Utils::AttachDepthTexture(m_depthAttachment, m_specification.samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_specification.width, m_specification.height);
					break;
			}
		}

		if (m_colorAttachments.size() > 1) {
			OI_ASSERT(m_colorAttachments.size() <= 4);
			GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
			glDrawBuffers(m_colorAttachments.size(), buffers);
		} else if (m_colorAttachments.empty()) {
			// Only depth-pass
			glDrawBuffer(GL_NONE);
		}

		OI_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFramebuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_rendererID);
		glViewport(0, 0, m_specification.width, m_specification.height);
	}

	void OpenGLFramebuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > s_MaxFramebufferSize || height > s_MaxFramebufferSize) {
			OI_WARN("Attempted to rezize framebuffer to {0}, {1}", width, height);
			return;
		}
		m_specification.width = width;
		m_specification.height = height;
		
		Invalidate();
	}

	int OpenGLFramebuffer::ReadPixel(uint32_t attachmentIndex, int x, int y)
	{
		OI_ASSERT(attachmentIndex < m_colorAttachments.size());

		glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);
		int pixelData;
		glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
		return pixelData;

	}

	void OpenGLFramebuffer::ClearAttachment(uint32_t attachmentIndex, int value)
	{
		OI_ASSERT(attachmentIndex < m_colorAttachments.size());

		auto& spec = m_colorAttachmentSpecifications[attachmentIndex];
		glClearTexImage(m_colorAttachments[attachmentIndex], 0,
			Utils::HazelFBTextureFormatToGL(spec.textureFormat), GL_INT, &value);
	}

}
