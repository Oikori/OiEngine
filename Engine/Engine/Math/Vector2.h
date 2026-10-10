#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    template<typename T>
    struct OI_API TVector2 {
        constexpr TVector2(T x = 0, T y = 0);
        constexpr TVector2(T (&arr)[2]);
        constexpr TVector2(const TVector2& v);
        constexpr TVector2(TVector2&& v) noexcept;

        constexpr TVector2& operator=(const TVector2& v);
        constexpr TVector2& operator=(TVector2&& v) noexcept;

        static constexpr size_t Length();
        constexpr T& operator[](size_t i);
        constexpr const T& operator[](size_t i) const;

        constexpr T *Data();
        constexpr const T *Data() const;

        static const TVector2 zero;

        union {
            struct { T x, y; };
            struct { T r, g; };
        };
    };

    template<typename T>
    const TVector2<T> TVector2<T>::zero = TVector2();

    template<typename T>
    constexpr TVector2<T>::TVector2(T x, T y): x(x), y(y) {}
    template<typename T>
    constexpr TVector2<T>::TVector2(T (&arr)[2]): x(arr[0]), y(arr[1]) {}
    template<typename T>
    constexpr TVector2<T>::TVector2(const TVector2& v): x(v.x), y(v.y) {}
    template<typename T>
    constexpr TVector2<T>::TVector2(TVector2&& v) noexcept: x(v.x), y(v.y) {}

    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator=(const TVector2& v)
    {
        if (&v == this) {
            return *this;
        }
        x = v.x;
        y = v.y;
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator=(TVector2&& v) noexcept
    {
        x = v.x;
        y = v.y;
        return *this;
    }

    template<typename T>
    constexpr size_t TVector2<T>::Length() { return 2; }
    template<typename T>
    constexpr T& TVector2<T>::operator[](size_t i)
    {
        OI_ASSERT(0 <= i && i < Length());
        return (&x)[i];
    }
    template<typename T>
    constexpr const T& TVector2<T>::operator[](size_t i) const
    {
        OI_ASSERT(0 <= i && i < Length());
        return (&x)[i];
    }

    template<typename T>
    constexpr T* TVector2<T>::Data()
    {
        return &x;
    }
    template<typename T>
    constexpr const T* TVector2<T>::Data() const
    {
        return &x;
    }

    using Vector2i = TVector2<int>;
    using Point2i = TVector2<int>;
    using Vector2f = TVector2<float>;
    using Point2f = TVector2<float>;
    using Vector2d = TVector2<double>;
    using Point2d = TVector2<double>;

    using Vector2 = Vector2f;
    using Point2 = Point2f;
}
