#include "Renderer.h"

#include <cstdint>

#include "RenderCommand.h"
#include "Shader.h"
#include "VertexArray.h"
#include "Engine/Core/Base.h"

namespace OiEngine {
	Scope<Renderer::SceneData> Renderer::s_sceneData = CreateScope<Renderer::SceneData>();

	void Renderer::Init()
	{
		RenderCommand::Init();
	}

	void Renderer::Shutdown()
	{
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}

	void Renderer::BeginScene(Camera& camera)
	{
	    s_sceneData->cameraPosition = camera.GetPosition();
	    s_sceneData->viewMatrix = camera.GetViewMatrix();
	    s_sceneData->projectionMatrix = camera.GetProjectionMatrix();
	}

	void Renderer::EndScene()
	{
	}

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
	    shader->Bind();
	    shader->SetFloat3("u_cameraPosition", s_sceneData->cameraPosition);
	    shader->SetMat4("u_Transform", transform);
	    shader->SetMat4("u_view", s_sceneData->viewMatrix);
	    shader->SetMat4("u_projection", s_sceneData->projectionMatrix);
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
}
