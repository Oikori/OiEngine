#pragma once

#include <cstdint>

#include "Engine/Math.h"

namespace OiEngine {
	// 单帧光源上限（光照 SSBO 的固定容量；shader 侧 MAX_LIGHTS 必须一致）
	constexpr uint32_t MaxLights = 256;
	// 阴影矩阵槽位上限（聚光灯阴影；shader 侧 MAX_SHADOW_CASTERS 必须一致）。
	// 方向光阴影走独立的 2D 阴影贴图路径，不占槽位。
	constexpr uint32_t MaxShadowCasters = 8;

	// 光源类型（数值会打包进 UBO/SSBO，与 shader 中的判断保持一致）
	enum class LightType : uint32_t {
		Directional = 0,  // 方向光：模拟太阳等极远处光源，无位置、无衰减
		Point = 1,        // 点光源：从一点向四周发光，range 内平滑衰减
		Spot = 2,         // 聚光灯：锥形光束，range 内衰减、内锥到外锥平滑过渡
	};

	// 衰减约定（shader 侧统一实现，见各场景光照着色器）：
	//
	//   w = saturate(1 - (d / range)^4)
	//   attenuation = w^2 / max(d^2, 0.01)      // 窗函数截断的平方反比衰减
	//
	// 即 intensity 表示"距离 1 个单位处的照度"，室内点光源典型取值 20 ~ 200。
	// range 是影响半径（窗函数在此处把强度压到 0），与亮度无关。

	// 方向光：direction 为光的传播方向（从光源射向场景，如顶光为 (0, -1, 0)）
	struct DirectionalLight {
		glm::vec3 direction{0.0f, -1.0f, 0.0f};
		glm::vec3 color{1.0f, 1.0f, 1.0f};
		float intensity = 1.0f;
	};

	// 点光源：position 为世界空间位置，range 为影响半径（衰减窗函数在此处归零）
	struct PointLight {
		glm::vec3 position{0.0f, 0.0f, 0.0f};
		float range = 10.0f;
		glm::vec3 color{1.0f, 1.0f, 1.0f};
		float intensity = 1.0f;
	};

	// 聚光灯：innerAngle 内全亮，innerAngle 到 outerAngle 之间线性过渡（均为半角、弧度）
	struct SpotLight {
		glm::vec3 position{0.0f, 0.0f, 0.0f};
		float range = 10.0f;
		glm::vec3 direction{0.0f, -1.0f, 0.0f};
		float innerAngle = glm::radians(12.5f);
		glm::vec3 color{1.0f, 1.0f, 1.0f};
		float outerAngle = glm::radians(20.0f);
		float intensity = 1.0f;
		// 阴影：-1 表示不投射阴影；>= 0 时对应 Renderer::SetLightShadowMatrix(index, ...) 登记的
		// 光空间矩阵，以及场景绑定到约定纹理单元的阴影贴图数组中的同号层（见 ShadowMapArray / IndoorDemo）
		int shadowIndex = -1;
		float shadowBias = 0.0015f;  // 深度偏移基数（斜率缩放）
	};
}
