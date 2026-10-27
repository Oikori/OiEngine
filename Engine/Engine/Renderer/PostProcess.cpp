#include "PostProcess.h"

#include "Buffer.h"
#include "Framebuffer.h"
#include "RenderCommand.h"
#include "Shader.h"
#include "VertexArray.h"
#include "Engine/Core/Application.h"

namespace OiEngine {
	Ref<Framebuffer> PostProcess::s_hdrTarget = nullptr;
	Ref<Shader> PostProcess::s_tonemapShader = nullptr;
	Ref<VertexArray> PostProcess::s_fullscreenQuad = nullptr;

	bool PostProcess::s_enabled = false;
	float PostProcess::s_exposure = 1.0f;
	uint32_t PostProcess::s_width = 0;
	uint32_t PostProcess::s_height = 0;

	void PostProcess::Init()
	{
		// 全屏四边形（顶点已是 NDC 坐标）：位置 + 纹理坐标
		float vertices[] = {
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, 1.0f, 1.0f,
			-1.0f,  1.0f, 0.0f, 1.0f,
		};
		uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

		s_fullscreenQuad = VertexArray::Create();
		Ref<VertexBuffer> vertexBuffer = VertexBuffer::Create(vertices, static_cast<uint32_t>(sizeof(vertices)));
		vertexBuffer->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" },
		});
		s_fullscreenQuad->AddVertexBuffer(vertexBuffer);
		s_fullscreenQuad->SetIndexBuffer(IndexBuffer::Create(indices, 6));

		s_tonemapShader = Shader::Create("assets/shader/engine/postprocess_tonemap.glsl");
		s_tonemapShader->Bind();
		s_tonemapShader->SetInt("u_HDRBuffer", 0);
	}

	void PostProcess::Shutdown()
	{
		s_fullscreenQuad.reset();
		s_tonemapShader.reset();
		s_hdrTarget.reset();

		s_enabled = false;
		s_width = 0;
		s_height = 0;
	}

	void PostProcess::CreateTarget(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) {
			return;
		}

		FramebufferSpecification spec;
		spec.width = width;
		spec.height = height;
		// RGBA16F：光照结果可以超过 1（为 bloom / 体积光等后处理留出高光余量）+ 深度附件
		spec.attachments = { FramebufferTextureFormat::RGBA16F, FramebufferTextureFormat::DEPTH24STENCIL8 };
		s_hdrTarget = Framebuffer::Create(spec);

		s_width = width;
		s_height = height;

		OI_INFO("HDR target created: {0} x {1} (RGBA16F + depth)", width, height);
	}

	void PostProcess::Resize(uint32_t width, uint32_t height)
	{
		if (!s_hdrTarget) {
			return;
		}
		if (width == s_width && height == s_height) {
			return;
		}

		s_hdrTarget->Resize(width, height);
		s_width = width;
		s_height = height;
	}

	void PostProcess::SetEnabled(bool enabled)
	{
		s_enabled = enabled;

		if (enabled && !s_hdrTarget) {
			const Window& window = Application::Get().GetWindow();
			CreateTarget(window.GetWidth(), window.GetHeight());
		}
	}

	uint32_t PostProcess::GetColorTextureRendererID()
	{
		return s_hdrTarget ? s_hdrTarget->GetColorAttachmentRendererID() : 0;
	}

	void PostProcess::BeginScene()
	{
		if (!s_enabled || !s_hdrTarget) {
			return;
		}

		s_hdrTarget->Bind();
		RenderCommand::Clear();
	}

	void PostProcess::EndScene()
	{
		if (!s_enabled || !s_hdrTarget) {
			return;
		}

		s_hdrTarget->Unbind();
		RenderCommand::SetViewport(0, 0, s_width, s_height);

		// 全屏 pass：关闭深度测试，避免被场景深度（默认帧缓冲的残留内容）剔除
		RenderCommand::SetDepthTest(false);

		RenderCommand::BindTexture2D(0, s_hdrTarget->GetColorAttachmentRendererID());
		s_tonemapShader->Bind();
		s_tonemapShader->SetFloat("u_Exposure", s_exposure);
		RenderCommand::DrawIndexed(s_fullscreenQuad);

		RenderCommand::SetDepthTest(true);
	}
}
