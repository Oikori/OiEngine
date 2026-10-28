#pragma once

#include "Engine/Renderer/RendererAPI.h"

namespace OiEngine {
	class OpenGLRendererAPI : public RendererAPI {
	public:
		void Init() override;
		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		void SetClearColor(const glm::vec4& color) override;
		void Clear() override;
		void ClearDepth() override;
		void SetDepthTest(bool enabled) override;
		void SetBlending(bool enabled) override;
		void MemoryBarrierShaderStorage() override;

		void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) override;
		void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) override;
		
		void BindTexture2D(uint32_t slot, uint32_t textureRendererID) override;
		void BindTexture2DArray(uint32_t slot, uint32_t textureRendererID) override;
		void BindTextureCube(uint32_t slot, uint32_t textureRendererID) override;
		void BindTexture3D(uint32_t slot, uint32_t textureRendererID) override;

		void SetLineWidth(float width) override;
	};
}
