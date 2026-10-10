#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    template<typename T>
    struct OI_API TVector3 {
        constexpr TVector3(T x = 0, T y = 0, T z = 0);
        constexpr TVector3(T (&arr)[3]);
        constexpr TVector3(const TVector3& v);
        constexpr TVector3(TVector3&& v) noexcept;

        constexpr TVector3& operator=(const TVector3& v);
        constexpr TVector3& operator=(TVector3&& v) noexcept;

        static constexpr size_t Length();
        constexpr T& operator[](size_t i);
        constexpr const T& operator[](size_t i) const;

        constexpr T *Data();
        constexpr const T *Data() const;

        static const TVector3 zero;

        union {
            struct { T x, y, z; };
            struct { T r, g, b; };
        };
    };

    template<typename T>
    const TVector3<T> TVector3<T>::zero = TVector3();

    template<typename T>
    constexpr TVector3<T>::TVector3(T x, T y, T z): x(x), y(y), z(z) {}
    template<typename T>
    constexpr TVector3<T>::TVector3(T (&arr)[3]): x(arr[0]), y(arr[1]), z(arr[2]) {}
    template<typename T>
    constexpr TVector3<T>::TVector3(const TVector3& v): x(v.x), y(v.y), z(v.z) {}
    template<typename T>
    constexpr TVector3<T>::TVector3(TVector3&& v) noexcept: x(v.x), y(v.y), z(v.z) {}

    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator=(const TVector3& v)
    {
        if (&v == this) {
            return *this;
        }
        x = v.x;
        y = v.y;
        z = v.z;
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator=(TVector3&& v) noexcept
    {
        x = v.x;
        y = v.y;
        z = v.z;
        return *this;
    }

    template<typename T>
    constexpr size_t TVector3<T>::Length() { return 3; }
    template<typename T>
    constexpr T& TVector3<T>::operator[](size_t i)
    {
        OI_ASSERT(0 <= i && i < Length());
        return (&x)[i];
    }
    template<typename T>
    constexpr const T& TVector3<T>::operator[](size_t i) const
    {
        OI_ASSERT(0 <= i && i < Length());
        return (&x)[i];
    }

    template<typename T>
    constexpr T* TVector3<T>::Data()
    {
        return &x;
    }
    template<typename T>
    constexpr const T* TVector3<T>::Data() const
    {
        return &x;
    }

    using Vector3i = TVector3<int>;
    using Point3i = TVector3<int>;
    using Vector3f = TVector3<float>;
    using Point3f = TVector3<float>;
    using Vector3d = TVector3<double>;
    using Point3d = TVector3<double>;

    using Vector3 = Vector3f;
    using Point3 = Point3f;
}
