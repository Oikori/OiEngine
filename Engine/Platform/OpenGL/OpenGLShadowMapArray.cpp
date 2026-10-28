#include "OpenGLShadowMapArray.h"

#include <glad/glad.h>

#include "Engine/Renderer/RenderCommand.h"

namespace OiEngine {
	OpenGLShadowMapArray::OpenGLShadowMapArray(uint32_t size, uint32_t layerCount)
		: m_size(size), m_layerCount(layerCount)
	{
		OI_ASSERT(layerCount > 0, "ShadowMapArray needs at least one layer!");

		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_depthTexture);
		glTextureStorage3D(m_depthTexture, 1, GL_DEPTH_COMPONENT24, m_size, m_size, m_layerCount);

		// 与 ShadowMap 一致：NEAREST（不做深度插值）、边界外返回 1.0（视锥外不产生阴影）
		glTextureParameteri(m_depthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(m_depthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(m_depthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(m_depthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		const float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTextureParameterfv(m_depthTexture, GL_TEXTURE_BORDER_COLOR, borderColor);

		glCreateFramebuffers(1, &m_framebuffer);
		// 只有深度附件；具体挂哪一层在 BeginLayer 里切换
		glNamedFramebufferTextureLayer(m_framebuffer, GL_DEPTH_ATTACHMENT, m_depthTexture, 0, 0);
		glNamedFramebufferDrawBuffer(m_framebuffer, GL_NONE);
		glNamedFramebufferReadBuffer(m_framebuffer, GL_NONE);

		OI_ASSERT(glCheckNamedFramebufferStatus(m_framebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
			"Shadow map array framebuffer is incomplete!");
	}

	OpenGLShadowMapArray::~OpenGLShadowMapArray()
	{
		glDeleteFramebuffers(1, &m_framebuffer);
		glDeleteTextures(1, &m_depthTexture);
	}

	void OpenGLShadowMapArray::BeginLayer(uint32_t layer)
	{
		OI_ASSERT(layer < m_layerCount, "Shadow map layer index out of range!");

		glNamedFramebufferTextureLayer(m_framebuffer, GL_DEPTH_ATTACHMENT, m_depthTexture, 0, layer);
		glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
		glViewport(0, 0, static_cast<GLsizei>(m_size), static_cast<GLsizei>(m_size));
		RenderCommand::ClearDepth();
	}

	void OpenGLShadowMapArray::EndLayer()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLShadowMapArray::BindDepthTexture(uint32_t slot) const
	{
		RenderCommand::BindTexture2DArray(slot, m_depthTexture);
	}
}
