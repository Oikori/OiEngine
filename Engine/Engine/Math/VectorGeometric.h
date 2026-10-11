#pragma once

#include <cmath>

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

namespace OiEngine::Math {
    template<typename T>
    constexpr T Dot(const TVector2<T>& a, const TVector2<T>& b)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Dot' accepts only floating-point inputs");
        return a.x * b.x + a.y * b.y;
    }
    template<typename T>
    constexpr T Dot(const TVector3<T>& a, const TVector3<T>& b)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Dot' accepts only floating-point inputs");
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    template<typename T>
    constexpr T Dot(const TVector4<T>& a, const TVector4<T>& b)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Dot' accepts only floating-point inputs");
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    template<typename T>
    constexpr T Distance(const TVector2<T>& a, const TVector2<T>& b)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Distance' accepts only floating-point inputs");
        TVector2<T> v = b - a;
        return sqrt(Dot(v, v));
    }
    template<typename T>
    constexpr T Distance(const TVector3<T>& a, const TVector3<T>& b)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Distance' accepts only floating-point inputs");
        TVector3<T> v = b - a;
        return sqrt(Dot(v, v));
    }
    template<typename T>
    constexpr T Distance(const TVector4<T>& a, const TVector4<T>& b)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Distance' accepts only floating-point inputs");
        TVector4<T> v = b - a;
        return sqrt(Dot(v, v));
    }

    template<typename T>
    constexpr TVector2<T> Normalize(const TVector2<T>& v)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Normalize' accepts only floating-point inputs");
        return v / sqrt(Dot(v, v));
    }
    template<typename T>
    constexpr TVector3<T> Normalize(const TVector3<T>& v)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Normalize' accepts only floating-point inputs");
        return v / sqrt(Dot(v, v));
    }
    template<typename T>
    constexpr TVector4<T> Normalize(const TVector4<T>& v)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Normalize' accepts only floating-point inputs");
        return v / sqrt(Dot(v, v));
    }

    template<typename T>
    constexpr TVector3<T> Cross(const TVector3<T>& a, const TVector3<T>& b)
    {
        static_assert(std::numeric_limits<T>::is_iec559, "'Cross' accepts only floating-point inputs");

        return TVector3<T>(
            a.y * b.z - b.y * a.z,
            a.z * b.x - b.z * a.x,
            a.x * b.y - b.x * a.y);
    }
}
