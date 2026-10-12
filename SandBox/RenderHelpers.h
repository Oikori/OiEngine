#pragma once

#include <cmath>

#include "OiEngine.h"

// 引擎数学库目前只提供向量/矩阵基础运算（见 Engine/Engine/Math/）。
// 这里补齐 SandBox 的 2D 场景需要的矩阵辅助函数与碰撞判断，均采用与引擎一致的列主序布局。
namespace OiEngine::SandBox {

    inline Math::Matrix4 Translate(const Math::Vector3& t)
    {
        return Math::Matrix4(
            Math::Vector4(1.0f, 0.0f, 0.0f, 0.0f),
            Math::Vector4(0.0f, 1.0f, 0.0f, 0.0f),
            Math::Vector4(0.0f, 0.0f, 1.0f, 0.0f),
            Math::Vector4(t.x, t.y, t.z, 1.0f));
    }

    inline Math::Matrix4 Scale(const Math::Vector3& s)
    {
        return Math::Matrix4(
            Math::Vector4(s.x, 0.0f, 0.0f, 0.0f),
            Math::Vector4(0.0f, s.y, 0.0f, 0.0f),
            Math::Vector4(0.0f, 0.0f, s.z, 0.0f),
            Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // 绕 Z 轴旋转（X-Y 游戏平面内的朝向）
    inline Math::Matrix4 RotateZ(float radians)
    {
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        return Math::Matrix4(
            Math::Vector4(c, s, 0.0f, 0.0f),
            Math::Vector4(-s, c, 0.0f, 0.0f),
            Math::Vector4(0.0f, 0.0f, 1.0f, 0.0f),
            Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // 正交投影：映射 (left, right) x (bottom, top) x (near, far) 到 NDC
    inline Math::Matrix4 Orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        return Math::Matrix4(
            Math::Vector4(2.0f / (right - left), 0.0f, 0.0f, 0.0f),
            Math::Vector4(0.0f, 2.0f / (top - bottom), 0.0f, 0.0f),
            Math::Vector4(0.0f, 0.0f, -2.0f / (farPlane - nearPlane), 0.0f),
            Math::Vector4(-(right + left) / (right - left),
                          -(top + bottom) / (top - bottom),
                          -(farPlane + nearPlane) / (farPlane - nearPlane),
                          1.0f));
    }

    // 世界坐标朝向 -> 旋转弧度（默认朝向为 +Y 轴）
    inline float RotationTo(const Math::Vector2& direction)
    {
        return std::atan2(direction.x, direction.y);
    }

    inline bool AabbOverlap(const Math::Vector2& aCenter, float aHalfSize,
                            const Math::Vector2& bCenter, float bHalfSize)
    {
        return std::abs(aCenter.x - bCenter.x) < (aHalfSize + bHalfSize) &&
               std::abs(aCenter.y - bCenter.y) < (aHalfSize + bHalfSize);
    }

    // 圆碰撞判定（半径不同）
    inline bool CircleHit(const Math::Vector2& aCenter, float aRadius,
                              const Math::Vector2& bCenter, float bRadius)
    {
        const float dx = aCenter.x - bCenter.x;
        const float dy = aCenter.y - bCenter.y;
        const float radiusSum = aRadius + bRadius;
        return (dx * dx + dy * dy) < radiusSum * radiusSum;
    }
}
