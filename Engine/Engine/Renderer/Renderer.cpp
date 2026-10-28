#include "Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "RenderCommand.h"
#include "Shader.h"
#include "VertexArray.h"
#include "Engine/Core/Base.h"

namespace OiEngine {
	namespace {
		constexpr uint32_t CameraUBOBinding = 0;  // shader 侧写作 layout(std140, binding = 0)
		// 光照 SSBO（shader 里写作 layout(std430, binding = 3) buffer Lighting）
		constexpr uint32_t LightingSSBOBinding = 3;
		// 阴影 UBO（shader 里写作 binding = 2），方向光阴影参数
		constexpr uint32_t ShadowUBOBinding = 2;

		// 打包后的单个光源（std430 布局，与 shader 中 Light 结构体逐字段对应）
		struct LightData {
			glm::vec4 positionType;   // xyz: 世界空间位置（Point/Spot）或传播方向（Directional）；w: LightType
			glm::vec4 directionRange; // xyz: 方向（Spot）；w: 影响半径（Point/Spot）
			glm::vec4 colorIntensity; // rgb: 颜色；w: 强度
			glm::vec4 params;         // x: 内锥角余弦；y: 外锥角余弦（Spot）
			glm::vec4 flags;          // x: 阴影矩阵槽位（-1 不投影）；y: 阴影深度偏移基数；z/w: 备用
		};

		// 打包后的光照数据（std430 布局，与 shader 中 uniform 块逐字段对应）：
		//
		//   layout(std430, binding = 3) buffer Lighting {
		//       vec4  u_Ambient;      // rgb: 颜色；a: 强度
		//       ivec4 u_Counts;       // x: 光源数；y: 阴影矩阵数；z: 每 cluster 光源上限；w: 备用
		//       vec4  u_ClipPlanes;   // x: near；y: far；z: 1/log(far/near)；w: 备用（cluster 深度切片用）
		//       ivec4 u_Grid;         // x/y: tilesX / tilesY；z: 深度切片数；w: 是否启用 cluster 剔除
		//       Light u_Lights[MAX_LIGHTS];
		//       mat4  u_ShadowMatrices[MAX_SHADOW_CASTERS];
		//   };
		struct LightingData {
			glm::vec4 ambient;
			glm::ivec4 counts;
			glm::vec4 clipPlanes;
			glm::ivec4 grid;
			LightData lights[MaxLights];
			glm::mat4 shadowMatrices[MaxShadowCasters];
		};

		static_assert(sizeof(LightData) == 80, "LightData must match the std430 layout in shader");
		static_assert(offsetof(LightingData, counts) == 16, "counts must start at offset 16");
		static_assert(offsetof(LightingData, clipPlanes) == 32, "clipPlanes must start at offset 32");
		static_assert(offsetof(LightingData, grid) == 48, "grid must start at offset 48");
		static_assert(offsetof(LightingData, lights) == 64, "lights must start at offset 64");
		static_assert(offsetof(LightingData, shadowMatrices) == 64 + MaxLights * 80, "shadowMatrices offset mismatch");
		static_assert(sizeof(LightingData) == 64 + MaxLights * 80 + MaxShadowCasters * 64, "LightingData size mismatch");

		// 打包后的阴影数据（std140 布局，与 shader 中 uniform 块逐字段对应）：
		//
		//   layout(std140, binding = 2) uniform Shadow {
		//       mat4 u_LightSpaceMatrix;  // 世界空间 -> 光空间裁剪空间
		//       vec4 u_ShadowParams;      // x: 1/贴图尺寸（texel 大小）；y: 深度偏移基数
		//   };
		struct ShadowData {
			glm::mat4 lightSpaceMatrix;
			glm::vec4 params;
		};

		static_assert(sizeof(ShadowData) == 80, "ShadowData must match the std140 layout in shader");

		// 打包光源：Directional -> Point -> Spot 顺序填充，总数超过 MaxLights 的部分被忽略
		void PackLightingData(const SceneEnvironment& scene, LightingData& data, uint32_t shadowMatrixCount)
		{
			data.ambient = glm::vec4(scene.ambientColor, scene.ambientIntensity);

			uint32_t count = 0;

			for (const auto& light : scene.directionalLights) {
				if (count >= MaxLights) {
					break;
				}
				LightData& packed = data.lights[count++];
				packed.positionType = glm::vec4(light.direction, static_cast<float>(LightType::Directional));
				packed.directionRange = glm::vec4(0.0f);
				packed.colorIntensity = glm::vec4(light.color, light.intensity);
				packed.params = glm::vec4(0.0f);
				packed.flags = glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
			}

			for (const auto& light : scene.pointLights) {
				if (count >= MaxLights) {
					break;
				}
				LightData& packed = data.lights[count++];
				packed.positionType = glm::vec4(light.position, static_cast<float>(LightType::Point));
				packed.directionRange = glm::vec4(0.0f, 0.0f, 0.0f, light.range);
				packed.colorIntensity = glm::vec4(light.color, light.intensity);
				packed.params = glm::vec4(0.0f);
				packed.flags = glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);  // 点光源不投射阴影
			}

			for (const auto& light : scene.spotLights) {
				if (count >= MaxLights) {
					break;
				}
				LightData& packed = data.lights[count++];
				packed.positionType = glm::vec4(light.position, static_cast<float>(LightType::Spot));
				packed.directionRange = glm::vec4(light.direction, light.range);
				packed.colorIntensity = glm::vec4(light.color, light.intensity);
				packed.params = glm::vec4(glm::cos(light.innerAngle), glm::cos(light.outerAngle), 0.0f, 0.0f);
				const float slot = light.shadowIndex >= 0 && static_cast<uint32_t>(light.shadowIndex) < shadowMatrixCount
					? static_cast<float>(light.shadowIndex) : -1.0f;
				packed.flags = glm::vec4(slot, light.shadowBias, 0.0f, 0.0f);
			}

			data.counts.x = static_cast<int>(count);
			data.counts.y = static_cast<int>(shadowMatrixCount);
		}
	}

	Ref<UniformBuffer> Renderer::s_cameraBuffer = nullptr;
	Ref<StorageBuffer> Renderer::s_lightingBuffer = nullptr;
	Ref<UniformBuffer> Renderer::s_shadowBuffer = nullptr;

	std::array<glm::mat4, MaxShadowCasters> Renderer::s_lightShadowMatrices{};
	uint32_t Renderer::s_shadowMatrixCount = 0;

	uint32_t Renderer::s_viewportWidth = 0;
	uint32_t Renderer::s_viewportHeight = 0;

	void Renderer::Init()
	{
		RenderCommand::Init();
		PostProcess::Init();
		LightClustering::Init();

		// 相机 UBO：BeginScene 时上传，shader 里以 binding = 0 读取
		s_cameraBuffer = UniformBuffer::Create(sizeof(CameraData), CameraUBOBinding);
		// 光照 SSBO：容量固定为 MaxLights 个光源 + MaxShadowCasters 个阴影矩阵，shader 里以 binding = 3 读取
		s_lightingBuffer = StorageBuffer::Create(sizeof(LightingData), LightingSSBOBinding);
		// 阴影 UBO：方向光阴影参数，BeginShadowPass 时上传，shader 里以 binding = 2 读取；
		// 先置零，未跑过阴影 pass 时采样不产生阴影
		s_shadowBuffer = UniformBuffer::Create(sizeof(ShadowData), ShadowUBOBinding);
		ShadowData zero;
		zero.lightSpaceMatrix = glm::mat4(0.0f);  // 注意 glm::mat4(0.0f) 是全零矩阵
		zero.params = glm::vec4(0.0f);
		s_shadowBuffer->SetData(&zero, sizeof(ShadowData));
	}

	void Renderer::Shutdown()
	{
		LightClustering::Shutdown();
		PostProcess::Shutdown();
		s_shadowBuffer.reset();
		s_lightingBuffer.reset();
		s_cameraBuffer.reset();
	}

	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		s_viewportWidth = width;
		s_viewportHeight = height;
		RenderCommand::SetViewport(0, 0, width, height);

		// HDR 目标与窗口同尺寸，一并重建
		PostProcess::Resize(width, height);
		// cluster 网格按窗口尺寸划分，同样需要重建
		LightClustering::Resize(width, height);
	}

	void Renderer::SetLightShadowMatrix(uint32_t index, const glm::mat4& lightSpaceMatrix)
	{
		OI_ASSERT(index < MaxShadowCasters, "Light shadow slot index out of range!");

		s_lightShadowMatrices[index] = lightSpaceMatrix;
		s_shadowMatrixCount = std::max(s_shadowMatrixCount, index + 1);
	}

	void Renderer::BeginScene(const Camera& camera, const SceneEnvironment& scene)
	{
		// HDR 路径：先绑定离屏目标并清屏（着色器输出线性 HDR，由 EndScene 统一 tonemap）
		PostProcess::BeginScene();

	    CameraData cameraData;
	    cameraData.projection = camera.GetProjectionMatrix();
	    cameraData.view = camera.GetViewMatrix();
	    cameraData.cameraPosition = glm::vec4(camera.GetPosition(), 1.0f);
	    s_cameraBuffer->SetData(&cameraData, sizeof(CameraData));

		LightingData lightingData{};
		const float nearClip = std::max(camera.GetNearClip(), 0.0001f);
		const float farClip = std::max(camera.GetFarClip(), nearClip * 1.001f);
		lightingData.clipPlanes = glm::vec4(nearClip, farClip, 1.0f / std::log(farClip / nearClip), 0.0f);
		lightingData.grid = LightClustering::GetGridInfo();  // 未启用剔除时为 (0,0,0,0)
		lightingData.counts = glm::ivec4(0, 0, static_cast<int>(LightClustering::MaxLightsPerCluster), 0);
		PackLightingData(scene, lightingData, s_shadowMatrixCount);
		for (uint32_t i = 0; i < s_shadowMatrixCount; ++i) {
			lightingData.shadowMatrices[i] = s_lightShadowMatrices[i];
		}
		s_lightingBuffer->SetData(&lightingData, sizeof(LightingData));

		// 光源数据就绪后派发 cluster 剔除（compute 写入 cluster 光源列表，供片元着色器读取）
		LightClustering::Dispatch();
	}

	void Renderer::EndScene()
	{
		PostProcess::EndScene();
	}

	void Renderer::UploadLightCamera(const glm::mat4& lightView, const glm::mat4& lightProjection)
	{
		// 复用相机 UBO（binding = 0）：阴影 pass 里几何着色器（深度着色器）读到的是光源相机的视图 / 投影。
		// 主场景的 BeginScene 会重新上传真正的相机数据，调用顺序保证互不干扰。
		CameraData cameraData;
		cameraData.projection = lightProjection;
		cameraData.view = lightView;
		cameraData.cameraPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // 光源相机无位置语义，占位
		s_cameraBuffer->SetData(&cameraData, sizeof(CameraData));
	}

	void Renderer::BeginShadowPass(const Ref<ShadowMap>& shadowMap, const glm::mat4& lightView, const glm::mat4& lightProjection)
	{
		OI_ASSERT(shadowMap, "Shadow map is null!");

		UploadLightCamera(lightView, lightProjection);

		ShadowData shadowData;
		shadowData.lightSpaceMatrix = lightProjection * lightView;
		shadowData.params = glm::vec4(1.0f / static_cast<float>(shadowMap->GetSize()), shadowMap->GetBias(), 0.0f, 0.0f);
		s_shadowBuffer->SetData(&shadowData, sizeof(ShadowData));

		shadowMap->Begin();
	}

	void Renderer::EndShadowPass(const Ref<ShadowMap>& shadowMap)
	{
		OI_ASSERT(shadowMap, "Shadow map is null!");

		shadowMap->End();

		// 阴影贴图尺寸与窗口不同，恢复窗口视口（窗口尺寸由 OnWindowResize 记录）
		if (s_viewportWidth > 0 && s_viewportHeight > 0) {
			RenderCommand::SetViewport(0, 0, s_viewportWidth, s_viewportHeight);
		}
	}

	void Renderer::BeginShadowPass(const Ref<ShadowMapArray>& shadowMapArray, uint32_t layer, const glm::mat4& lightView, const glm::mat4& lightProjection)
	{
		OI_ASSERT(shadowMapArray, "Shadow map array is null!");

		// 聚光灯的光空间矩阵由场景通过 SetLightShadowMatrix 登记（index = SpotLight::shadowIndex），
		// 这里只负责把光源相机上传到相机 UBO 并开始渲染该层
		UploadLightCamera(lightView, lightProjection);
		shadowMapArray->BeginLayer(layer);
	}

	void Renderer::EndShadowPass(const Ref<ShadowMapArray>& shadowMapArray)
	{
		OI_ASSERT(shadowMapArray, "Shadow map array is null!");

		shadowMapArray->EndLayer();

		if (s_viewportWidth > 0 && s_viewportHeight > 0) {
			RenderCommand::SetViewport(0, 0, s_viewportWidth, s_viewportHeight);
		}
	}

	void Renderer::Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform)
	{
		shader->Bind();
		shader->SetMat4("u_Transform", transform);
		vertexArray->Bind();
		RenderCommand::DrawIndexed(vertexArray);
	}
}
