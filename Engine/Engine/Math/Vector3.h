#pragma once

#include "Engine/Core/Base.h"

namespace OiEngine::Math {
    // 注意：模板 struct 不加 OI_API——头文件内实例化即可；若标为 dllimport，
    // 静态成员（zero 等）在 DLL 客户端会因未导出而链接失败
    template<typename T>
    struct TVector3 {
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

        template<typename U>
        constexpr TVector3& operator+=(U scalar);
        constexpr TVector3& operator+=(const TVector3& v);
        template<typename U>
        constexpr TVector3& operator-=(U scalar);
        constexpr TVector3& operator-=(const TVector3& v);
        template<typename U>
        constexpr TVector3& operator*=(U scalar);
        constexpr TVector3& operator*=(const TVector3& v);
        template<typename U>
        constexpr TVector3& operator/=(U scalar);
        constexpr TVector3& operator/=(const TVector3& v);

        // -- Increment and decrement operators --

        constexpr TVector3& operator++();
        constexpr TVector3& operator--();
        constexpr TVector3 operator++(int);
        constexpr TVector3 operator--(int);

        // -- Unary bit operators --

        template<typename U>
        constexpr TVector3& operator%=(U scalar);
        constexpr TVector3& operator%=(const TVector3& v);
        template<typename U>
        constexpr TVector3& operator&=(U scalar);
        constexpr TVector3& operator&=(const TVector3& v);
        template<typename U>
        constexpr TVector3& operator|=(U scalar);
        constexpr TVector3& operator|=(const TVector3& v);
        template<typename U>
        constexpr TVector3& operator^=(U scalar);
        constexpr TVector3& operator^=(const TVector3& v);
        template<typename U>
        constexpr TVector3& operator<<=(U scalar);
        constexpr TVector3& operator<<=(const TVector3& v);
        template<typename U>
        constexpr TVector3& operator>>=(U scalar);
        constexpr TVector3& operator>>=(const TVector3& v);

        static const TVector3 zero;

        union {
            struct { T x, y, z; };
            struct { T r, g, b; };
        };
    };

    using Vector3i = TVector3<int>;
    using Point3i = TVector3<int>;
    using Vector3f = TVector3<float>;
    using Point3f = TVector3<float>;
    using Vector3d = TVector3<double>;
    using Point3d = TVector3<double>;

    using Vector3 = Vector3f;
    using Point3 = Point3f;

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

    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator+=(U scalar)
    {
        x += static_cast<T>(scalar);
        y += static_cast<T>(scalar);
        z += static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator+=(const TVector3<T>& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator-=(U scalar)
    {
        x -= static_cast<T>(scalar);
        y -= static_cast<T>(scalar);
        z -= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator-=(const TVector3<T>& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator*=(U scalar)
    {
        x *= static_cast<T>(scalar);
        y *= static_cast<T>(scalar);
        z *= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator*=(const TVector3<T>& v)
    {
        x *= v.x;
        y *= v.y;
        z *= v.z;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator/=(U scalar)
    {
        x /= static_cast<T>(scalar);
        y /= static_cast<T>(scalar);
        z /= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator/=(const TVector3<T>& v)
    {
        x /= v.x;
        y /= v.y;
        z /= v.z;
        return *this;
    }

    // -- Increment and decrement operators --

    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator++()
    {
        ++x;
        ++y;
        ++z;
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator--()
    {
        --x;
        --y;
        --z;
        return *this;
    }
    template<typename T>
    constexpr TVector3<T> TVector3<T>::operator++(int)
    {
        TVector3<T> result(*this);
        ++(*this);
        return result;
    }
    template<typename T>
    constexpr TVector3<T> TVector3<T>::operator--(int)
    {
        TVector3<T> result(*this);
        --(*this);
        return result;
    }

    // -- Unary bit operators --

    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator%=(U scalar)
    {
        x %= static_cast<T>(scalar);
        y %= static_cast<T>(scalar);
        z %= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator%=(const TVector3<T>& v)
    {
        x %= v.x;
        y %= v.y;
        z %= v.z;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator&=(U scalar)
    {
        x &= static_cast<T>(scalar);
        y &= static_cast<T>(scalar);
        z &= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator&=(const TVector3<T>& v)
    {
        x &= v.x;
        y &= v.y;
        z &= v.z;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator|=(U scalar)
    {
        x |= static_cast<T>(scalar);
        y |= static_cast<T>(scalar);
        z |= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator|=(const TVector3<T>& v)
    {
        x |= v.x;
        y |= v.y;
        z |= v.z;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator^=(U scalar)
    {
        x ^= static_cast<T>(scalar);
        y ^= static_cast<T>(scalar);
        z ^= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator^=(const TVector3<T>& v)
    {
        x ^= v.x;
        y ^= v.y;
        z ^= v.z;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator<<=(U scalar)
    {
        x <<= static_cast<T>(scalar);
        y <<= static_cast<T>(scalar);
        z <<= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator<<=(const TVector3<T>& v)
    {
        x <<= v.x;
        y <<= v.y;
        z <<= v.z;
        return *this;
    }
    template<typename T>
    template<typename U>
    constexpr TVector3<T>& TVector3<T>::operator>>=(U scalar)
    {
        x >>= static_cast<T>(scalar);
        y >>= static_cast<T>(scalar);
        z >>= static_cast<T>(scalar);
        return *this;
    }
    template<typename T>
    constexpr TVector3<T>& TVector3<T>::operator>>=(const TVector3<T>& v)
    {
        x >>= v.x;
        y >>= v.y;
        z >>= v.z;
        return *this;
    }

    // -- Unary operators --

    template<typename T>
    constexpr TVector3<T> operator+(const TVector3<T>& v)
    {
        return v;
    }

    template<typename T>
    constexpr TVector3<T> operator-(const TVector3<T>& v)
    {
        return {-v.x, -v.y, -v.z};
    }

    // -- Binary operators --

    template<typename T, typename U>
    constexpr TVector3<T> operator+(const TVector3<T>& v, U scalar)
    {
        return {v.x + static_cast<T>(scalar), v.y + static_cast<T>(scalar), v.z + static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator+(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) + v.x, static_cast<T>(scalar) + v.y, static_cast<T>(scalar) + v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator+(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator-(const TVector3<T>& v, U scalar)
    {
        return {v.x - static_cast<T>(scalar), v.y - static_cast<T>(scalar), v.z - static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator-(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) - v.x, static_cast<T>(scalar) - v.y, static_cast<T>(scalar) - v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator-(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator*(const TVector3<T>& v, U scalar)
    {
        return {v.x * static_cast<T>(scalar), v.y * static_cast<T>(scalar), v.z * static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator*(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) * v.x, static_cast<T>(scalar) * v.y, static_cast<T>(scalar) * v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator*(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x * v2.x, v1.y * v2.y, v1.z * v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator/(const TVector3<T>& v, U scalar)
    {
        return {v.x / static_cast<T>(scalar), v.y / static_cast<T>(scalar), v.z / static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator/(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) / v.x, static_cast<T>(scalar) / v.y, static_cast<T>(scalar) / v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator/(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x / v2.x, v1.y / v2.y, v1.z / v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator%(const TVector3<T>& v, U scalar)
    {
        return {v.x % static_cast<T>(scalar), v.y % static_cast<T>(scalar), v.z % static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator%(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) % v.x, static_cast<T>(scalar) % v.y, static_cast<T>(scalar) % v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator%(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x % v2.x, v1.y % v2.y, v1.z % v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator&(const TVector3<T>& v, U scalar)
    {
        return {v.x & static_cast<T>(scalar), v.y & static_cast<T>(scalar), v.z & static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator&(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) & v.x, static_cast<T>(scalar) & v.y, static_cast<T>(scalar) & v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator&(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x & v2.x, v1.y & v2.y, v1.z & v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator|(const TVector3<T>& v, U scalar)
    {
        return {v.x | static_cast<T>(scalar), v.y | static_cast<T>(scalar), v.z | static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator|(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) | v.x, static_cast<T>(scalar) | v.y, static_cast<T>(scalar) | v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator|(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x | v2.x, v1.y | v2.y, v1.z | v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator^(const TVector3<T>& v, U scalar)
    {
        return {v.x ^ static_cast<T>(scalar), v.y ^ static_cast<T>(scalar), v.z ^ static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator^(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) ^ v.x, static_cast<T>(scalar) ^ v.y, static_cast<T>(scalar) ^ v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator^(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x ^ v2.x, v1.y ^ v2.y, v1.z ^ v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator<<(const TVector3<T>& v, U scalar)
    {
        return {v.x << static_cast<T>(scalar), v.y << static_cast<T>(scalar), v.z << static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator<<(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) << v.x, static_cast<T>(scalar) << v.y, static_cast<T>(scalar) << v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator<<(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x << v2.x, v1.y << v2.y, v1.z << v2.z};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator>>(const TVector3<T>& v, U scalar)
    {
        return {v.x >> static_cast<T>(scalar), v.y >> static_cast<T>(scalar), v.z >> static_cast<T>(scalar)};
    }

    template<typename T, typename U>
    constexpr TVector3<T> operator>>(U scalar, const TVector3<T>& v)
    {
        return {static_cast<T>(scalar) >> v.x, static_cast<T>(scalar) >> v.y, static_cast<T>(scalar) >> v.z};
    }

    template<typename T>
    constexpr TVector3<T> operator>>(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return {v1.x >> v2.x, v1.y >> v2.y, v1.z >> v2.z};
    }

    template<typename T>
    constexpr TVector3<T> operator~(const TVector3<T>& v)
    {
        return {~v.x, ~v.y, ~v.z};
    }

    // -- Boolean operators --

    template<typename T>
    constexpr bool operator==(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
    }

    template<typename T>
    constexpr bool operator!=(const TVector3<T>& v1, const TVector3<T>& v2)
    {
        return !(v1 == v2);
    }
}
