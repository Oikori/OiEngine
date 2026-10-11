#pragma once

#include "Engine/Math.h"

namespace OiEngine {
	class OI_API Camera {
	public:
		virtual ~Camera() = default;

	    virtual Math::Vector3 GetPosition() const = 0;
	    virtual Math::Matrix4 GetViewMatrix() const = 0;
		virtual Math::Matrix4 GetProjectionMatrix() const = 0;
	};
}
