#include "Renderer.h"

#include <cstddef>
#include <cstdint>

#include "RenderCommand.h"
#include "Shader.h"
#include "VertexArray.h"
#include "Engine/Core/Base.h"

namespace OiEngine {
	namespace {
		constexpr uint32_t CameraUBOBinding = 0;  // shader 侧写作 layout(std140, binding = 0)
		// shader 侧约定：光照 UBO 的 binding 与最大光源数（shader 里写作 binding = 1 / MAX_LIGHTS），需与 shader 保持一致
		constexpr uint32_t LightingUBOBinding = 1;
		constexpr uint32_t MaxLights = 32;

		// 打包后的单个光源（std140 布局，与 shader 中 Light 结构体逐字段对应）
		struct LightData {
			glm::vec4 positionType;   // xyz: 世界空间位置（Point/Spot）或传播方向（Directional）；w: LightType
			glm::vec4 directionRange; // xyz: 方向（Spot）；w: 影响范围（Point/Spot）
			glm::vec4 colorIntensity; // rgb: 颜色；w: 强度
			glm::vec4 params;         // x: 内锥角余弦；y: 外锥角余弦（Spot）
		};

		// 打包后的光照数据（std140 布局，与 shader 中 uniform 块逐字段对应）：
		//
		//   layout(std140, binding = 1) uniform Lighting {
		//       vec4  u_Ambient;     // rgb: 颜色；a: 强度
		//       int   u_LightCount;  // 有效光源数
		//       Light u_Lights[MAX_LIGHTS];
		//   };
		//
		// std140 规则下结构体数组按 16 字节对齐，lightCount 之后有 12 字节 padding，
		// C++ 侧用 ivec4 显式补齐（只有 x 有效）。
		struct LightingData {
			glm::vec4 ambient;
			glm::ivec4 lightCount;
			LightData lights[MaxLights];
		};

		static_assert(sizeof(LightData) == 64, "LightData must match the std140 layout in shader");
		static_assert(offsetof(LightingData, lightCount) == 16, "lightCount must start at offset 16");
		static_assert(offsetof(LightingData, lights) == 32, "lights must start at offset 32");
		static_assert(sizeof(LightingData) == 32 + MaxLights * 64, "LightingData size mismatch");

		// 按 Directional -> Point -> Spot 顺序填充，总数超过 MaxLights 的部分被忽略
		void PackLightingData(const SceneEnvironment& lighting, LightingData& data)
		{
			data.ambient = glm::vec4(lighting.ambientColor, lighting.ambientIntensity);

			uint32_t count = 0;

			for (const auto& light : lighting.directionalLights) {
				if (count >= MaxLights) {
					break;
				}
				LightData& packed = data.lights[count++];
				packed.positionType = glm::vec4(light.direction, static_cast<float>(LightType::Directional));
				packed.directionRange = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
				packed.colorIntensity = glm::vec4(light.color, light.intensity);
				packed.params = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			}

			for (const auto& light : lighting.pointLights) {
				if (count >= MaxLights) {
					break;
				}
				LightData& packed = data.lights[count++];
				packed.positionType = glm::vec4(light.position, static_cast<float>(LightType::Point));
				packed.directionRange = glm::vec4(0.0f, 0.0f, 0.0f, light.range);
				packed.colorIntensity = glm::vec4(light.color, light.intensity);
				packed.params = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			}

			for (const auto& light : lighting.spotLights) {
				if (count >= MaxLights) {
					break;
				}
				LightData& packed = data.lights[count++];
				packed.positionType = glm::vec4(light.position, static_cast<float>(LightType::Spot));
				packed.directionRange = glm::vec4(light.direction, light.range);
				packed.colorIntensity = glm::vec4(light.color, light.intensity);
				packed.params = glm::vec4(glm::cos(light.innerAngle), glm::cos(light.outerAngle), 0.0f, 0.0f);
			}

			data.lightCount = glm::ivec4(static_cast<int>(count), 0, 0, 0);
		}
	}

	Ref<UniformBuffer> Renderer::s_cameraBuffer = nullptr;
	Ref<UniformBuffer> Renderer::s_lightingBuffer = nullptr;

	void Renderer::Init()
	{
		RenderCommand::Init();

		// 相机 UBO：BeginScene 时上传，shader 里以 binding = 0 读取
		s_cameraBuffer = UniformBuffer::Create(sizeof(CameraData), CameraUBOBinding);
		// 光照 UBO：容量固定为 MaxLights 个光源，shader 里以 binding = 1 读取
		s_lightingBuffer = UniformBuffer::Create(sizeof(LightingData), LightingUBOBinding);
	}

	void Renderer::Shutdown()
	{
		s_cameraBuffer.reset();
		s_lightingBuffer.reset();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		RenderCommand::SetViewport(0, 0, width, height);
	}

	void Renderer::BeginScene(const Camera& camera, const SceneEnvironment& scene)
	{
	    CameraData cameraData;
	    cameraData.projection = camera.GetProjectionMatrix();
	    cameraData.view = camera.GetViewMatrix();
	    cameraData.cameraPosition = glm::vec4(camera.GetPosition(), 1.0f);
	    s_cameraBuffer->SetData(&cameraData, sizeof(CameraData));

		LightingData lightingData{};
		PackLightingData(scene, lightingData);
		s_lightingBuffer->SetData(&lightingData, sizeof(LightingData));
	}

	void Renderer::EndScene()
	{
	}

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();
		shader->SetMat4("u_Transform", transform);
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
}
