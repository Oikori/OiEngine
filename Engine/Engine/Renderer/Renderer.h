#pragma once

#include <array>
#include <cstdint>

#include "RenderCommand.h"
#include "PostProcess.h"
#include "LightClustering.h"
#include "Shader.h"
#include "Camera.h"
#include "SceneEnvironment.h"
#include "ShadowMap.h"
#include "ShadowMapArray.h"
#include "StorageBuffer.h"
#include "UniformBuffer.h"

namespace OiEngine {
	class OI_ENGINE_API Renderer {
	public:
		static void Init();
		static void Shutdown();
		
		static void OnWindowResize(uint32_t width, uint32_t height);

		// 带光照的场景：把 SceneEnvironment 打包上传到光照 SSBO（binding = 3，std430，上限 MaxLights 个光源）
		static void BeginScene(const Camera& camera, const SceneEnvironment& scene = {});
		static void EndScene();

		// 方向光阴影 pass：把光源相机的视图 / 正交投影打包进相机 UBO（binding = 0，与深度着色器共用），
		// 同时把光空间矩阵与阴影参数上传到阴影 UBO（binding = 2），并绑定阴影贴图帧缓冲、清除深度。
		// 应在主场景 BeginScene 之前调用，期间用只写深度的着色器提交所有投影物体。
		static void BeginShadowPass(const Ref<ShadowMap>& shadowMap, const glm::mat4& lightView, const glm::mat4& lightProjection);
		// 结束阴影 pass：解绑帧缓冲并把视口恢复为窗口尺寸
		static void EndShadowPass(const Ref<ShadowMap>& shadowMap);

		// 聚光灯阴影 pass：渲染到阴影贴图数组的指定层；光空间矩阵需另行登记（见 SetLightShadowMatrix）
		static void BeginShadowPass(const Ref<ShadowMapArray>& shadowMapArray, uint32_t layer, const glm::mat4& lightView, const glm::mat4& lightProjection);
		static void EndShadowPass(const Ref<ShadowMapArray>& shadowMapArray);

		// 登记聚光灯 / 点光源以外的光源阴影矩阵：index 与 SpotLight::shadowIndex 一致（0 ~ MaxShadowCasters-1），
		// 下一次 BeginScene 时随光照 SSBO 一起上传；矩阵由 ComputeSpotLightShadow 计算
		static void SetLightShadowMatrix(uint32_t index, const glm::mat4& lightSpaceMatrix);
		static uint32_t GetShadowCasterCount() { return s_shadowMatrixCount; }

		// HDR 渲染：开启后场景渲染到 RGBA16F 离屏目标，EndScene 时统一做 tonemap + gamma 输出；
		// 场景着色器应输出线性 HDR 颜色（不要自己 tonemap / gamma）。默认关闭（行为与逐物体 tonemap 的旧路径一致）
		static void SetHDREnabled(bool enabled) { PostProcess::SetEnabled(enabled); }
		static bool IsHDREnabled() { return PostProcess::IsEnabled(); }
		// 全局曝光（tonemap 之前相乘）
		static void SetExposure(float exposure) { PostProcess::SetExposure(exposure); }
		static float GetExposure() { return PostProcess::GetExposure(); }

		// Forward+ 光源剔除（cluster）：启用后 BeginScene 在上传光源数据后自动派发 compute 剔除，
		// 光照着色器按 cluster 读取光源列表（shader 侧需声明 binding = 4 / 5 的 buffer）。
		// 关闭时着色器退回遍历全部光源（对照排查用）
		static void SetLightCullingEnabled(bool enabled) { LightClustering::SetEnabled(enabled); }
		static bool IsLightCullingEnabled() { return LightClustering::IsEnabled(); }

		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));
	private:
		// 把光源相机的视图 / 投影写进相机 UBO（shadow pass 期间几何着色器读到的就是光源相机）
		static void UploadLightCamera(const glm::mat4& lightView, const glm::mat4& lightProjection);

		// 相机 UBO（binding = 0）、光照 SSBO（binding = 3）与阴影 UBO（binding = 2），BeginScene / BeginShadowPass 时上传
		static Ref<UniformBuffer> s_cameraBuffer;
		static Ref<StorageBuffer> s_lightingBuffer;
		static Ref<UniformBuffer> s_shadowBuffer;

		// 阴影矩阵槽位（SetLightShadowMatrix 登记，BeginScene 时随光照 SSBO 上传）
		static std::array<glm::mat4, MaxShadowCasters> s_lightShadowMatrices;
		static uint32_t s_shadowMatrixCount;

		// 窗口尺寸记录，EndShadowPass 恢复视口用
		static uint32_t s_viewportWidth;
		static uint32_t s_viewportHeight;
	};
}
