#pragma once

#include "RendererAPI.h"

namespace OiEngine {
	class OI_API RenderCommand {
	public:
		static void Init()
		{
			s_RendererAPI->Init();
		}

		static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

		static void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		static void Clear()
		{
			s_RendererAPI->Clear();
		}

		static void ClearDepth()
		{
			s_RendererAPI->ClearDepth();
		}

		static void SetDepthTest(bool enabled)
		{
			s_RendererAPI->SetDepthTest(enabled);
		}

		static void SetBlending(bool enabled)
		{
			s_RendererAPI->SetBlending(enabled);
		}

		// 内存屏障：compute 写入 SSBO 后，让后续绘制（片元着色器读 SSBO）能看到结果
		static void MemoryBarrierShaderStorage()
		{
			s_RendererAPI->MemoryBarrierShaderStorage();
		}

		static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0)
		{
			s_RendererAPI->DrawIndexed(vertexArray, indexCount);
		}

		static void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
		{
			s_RendererAPI->DrawLines(vertexArray, vertexCount);
		}

		static void BindTexture2D(uint32_t slot, uint32_t textureRendererID)
		{
			s_RendererAPI->BindTexture2D(slot, textureRendererID);
		}

		static void BindTexture2DArray(uint32_t slot, uint32_t textureRendererID)
		{
			s_RendererAPI->BindTexture2DArray(slot, textureRendererID);
		}

		static void BindTextureCube(uint32_t slot, uint32_t textureRendererID)
		{
			s_RendererAPI->BindTextureCube(slot, textureRendererID);
		}

		static void BindTexture3D(uint32_t slot, uint32_t textureRendererID)
		{
			s_RendererAPI->BindTexture3D(slot, textureRendererID);
		}

		static void SetLineWidth(float width)
		{
			s_RendererAPI->SetLineWidth(width);
		}
	private:
		static Scope<RendererAPI> s_RendererAPI;
	};

}
