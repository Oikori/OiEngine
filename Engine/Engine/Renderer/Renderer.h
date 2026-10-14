#pragma once

#include "RenderCommand.h"
#include "Shader.h"
#include "Camera.h"
#include "SceneEnvironment.h"
#include "ShadowMap.h"
#include "UniformBuffer.h"

namespace OiEngine {
	class OI_API Renderer {
	public:
		static void Init();
		static void Shutdown();
		
		static void OnWindowResize(uint32_t width, uint32_t height);

		// 带光照的场景：额外把 SceneEnvironment 打包上传到光照 UBO（binding = 1）
		static void BeginScene(const Camera& camera, const SceneEnvironment& scene = {});
		static void EndScene();

		// 阴影 pass：把光源相机的视图 / 正交投影打包进相机 UBO（binding = 0，与深度着色器共用），
		// 同时把光空间矩阵与阴影参数上传到阴影 UBO（binding = 2），并绑定阴影贴图帧缓冲、清除深度。
		// 应在主场景 BeginScene 之前调用，期间用只写深度的着色器提交所有投影物体。
		static void BeginShadowPass(const Ref<ShadowMap>& shadowMap, const glm::mat4& lightView, const glm::mat4& lightProjection);
		// 结束阴影 pass：解绑帧缓冲并把视口恢复为窗口尺寸
		static void EndShadowPass(const Ref<ShadowMap>& shadowMap);

		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));
	private:
		// 相机 UBO（binding = 0）、光照 UBO（binding = 1）与阴影 UBO（binding = 2），BeginScene / BeginShadowPass 时上传
		static Ref<UniformBuffer> s_cameraBuffer;
		static Ref<UniformBuffer> s_lightingBuffer;
		static Ref<UniformBuffer> s_shadowBuffer;

		// 窗口尺寸记录，EndShadowPass 恢复视口用
		static uint32_t s_viewportWidth;
		static uint32_t s_viewportHeight;
	};
}
