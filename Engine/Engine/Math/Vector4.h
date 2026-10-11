#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    // 注意：模板 struct 不加 OI_API——头文件内实例化即可；若标为 dllimport，
    // 静态成员（zero 等）在 DLL 客户端会因未导出而链接失败
    template<typename T>
    struct TVector4 {
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

        template<typename U>
        constexpr TVector4& operator+=(U scalar);
        constexpr TVector4& operator+=(const TVector4& v);
        template<typename U>
        constexpr TVector4& operator-=(U scalar);
        constexpr TVector4& operator-=(const TVector4& v);
        template<typename U>
        constexpr TVector4& operator*=(U scalar);
        constexpr TVector4& operator*=(const TVector4& v);
        template<typename U>
        constexpr TVector4& operator/=(U scalar);
        constexpr TVector4& operator/=(const TVector4& v);

        // -- Increment and decrement operators --

        constexpr TVector4& operator++();
        constexpr TVector4& operator--();
        constexpr TVector4 operator++(int);
        constexpr TVector4 operator--(int);

        // -- Unary bit operators --

        template<typename U>
        constexpr TVector4& operator%=(U scalar);
        constexpr TVector4& operator%=(const TVector4& v);
        template<typename U>
        constexpr TVector4& operator&=(U scalar);
        constexpr TVector4& operator&=(const TVector4& v);
        template<typename U>
        constexpr TVector4& operator|=(U scalar);
        constexpr TVector4& operator|=(const TVector4& v);
        template<typename U>
        constexpr TVector4& operator^=(U scalar);
        constexpr TVector4& operator^=(const TVector4& v);
        template<typename U>
        constexpr TVector4& operator<<=(U scalar);
        constexpr TVector4& operator<<=(const TVector4& v);
        template<typename U>
        constexpr TVector4& operator>>=(U scalar);
        constexpr TVector4& operator>>=(const TVector4& v);

        static const TVector4 zero;

        union {
            struct { T x, y, z, w; };
            struct { T r, g, b, a; };
        };
    };

    using Vector4i = TVector4<int>;
    using Point4i = TVector4<int>;
    using Vector4f = TVector4<float>;
    using Point4f = TVector4<float>;
    using Vector4d = TVector4<double>;
    using Point4d = TVector4<double>;

    using Vector4 = Vector4f;

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

    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator+=(U scalar)
    {
        x += static_cast<T>(scalar);
        y += static_cast<T>(scalar);
        z += static_cast<T>(scalar);
        w += static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator+=(const TVector4<T>& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        w += v.w;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator-=(U scalar)
    {
        x -= static_cast<T>(scalar);
        y -= static_cast<T>(scalar);
        z -= static_cast<T>(scalar);
        w -= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator-=(const TVector4<T>& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        w -= v.w;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator*=(U scalar)
    {
        x *= static_cast<T>(scalar);
        y *= static_cast<T>(scalar);
        z *= static_cast<T>(scalar);
        w *= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator*=(const TVector4<T>& v)
    {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        w *= v.w;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator/=(U scalar)
    {
        x /= static_cast<T>(scalar);
        y /= static_cast<T>(scalar);
        z /= static_cast<T>(scalar);
        w /= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator/=(const TVector4<T>& v)
    {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        w /= v.w;
        return *this;
    }

    // -- Increment and decrement operators --

    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator++()
    {
        ++x;
        ++y;
        ++z;
        ++w;
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator--()
    {
        --x;
        --y;
        --z;
        --w;
        return *this;
    }
    template<typename T>
    constexpr TVector4<T> TVector4<T>::operator++(int)
    {
        TVector4<T> result(*this);
        ++(*this);
        return result;
    }
    template<typename T>
    constexpr TVector4<T> TVector4<T>::operator--(int)
    {
        TVector4<T> result(*this);
        --(*this);
        return result;
    }

    // -- Unary bit operators --

    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator%=(U scalar)
    {
        x %= static_cast<T>(scalar);
        y %= static_cast<T>(scalar);
        z %= static_cast<T>(scalar);
        w %= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator%=(const TVector4<T>& v)
    {
        x %= v.x;
        y %= v.y;
        z %= v.z;
        w %= v.w;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator&=(U scalar)
    {
        x &= static_cast<T>(scalar);
        y &= static_cast<T>(scalar);
        z &= static_cast<T>(scalar);
        w &= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator&=(const TVector4<T>& v)
    {
        x &= v.x;
        y &= v.y;
        z &= v.z;
        w &= v.w;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator|=(U scalar)
    {
        x |= static_cast<T>(scalar);
        y |= static_cast<T>(scalar);
        z |= static_cast<T>(scalar);
        w |= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator|=(const TVector4<T>& v)
    {
        x |= v.x;
        y |= v.y;
        z |= v.z;
        w |= v.w;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator^=(U scalar)
    {
        x ^= static_cast<T>(scalar);
        y ^= static_cast<T>(scalar);
        z ^= static_cast<T>(scalar);
        w ^= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator^=(const TVector4<T>& v)
    {
        x ^= v.x;
        y ^= v.y;
        z ^= v.z;
        w ^= v.w;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator<<=(U scalar)
    {
        x <<= static_cast<T>(scalar);
        y <<= static_cast<T>(scalar);
        z <<= static_cast<T>(scalar);
        w <<= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator<<=(const TVector4<T>& v)
    {
        x <<= v.x;
        y <<= v.y;
        z <<= v.z;
        w <<= v.w;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector4<T>& TVector4<T>::operator>>=(U scalar)
    {
        x >>= static_cast<T>(scalar);
        y >>= static_cast<T>(scalar);
        z >>= static_cast<T>(scalar);
        w >>= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector4<T>& TVector4<T>::operator>>=(const TVector4<T>& v)
    {
        x >>= v.x;
        y >>= v.y;
        z >>= v.z;
        w >>= v.w;
        return *this;
    }

    // -- Unary operators --

    template<typename T>
    constexpr TVector4<T> operator+(const TVector4<T>& v)
    {
        return v;
    }

    template<typename T>
    constexpr TVector4<T> operator-(const TVector4<T>& v)
    {
        return {-v.x, -v.y, -v.z, -v.w};
    }

    // -- Binary operators --

    template<typename T, typename U>
    constexpr TVector4<T> operator+(const TVector4<T>& v, U scalar)
    {
        return {v.x + static_cast<T>(scalar), v.y + static_cast<T>(scalar), v.z + static_cast<T>(scalar), v.w + static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator+(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) + v.x, static_cast<T>(scalar) + v.y, static_cast<T>(scalar) + v.z, static_cast<T>(scalar) + v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator+(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator-(const TVector4<T>& v, U scalar)
    {
        return {v.x - static_cast<T>(scalar), v.y - static_cast<T>(scalar), v.z - static_cast<T>(scalar), v.w - static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator-(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) - v.x, static_cast<T>(scalar) - v.y, static_cast<T>(scalar) - v.z, static_cast<T>(scalar) - v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator-(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator*(const TVector4<T>& v, U scalar)
    {
        return {v.x * static_cast<T>(scalar), v.y * static_cast<T>(scalar), v.z * static_cast<T>(scalar), v.w * static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator*(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) * v.x, static_cast<T>(scalar) * v.y, static_cast<T>(scalar) * v.z, static_cast<T>(scalar) * v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator*(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x * v2.x, v1.y * v2.y, v1.z * v2.z, v1.w * v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator/(const TVector4<T>& v, U scalar)
    {
        return {v.x / static_cast<T>(scalar), v.y / static_cast<T>(scalar), v.z / static_cast<T>(scalar), v.w / static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator/(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) / v.x, static_cast<T>(scalar) / v.y, static_cast<T>(scalar) / v.z, static_cast<T>(scalar) / v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator/(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x / v2.x, v1.y / v2.y, v1.z / v2.z, v1.w / v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator%(const TVector4<T>& v, U scalar)
    {
        return {v.x % static_cast<T>(scalar), v.y % static_cast<T>(scalar), v.z % static_cast<T>(scalar), v.w % static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator%(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) % v.x, static_cast<T>(scalar) % v.y, static_cast<T>(scalar) % v.z, static_cast<T>(scalar) % v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator%(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x % v2.x, v1.y % v2.y, v1.z % v2.z, v1.w % v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator&(const TVector4<T>& v, U scalar)
    {
        return {v.x & static_cast<T>(scalar), v.y & static_cast<T>(scalar), v.z & static_cast<T>(scalar), v.w & static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator&(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) & v.x, static_cast<T>(scalar) & v.y, static_cast<T>(scalar) & v.z, static_cast<T>(scalar) & v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator&(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x & v2.x, v1.y & v2.y, v1.z & v2.z, v1.w & v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator|(const TVector4<T>& v, U scalar)
    {
        return {v.x | static_cast<T>(scalar), v.y | static_cast<T>(scalar), v.z | static_cast<T>(scalar), v.w | static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator|(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) | v.x, static_cast<T>(scalar) | v.y, static_cast<T>(scalar) | v.z, static_cast<T>(scalar) | v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator|(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x | v2.x, v1.y | v2.y, v1.z | v2.z, v1.w | v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator^(const TVector4<T>& v, U scalar)
    {
        return {v.x ^ static_cast<T>(scalar), v.y ^ static_cast<T>(scalar), v.z ^ static_cast<T>(scalar), v.w ^ static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator^(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) ^ v.x, static_cast<T>(scalar) ^ v.y, static_cast<T>(scalar) ^ v.z, static_cast<T>(scalar) ^ v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator^(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x ^ v2.x, v1.y ^ v2.y, v1.z ^ v2.z, v1.w ^ v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator<<(const TVector4<T>& v, U scalar)
    {
        return {v.x << static_cast<T>(scalar), v.y << static_cast<T>(scalar), v.z << static_cast<T>(scalar), v.w << static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator<<(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) << v.x, static_cast<T>(scalar) << v.y, static_cast<T>(scalar) << v.z, static_cast<T>(scalar) << v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator<<(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x << v2.x, v1.y << v2.y, v1.z << v2.z, v1.w << v2.w};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator>>(const TVector4<T>& v, U scalar)
    {
        return {v.x >> static_cast<T>(scalar), v.y >> static_cast<T>(scalar), v.z >> static_cast<T>(scalar), v.w >> static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector4<T> operator>>(U scalar, const TVector4<T>& v)
    {
        return {static_cast<T>(scalar) >> v.x, static_cast<T>(scalar) >> v.y, static_cast<T>(scalar) >> v.z, static_cast<T>(scalar) >> v.w};
    }

    template<typename T>
    constexpr TVector4<T> operator>>(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return {v1.x >> v2.x, v1.y >> v2.y, v1.z >> v2.z, v1.w >> v2.w};
    }

    template<typename T>
    constexpr TVector4<T> operator~(const TVector4<T>& v)
    {
        return {~v.x, ~v.y, ~v.z, ~v.w};
    }

    // -- Boolean operators --

    template<typename T>
    constexpr bool operator==(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z && v1.w == v2.w;
    }

    template<typename T>
    constexpr bool operator!=(const TVector4<T>& v1, const TVector4<T>& v2)
    {
        return !(v1 == v2);
    }
}
