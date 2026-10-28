#pragma once

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

#include "OiEngine.h"

// SandBox 场景共用的矩阵辅助与碰撞判断；矩阵使用 glm（列主序，与 OpenGL 一致）。
namespace OiEngine::SandBox {

    inline glm::mat4 Translate(const glm::vec3& t)
    {
        return glm::translate(glm::mat4(1.0f), t);
    }

    inline glm::mat4 Scale(const glm::vec3& s)
    {
        return glm::scale(glm::mat4(1.0f), s);
    }

    // 绕 Z 轴旋转（X-Y 游戏平面内的朝向）
    inline glm::mat4 RotateZ(float radians)
    {
        return glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    // 正交投影：映射 (left, right) x (bottom, top) x (near, far) 到 NDC
    inline glm::mat4 Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        return glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    }

    // 世界坐标朝向 -> 旋转弧度（默认朝向为 +Y 轴）
    inline float RotationTo(const glm::vec2& direction)
    {
        return std::atan2(direction.x, direction.y);
    }

    inline bool AabbOverlap(const glm::vec2& aCenter, float aHalfSize,
                            const glm::vec2& bCenter, float bHalfSize)
    {
        return std::abs(aCenter.x - bCenter.x) < (aHalfSize + bHalfSize) &&
               std::abs(aCenter.y - bCenter.y) < (aHalfSize + bHalfSize);
    }

    // 圆碰撞判定（半径不同）
    inline bool CircleHit(const glm::vec2& aCenter, float aRadius,
                              const glm::vec2& bCenter, float bRadius)
    {
        const float dx = aCenter.x - bCenter.x;
        const float dy = aCenter.y - bCenter.y;
        const float radiusSum = aRadius + bRadius;
        return (dx * dx + dy * dy) < radiusSum * radiusSum;
    }
}
