#pragma once

#include "RenderCommand.h"
#include "Shader.h"
#include "Camera.h"
#include "SceneEnvironment.h"
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

		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));
	private:
		// 相机 UBO（binding = 0）与光照 UBO（binding = 1），BeginScene 时上传
		static Ref<UniformBuffer> s_cameraBuffer;
		static Ref<UniformBuffer> s_lightingBuffer;
	};
}
