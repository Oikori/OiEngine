#pragma once

#include "Engine/Renderer/VertexArray.h"

namespace OiEngine {
	class OpenGLVertexArray : public VertexArray {
	public:
		OpenGLVertexArray();
		~OpenGLVertexArray() override;

		void Bind() const override;
		void Unbind() const override;

		void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) override;
		void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) override;

		const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const override { return m_vertexBuffers; }
		const Ref<IndexBuffer>& GetIndexBuffer() const override { return m_indexBuffer; }
	private:
		uint32_t m_rendererID;
		uint32_t m_vertexBufferIndex = 0;
		std::vector<Ref<VertexBuffer>> m_vertexBuffers;
		Ref<IndexBuffer> m_indexBuffer;
	};
}
