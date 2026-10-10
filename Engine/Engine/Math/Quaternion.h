#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    template<typename T>
    struct OI_API TQuaternion {
        constexpr explicit TQuaternion(T w = 1, T x = 0, T y = 0, T z = 0);
        constexpr explicit TQuaternion(T (&arr)[4]);
        constexpr TQuaternion(const TQuaternion& q);
        constexpr TQuaternion(TQuaternion&& q) noexcept;

        constexpr TQuaternion& operator=(const TQuaternion& q);
        constexpr TQuaternion& operator=(TQuaternion&& q) noexcept;

        static constexpr size_t Length();
        constexpr T& operator[](size_t i);
        constexpr const T& operator[](size_t i) const;

        constexpr T *Data();
        constexpr const T *Data() const;

        constexpr static TQuaternion zero = TQuaternion(0, 0, 0, 0);
        constexpr static TQuaternion identity = TQuaternion();

        union {
            struct { T w, x, y, z; };
            struct { T r, i, j, k; };
        };
    };

    template<typename T>
    constexpr TQuaternion<T>::TQuaternion(T w, T x, T y, T z): w(w), x(x), y(y), z(z) {}
    template<typename T>
    constexpr TQuaternion<T>::TQuaternion(T (&arr)[4]): w(arr[0]), x(arr[1]), y(arr[2]), z(arr[3]) {}
    template<typename T>
    constexpr TQuaternion<T>::TQuaternion(const TQuaternion& q): w(q.w), x(q.x), y(q.y), z(q.z) {}
    template<typename T>
    constexpr TQuaternion<T>::TQuaternion(TQuaternion&& q) noexcept: w(q.w), x(q.x), y(q.y), z(q.z) {}

    template<typename T>
    constexpr TQuaternion<T>& TQuaternion<T>::operator=(const TQuaternion& q)
    {
        if (&q == this) {
            return *this;
        }
        w = q.w;
        x = q.x;
        y = q.y;
        z = q.z;
        return *this;
    }
    template<typename T>
    constexpr TQuaternion<T>& TQuaternion<T>::operator=(TQuaternion&& q) noexcept
    {
        w = q.w;
        x = q.x;
        y = q.y;
        z = q.z;
        return *this;
    }

    template<typename T>
    constexpr size_t TQuaternion<T>::Length() { return 4; }
    template<typename T>
    constexpr T& TQuaternion<T>::operator[](size_t i)
    {
        OI_ASSERT(0 <= i && i < Length());
        return (&w)[i];
    }
    template<typename T>
    constexpr const T& TQuaternion<T>::operator[](size_t i) const
    {
        OI_ASSERT(0 <= i && i < Length());
        return (&w)[i];
    }

    template<typename T>
    constexpr T* TQuaternion<T>::Data()
    {
        return &w;
    }
    template<typename T>
    constexpr const T* TQuaternion<T>::Data() const
    {
        return &w;
    }

    using Quaternionf = TQuaternion<float>;
    using Quaterniond = TQuaternion<double>;

    using Quaternion = Quaternionf;
}
