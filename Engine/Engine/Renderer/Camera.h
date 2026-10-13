#pragma once

#include "Engine/Math.h"

namespace OiEngine {
	// 相机 UBO 数据（std140 布局，与 shader 中 uniform 块逐字段对应）：
	//
	//   layout(std140, binding = 0) uniform Camera {
	//       mat4 u_Projection;
	//       mat4 u_View;
	//       vec4 u_CameraPosition;  // xyz: 世界空间位置；w: 对齐占位
	//   };
	struct CameraData {
		glm::mat4 projection;
		glm::mat4 view;
		glm::vec4 cameraPosition;
	};

	class OI_API Camera {
	public:
		virtual ~Camera() = default;

		// 相机效果（如震屏）应体现在这里返回的视图矩阵中，随 UBO 一起上传
	    virtual glm::vec3 GetPosition() const = 0;
	    virtual glm::mat4 GetViewMatrix() const = 0;
		virtual glm::mat4 GetProjectionMatrix() const = 0;
	};
}
