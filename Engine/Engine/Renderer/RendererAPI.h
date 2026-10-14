#pragma once

#include "VertexArray.h"
#include "Engine/Math.h"

namespace OiEngine {
	class RendererAPI {
	public:
		enum class API {
			None = 0, OpenGL = 1
		};
	public:
		virtual ~RendererAPI() = default;

		virtual void Init() = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void Clear() = 0;
		// 只清深度（阴影 pass 等 depth-only 帧缓冲）
		virtual void ClearDepth() = 0;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) = 0;
		
		// 把 2D 纹理绑定到指定纹理单元（供着色器采样，如阴影贴图）
		virtual void BindTexture2D(uint32_t slot, uint32_t textureRendererID) = 0;

		virtual void SetLineWidth(float width) = 0;

		static API GetAPI();
		static Scope<RendererAPI> Create();
	};

}
