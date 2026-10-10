#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    template<typename T>
    struct OI_API TVector4 {
        constexpr TVector4(T x = 0, T y = 0, T z = 0, T w = 0);
        constexpr TVector4(T (&arr)[4]);
        constexpr TVector4(const TVector4& v);
        constexpr TVector4(TVector4&& v) noexcept;

        constexpr TVector4& operator=(const TVector4& v);
        constexpr TVector4& operator=(TVector4&& v) noexcept;

        static constexpr size_t Length();
        constexpr T& operator[](size_t i);
        constexpr const T& operator[](size_t i) const;

        constexpr T *Data();
        constexpr const T *Data() const;

        static const TVector4 zero;

        union {
            struct { T x, y, z, w; };
            struct { T r, g, b, a; };
        };
    };

    template<typename T>
    const TVector4<T> TVector4<T>::zero = TVector4();

    template<typename T>
    constexpr TVector4<T>::TVector4(T x, T y, T z, T w): x(x), y(y), z(z), w(w) {}
    template<typename T>
    constexpr TVector4<T>::TVector4(T (&arr)[4]): x(arr[0]), y(arr[1]), z(arr[2]), w(arr[3]) {}
    template<typename T>
    constexpr TVector4<T>::TVector4(const TVector4& v): x(v.x), y(v.y), z(v.z), w(v.w) {}
    template<typename T>
    constexpr TVector4<T>::TVector4(TVector4&& v) noexcept: x(v.x), y(v.y), z(v.z), w(v.w) {}

    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator=(const TVector4& v)
    {
        if (&v == this) {
            return *this;
        }
        x = v.x;
        y = v.y;
        z = v.z;
        w = v.w;
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator=(TVector4&& v) noexcept
    {
        x = v.x;
        y = v.y;
        z = v.z;
        w = v.w;
        return *this;
    }

    template<typename T>
    constexpr size_t TVector4<T>::Length() { return 4; }
    template<typename T>
    constexpr T& TVector4<T>::operator[](size_t i)
    {
        OI_ASSERT(0 <= i && i < Length());
        return (&x)[i];
    }
    template<typename T>
    constexpr const T& TVector4<T>::operator[](size_t i) const
    {
        OI_ASSERT(0 <= i && i < Length());
        return (&x)[i];
    }

    template<typename T>
    constexpr T* TVector4<T>::Data()
    {
        return &x;
    }
    template<typename T>
    constexpr const T* TVector4<T>::Data() const
    {
        return &x;
    }

    using Vector4i = TVector4<int>;
    using Vector4f = TVector4<float>;
    using Vector4d = TVector4<double>;

    using Vector4 = Vector4f;
}
