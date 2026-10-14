#pragma once

#include "Engine/Renderer/StorageBuffer.h"

namespace OiEngine {
	class OpenGLStorageBuffer : public StorageBuffer {
	public:
		OpenGLStorageBuffer(uint32_t size, uint32_t binding);
		virtual ~OpenGLStorageBuffer();

		void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
		void GetData(void* data, uint32_t size, uint32_t offset = 0) const override;
		void Clear(uint32_t offset, uint32_t size, uint32_t value = 0) override;
	private:
		uint32_t m_RendererID = 0;
	};
}
