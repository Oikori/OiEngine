#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    // 注意：模板 struct 不加 OI_API——头文件内实例化即可；若标为 dllimport，
    // 静态成员（zero/identity）在 DLL 客户端会因未导出而链接失败
    template<typename T>
    struct TVector2 {
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

        template<typename U>
		constexpr TVector2& operator+=(U scalar);
		constexpr TVector2& operator+=(const TVector2& v);
		template<typename U>
		constexpr TVector2& operator-=(U scalar);
		constexpr TVector2& operator-=(const TVector2& v);
		template<typename U>
		constexpr TVector2& operator*=(U scalar);
		constexpr TVector2& operator*=(const TVector2& v);
		template<typename U>
		constexpr TVector2& operator/=(U scalar);
		constexpr TVector2& operator/=(const TVector2& v);

		// -- Increment and decrement operators --

		constexpr TVector2& operator++();
		constexpr TVector2& operator--();
		constexpr TVector2 operator++(int);
		constexpr TVector2 operator--(int);

		// -- Unary bit operators --

		template<typename U>
		constexpr TVector2& operator%=(U scalar);
		constexpr TVector2& operator%=(const TVector2& v);
		template<typename U>
		constexpr TVector2& operator&=(U scalar);
		constexpr TVector2& operator&=(const TVector2& v);
		template<typename U>
		constexpr TVector2& operator|=(U scalar);
		constexpr TVector2& operator|=(const TVector2& v);
		template<typename U>
		constexpr TVector2& operator^=(U scalar);
		constexpr TVector2& operator^=(const TVector2& v);
		template<typename U>
		constexpr TVector2& operator<<=(U scalar);
		constexpr TVector2& operator<<=(const TVector2& v);
		template<typename U>
		constexpr TVector2& operator>>=(U scalar);
		constexpr TVector2& operator>>=(const TVector2& v);
        
        static const TVector2 zero;

        union {
            struct { T x, y; };
            struct { T r, g; };
        };
    };

    using Vector2i = TVector2<int>;
    using Point2i = TVector2<int>;
    using Vector2f = TVector2<float>;
    using Point2f = TVector2<float>;
    using Vector2d = TVector2<double>;
    using Point2d = TVector2<double>;

    using Vector2 = Vector2f;
    using Point2 = Point2f;

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

    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator+=(U scalar)
    {
        x += static_cast<T>(scalar);
        y += static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator+=(const TVector2<T>& v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator-=(U scalar)
    {
        x -= static_cast<T>(scalar);
        y -= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator-=(const TVector2<T>& v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator*=(U scalar)
    {
        x *= static_cast<T>(scalar);
        y *= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator*=(const TVector2<T>& v)
    {
        x *= v.x;
        y *= v.y;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator/=(U scalar)
    {
        x /= static_cast<T>(scalar);
        y /= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator/=(const TVector2<T>& v)
    {
        x /= v.x;
        y /= v.y;
        return *this;
    }

    // -- Increment and decrement operators --

    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator++()
    {
        ++x;
        ++y;
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator--()
    {
        --x;
        --y;
        return *this;
    }
    template<typename T>
    constexpr TVector2<T> TVector2<T>::operator++(int)
    {
        TVector2<T> result(*this);
        ++(*this);
        return result;
    }
    template<typename T>
    constexpr TVector2<T> TVector2<T>::operator--(int)
    {
        TVector2<T> result(*this);
        --(*this);
        return result;
    }

    // -- Unary bit operators --

    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator%=(U scalar)
    {
        x %= static_cast<T>(scalar);
        y %= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator%=(const TVector2<T>& v)
    {
        x %= v.x;
        y %= v.y;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator&=(U scalar)
    {
        x &= static_cast<T>(scalar);
        y &= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator&=(const TVector2<T>& v)
    {
        x &= v.x;
        y &= v.y;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator|=(U scalar)
    {
        x |= static_cast<T>(scalar);
        y |= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator|=(const TVector2<T>& v)
    {
        x |= v.x;
        y |= v.y;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator^=(U scalar)
    {
        x ^= static_cast<T>(scalar);
        y ^= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator^=(const TVector2<T>& v)
    {
        x ^= v.x;
        y ^= v.y;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator<<=(U scalar)
    {
        x <<= static_cast<T>(scalar);
        y <<= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator<<=(const TVector2<T>& v)
    {
        x <<= v.x;
        y <<= v.y;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector2<T>& TVector2<T>::operator>>=(U scalar)
    {
        x >>= static_cast<T>(scalar);
        y >>= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector2<T>& TVector2<T>::operator>>=(const TVector2<T>& v)
    {
        x >>= v.x;
        y >>= v.y;
        return *this;
    }
    
    // -- Unary operators --

    template<typename T>
    constexpr TVector2<T> operator+(const TVector2<T>& v)
    {
        return v;
    }

    template<typename T>
    constexpr TVector2<T> operator-(const TVector2<T>& v)
    {
        return {-v.x, -v.y};
    }

    // -- Binary operators --

    template<typename T, typename U>
    constexpr TVector2<T> operator+(const TVector2<T>& v, U scalar)
    {
        return {v.x + static_cast<T>(scalar), v.y + static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator+(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) + v.x, static_cast<T>(scalar) + v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator+(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x + v2.x, v1.y + v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator-(const TVector2<T>& v, U scalar)
    {
        return {v.x - static_cast<T>(scalar), v.y - static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator-(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) - v.x, static_cast<T>(scalar) - v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator-(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x - v2.x, v1.y - v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator*(const TVector2<T>& v, U scalar)
    {
        return {v.x * static_cast<T>(scalar), v.y * static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator*(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) * v.x, static_cast<T>(scalar) * v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator*(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x * v2.x, v1.y * v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator/(const TVector2<T>& v, U scalar)
    {
        return {v.x / static_cast<T>(scalar), v.y / static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator/(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) / v.x, static_cast<T>(scalar) / v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator/(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x / v2.x, v1.y / v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator%(const TVector2<T>& v, U scalar)
    {
        return {v.x % static_cast<T>(scalar), v.y % static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator%(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) % v.x, static_cast<T>(scalar) % v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator%(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x % v2.x, v1.y % v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator&(const TVector2<T>& v, U scalar)
    {
        return {v.x & static_cast<T>(scalar), v.y & static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator&(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) & v.x, static_cast<T>(scalar) & v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator&(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x & v2.x, v1.y & v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator|(const TVector2<T>& v, U scalar)
    {
        return {v.x | static_cast<T>(scalar), v.y | static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator|(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) | v.x, static_cast<T>(scalar) | v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator|(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x | v2.x, v1.y | v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator^(const TVector2<T>& v, U scalar)
    {
        return {v.x ^ static_cast<T>(scalar), v.y ^ static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator^(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) ^ v.x, static_cast<T>(scalar) ^ v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator^(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x ^ v2.x, v1.y ^ v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator<<(const TVector2<T>& v, U scalar)
    {
        return {v.x << static_cast<T>(scalar), v.y << static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator<<(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) << v.x, static_cast<T>(scalar) << v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator<<(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x << v2.x, v1.y << v2.y};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator>>(const TVector2<T>& v, U scalar)
    {
        return {v.x >> static_cast<T>(scalar), v.y >> static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector2<T> operator>>(U scalar, const TVector2<T>& v)
    {
        return {static_cast<T>(scalar) >> v.x, static_cast<T>(scalar) >> v.y};
    }

    template<typename T>
    constexpr TVector2<T> operator>>(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return {v1.x >> v2.x, v1.y >> v2.y};
    }

    template<typename T>
    constexpr TVector2<T> operator~(const TVector2<T>& v)
    {
        return {~v.x, ~v.y};
    }

    // -- Boolean operators --

    template<typename T>
    constexpr bool operator==(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return v1.x == v2.x && v1.y == v2.y;
    }

    template<typename T>
    constexpr bool operator!=(const TVector2<T>& v1, const TVector2<T>& v2)
    {
        return !(v1 == v2);
    }
}
