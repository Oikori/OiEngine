#pragma once

#include <functional>

#include "Engine/Core/Base.h"
#include "Engine/Math.h"

namespace OiEngine {
	// 环境探针（IBL）：把场景从某个位置捕获进 cubemap，再预计算出
	//   - irradiance cubemap（余弦卷积，供漫反射环境光）
	//   - prefilter cubemap（GGX 重要性采样 + mip 链，供粗糙度分级的环境镜面反射）
	//
	// 用法（场景侧）：
	//   m_probe = EnvironmentProbe::Create();
	//   m_probe->Capture(position, [this](const glm::mat4& view, const glm::mat4& projection) {
	//       // 用探针给的视图 / 投影把场景画进当前面（材质用简化着色即可，不能依赖探针自身）
	//   });
	// 光照着色器侧：环境漫反射采样 irradiance，环境镜面采样 prefilter（LOD = roughness * 最大 mip）。
	class OI_API EnvironmentProbe {
	public:
		// 捕获回调：画进当前 cubemap 面（视图 / 投影矩阵由探针提供，fov 90°、宽高比 1:1）
		using CaptureCallback = std::function<void(const glm::mat4& faceView, const glm::mat4& faceProjection)>;

		virtual ~EnvironmentProbe() = default;

		// 重新捕获并预计算（捕获 cubemap 6 个面 + irradiance 卷积 + GGX prefilter）
		virtual void Capture(const glm::vec3& position, const CaptureCallback& drawScene,
			float nearClip = 0.1f, float farClip = 200.0f) = 0;

		virtual void BindIrradiance(uint32_t slot) const = 0;
		virtual void BindPrefiltered(uint32_t slot) const = 0;
		virtual uint32_t GetIrradianceRendererID() const = 0;
		virtual uint32_t GetPrefilteredRendererID() const = 0;
		// prefilter 的 mip 层数（着色器用它换算 LOD：roughness * (层数 - 1)）
		virtual uint32_t GetPrefilterLevelCount() const = 0;

		static Ref<EnvironmentProbe> Create(uint32_t captureSize = 256, uint32_t irradianceSize = 32, uint32_t prefilterSize = 128);
	};
}
