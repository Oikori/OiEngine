#pragma once

#include "Engine/Renderer/ShadowMapArray.h"

namespace OiEngine {
	class OpenGLShadowMapArray : public ShadowMapArray {
	public:
		OpenGLShadowMapArray(uint32_t size, uint32_t layerCount);
		virtual ~OpenGLShadowMapArray();

		uint32_t GetSize() const override { return m_size; }
		uint32_t GetLayerCount() const override { return m_layerCount; }

		float GetBias() const override { return m_bias; }
		void SetBias(float bias) override { m_bias = bias; }

		void BeginLayer(uint32_t layer) override;
		void EndLayer() override;

		void BindDepthTexture(uint32_t slot = 0) const override;
		uint32_t GetDepthTextureRendererID() const override { return m_depthTexture; }
	private:
		uint32_t m_framebuffer = 0;
		uint32_t m_depthTexture = 0;
		uint32_t m_size = 1024;
		uint32_t m_layerCount = 4;
		float m_bias = 0.0015f;
	};
}
