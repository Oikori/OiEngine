#pragma once

#include "Engine/Renderer/Framebuffer.h"

namespace OiEngine {
	class OpenGLFramebuffer : public Framebuffer {
	public:
		OpenGLFramebuffer(const FramebufferSpecification& spec);
		~OpenGLFramebuffer() override;

		void Invalidate();

        void Bind() override;
        void Unbind() override;

		void Resize(uint32_t width, uint32_t height) override;
		int ReadPixel(uint32_t attachmentIndex, int x, int y) override;

		void ClearAttachment(uint32_t attachmentIndex, int value) override;

		uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override { OI_ASSERT(index < m_ColorAttachments.size()); return m_colorAttachments[index]; }

		const FramebufferSpecification& GetSpecification() const override { return m_specification; }
	private:
		uint32_t m_rendererID = 0;
		FramebufferSpecification m_specification;

		std::vector<FramebufferTextureSpecification> m_colorAttachmentSpecifications;
		FramebufferTextureSpecification m_depthAttachmentSpecification = FramebufferTextureFormat::None;

		std::vector<uint32_t> m_colorAttachments;
		uint32_t m_depthAttachment = 0;
	};
}
