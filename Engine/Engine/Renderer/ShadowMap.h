#pragma once

// glm 的 lookAt / ortho / perspective 需要显式包含（不在 glm/glm.hpp 中）
#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Core/Base.h"
#include "Engine/Math.h"
#include "Framebuffer.h"
#include "Light.h"

namespace OiEngine {
	// 方向光阴影贴图：只含一个深度附件的离屏帧缓冲，从光源视角渲染场景深度，
	// 光照着色时把世界坐标投影到光空间并与深度比较，即可得到阴影。
	// 使用方法见 AGENTS.md「阴影」一节；渲染流程由 Renderer::BeginShadowPass / EndShadowPass 驱动。
	class OI_API ShadowMap {
	public:
		// size 为正方形边长（像素），默认 2048
		explicit ShadowMap(uint32_t size = 2048);

		// 调整分辨率（重建深度纹理）
		void Resize(uint32_t size);
		uint32_t GetSize() const { return m_size; }

		// 深度偏移基数（斜率缩放）：越大越不容易出现自阴影条纹，但阴影会"悬浮"（peter-panning）
		float GetBias() const { return m_bias; }
		void SetBias(float bias) { m_bias = bias; }

		// 阴影 pass 的帧缓冲绑定 / 解除（Begin 会设置视口为贴图尺寸并清除深度）
		void Begin();
		void End();

		// 供光照着色器采样：把深度纹理绑定到指定纹理单元
		void BindDepthTexture(uint32_t slot = 0) const;
		uint32_t GetDepthTextureRendererID() const;
	private:
		Ref<Framebuffer> m_framebuffer;
		uint32_t m_size = 2048;
		float m_bias = 0.0025f;
	};

	// 方向光的阴影相机（正交投影）
	struct DirectionalLightShadow {
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 lightSpaceMatrix;  // projection * view
	};

	// 计算方向光的阴影相机：正交视锥覆盖以 center 为中心、半径 radius 的包围球。
	// 传入光源方向（传播方向）与场景包围球即可，无需手动摆放光源位置。
	inline DirectionalLightShadow ComputeDirectionalLightShadow(const DirectionalLight& light, const glm::vec3& center, float radius)
	{
		OI_ASSERT(radius > 0.0f, "Shadow radius must be positive!");

		const glm::vec3 direction = glm::normalize(light.direction);
		const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

		DirectionalLightShadow shadow;
		// 光源沿传播方向的反方向退到包围球外 2 倍半径处，near/far 足以覆盖整个球体
		shadow.view = glm::lookAt(center - direction * (radius * 2.0f), center, up);
		shadow.projection = glm::ortho(-radius, radius, -radius, radius, 0.01f, radius * 4.0f);
		shadow.lightSpaceMatrix = shadow.projection * shadow.view;
		return shadow;
	}

	// 聚光灯的阴影相机（透视投影，视锥与光锥一致：fov = 2 * outerAngle，宽高比 1:1）
	struct SpotLightShadow {
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 lightSpaceMatrix;  // projection * view
	};

	// 计算聚光灯的阴影相机：near/far 覆盖光源到 range 的距离。
	// 采样时世界坐标乘 lightSpaceMatrix、除以 w 再做 * 0.5 + 0.5 即得 [0,1] 的贴图坐标，
	// 投影已把整个光锥映射到 NDC 的 [-1,1]^2，不需要额外缩放。
	inline SpotLightShadow ComputeSpotLightShadow(const SpotLight& light, float nearPlane = 0.15f)
	{
		OI_ASSERT(light.outerAngle > 0.0f, "Spot light outer angle must be positive!");

		const glm::vec3 direction = glm::normalize(light.direction);
		const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

		SpotLightShadow shadow;
		shadow.view = glm::lookAt(light.position, light.position + direction, up);
		shadow.projection = glm::perspective(2.0f * light.outerAngle, 1.0f, nearPlane,
			std::max(light.range, nearPlane * 2.0f));
		shadow.lightSpaceMatrix = shadow.projection * shadow.view;
		return shadow;
	}
}
