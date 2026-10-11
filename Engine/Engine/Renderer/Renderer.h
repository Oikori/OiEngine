#pragma once

#include "RenderCommand.h"
#include "Shader.h"
#include "Camera.h"

namespace OiEngine {
	class OI_API Renderer {
	public:
		static void Init();
		static void Shutdown();
		
		static void OnWindowResize(uint32_t width, uint32_t height);

		static void BeginScene(Camera& camera);
		static void EndScene();

		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const Math::Matrix4& transform = Math::Matrix4(1.0f));
	private:
	    struct SceneData {
	        Math::Vector3 cameraPosition;
	        Math::Matrix4 viewMatrix;
	        Math::Matrix4 projectionMatrix;
		};

		static Scope<SceneData> s_sceneData;
	};
}
