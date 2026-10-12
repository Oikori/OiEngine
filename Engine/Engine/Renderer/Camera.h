#pragma once

#include "Engine/Math.h"

namespace OiEngine {
	class OI_API Camera {
	public:
		virtual ~Camera() = default;

	    virtual glm::vec3 GetPosition() const = 0;
	    virtual glm::mat4 GetViewMatrix() const = 0;
		virtual glm::mat4 GetProjectionMatrix() const = 0;
	};
}
