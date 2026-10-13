#pragma once

#include <cstdint>

#include "Engine/Math.h"

namespace OiEngine {
	// 光源类型（数值会打包进 UBO，与 shader 中的判断保持一致）
	enum class LightType : uint32_t {
		Directional = 0,  // 方向光：模拟太阳等极远处光源，无位置、无衰减
		Point = 1,        // 点光源：从一点向四周发光，range 内平滑衰减
		Spot = 2,         // 聚光灯：锥形光束，range 内衰减、内锥到外锥平滑过渡
	};

	// 方向光：direction 为光的传播方向（从光源射向场景，如顶光为 (0, -1, 0)）
	struct DirectionalLight {
		glm::vec3 direction{0.0f, -1.0f, 0.0f};
		glm::vec3 color{1.0f, 1.0f, 1.0f};
		float intensity = 1.0f;
	};

	// 点光源：position 为世界空间位置，range 为强度衰减到 0 的距离
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
	};
}
