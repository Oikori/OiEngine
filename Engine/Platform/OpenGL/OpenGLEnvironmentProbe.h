#pragma once

#include "Engine/Renderer/EnvironmentProbe.h"
#include "Engine/Renderer/Shader.h"
#include "Engine/Renderer/VertexArray.h"

namespace OiEngine {
	class OpenGLEnvironmentProbe : public EnvironmentProbe {
	public:
		OpenGLEnvironmentProbe(uint32_t captureSize, uint32_t irradianceSize, uint32_t prefilterSize);
		virtual ~OpenGLEnvironmentProbe();

		void Capture(const glm::vec3& position, const CaptureCallback& drawScene,
			float nearClip = 0.1f, float farClip = 200.0f) override;

		void BindIrradiance(uint32_t slot) const override;
		void BindPrefiltered(uint32_t slot) const override;
		uint32_t GetIrradianceRendererID() const override { return m_irradianceTexture; }
		uint32_t GetPrefilteredRendererID() const override { return m_prefilterTexture; }
		uint32_t GetPrefilterLevelCount() const override { return m_prefilterLevels; }
	private:
		// 保证资源只初始化一次（构造函数不直接开 GL，便于将来延迟创建）
		void InitResources();

		// 把 cubemap 的 6 个面依次渲染（shader 需要已绑定；u_FaceProjection / u_FaceView 由本函数设置）
		void RenderFaces(const Ref<Shader>& shader, uint32_t texture, uint32_t size, uint32_t mipLevel, const glm::vec3& position);
	private:
		uint32_t m_captureSize = 256;
		uint32_t m_irradianceSize = 32;
		uint32_t m_prefilterSize = 128;
		uint32_t m_prefilterLevels = 1;

		uint32_t m_captureFramebuffer = 0;
		uint32_t m_captureTexture = 0;
		uint32_t m_captureDepth = 0;

		uint32_t m_irradianceFramebuffer = 0;
		uint32_t m_irradianceTexture = 0;

		uint32_t m_prefilterFramebuffer = 0;
		uint32_t m_prefilterTexture = 0;

		Ref<Shader> m_irradianceShader;
		Ref<Shader> m_prefilterShader;
		Ref<VertexArray> m_cubeMesh;
	};
}
