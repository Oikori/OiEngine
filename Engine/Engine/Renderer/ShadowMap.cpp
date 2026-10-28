#include "ShadowMap.h"

#include "RenderCommand.h"

namespace OiEngine {
	ShadowMap::ShadowMap(uint32_t size): m_size(size)
	{
		FramebufferSpecification spec;
		spec.width = size;
		spec.height = size;
		// 仅深度附件（DEPTH24 带有阴影采样所需的 NEAREST + 边界外 1.0 参数，见 OpenGLFramebuffer）
		spec.attachments = { FramebufferTextureFormat::DEPTH24 };
		m_framebuffer = Framebuffer::Create(spec);
	}

	void ShadowMap::Resize(uint32_t size)
	{
		if (size == m_size) {
			return;
		}

		m_size = size;
		m_framebuffer->Resize(size, size);
	}

	void ShadowMap::Begin()
	{
		m_framebuffer->Bind();
		RenderCommand::ClearDepth();
	}

	void ShadowMap::End()
	{
		m_framebuffer->Unbind();
	}

	void ShadowMap::BindDepthTexture(uint32_t slot) const
	{
		RenderCommand::BindTexture2D(slot, m_framebuffer->GetDepthAttachmentRendererID());
	}

	uint32_t ShadowMap::GetDepthTextureRendererID() const
	{
		return m_framebuffer->GetDepthAttachmentRendererID();
	}
}
