#pragma once

#include <vector>

#include "Engine/Renderer/Light.h"

namespace OiEngine {
	// 场景光照环境（纯数据）：环境光 + 各类光源列表。
	// 由 Renderer::BeginScene(camera, lighting) 打包上传；打包格式与顺序见 Renderer.cpp
	struct LightingEnvironment {
		glm::vec3 ambientColor{1.0f, 1.0f, 1.0f};
		float ambientIntensity = 0.0f;
		std::vector<DirectionalLight> directionalLights;
		std::vector<PointLight> pointLights;
		std::vector<SpotLight> spotLights;
	};
}
