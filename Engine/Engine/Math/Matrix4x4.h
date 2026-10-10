#pragma once

#include "Engine/Math/Vector4.h"

namespace OiEngine::Math {
    template<typename T>
    struct OI_API TMatrix4x4 {
        constexpr explicit TMatrix4x4(T diagonal = 0);
        constexpr TMatrix4x4(const TVector4<T>& c0, const TVector4<T>& c1, const TVector4<T>& c2, const TVector4<T>& c3);
        constexpr explicit TMatrix4x4(T (&arr)[16]);
        constexpr TMatrix4x4(const TMatrix4x4& m);
        constexpr TMatrix4x4(TMatrix4x4&& m) noexcept;

        constexpr TMatrix4x4& operator=(const TMatrix4x4& m);
        constexpr TMatrix4x4& operator=(TMatrix4x4&& m) noexcept;

        static constexpr size_t Rows();
        static constexpr size_t Columns();
        constexpr TVector4<T>& operator[](size_t i);
        constexpr const TVector4<T>& operator[](size_t i) const;

        constexpr T *Data();
        constexpr const T *Data() const;

        static const TMatrix4x4 zero;
        static const TMatrix4x4 identity;

    private:
        union {
            TVector4<T> columns[4];
            struct { TVector4<T> c0, c1, c2, c3; };
        };
    };

    template<typename T>
    const TMatrix4x4<T> TMatrix4x4<T>::zero = TMatrix4x4();
    template<typename T>
    const TMatrix4x4<T> TMatrix4x4<T>::identity = TMatrix4x4(1);

    template<typename T>
    constexpr TMatrix4x4<T>::TMatrix4x4(T diagonal):
        columns{
            TVector4<T>(diagonal),
            TVector4<T>(0, diagonal),
            TVector4<T>(0, 0, diagonal),
            TVector4<T>(0, 0, 0, diagonal)
        } {}
    template<typename T>
    constexpr TMatrix4x4<T>::TMatrix4x4(const TVector4<T>& c0, const TVector4<T>& c1, const TVector4<T>& c2, const TVector4<T>& c3):
        columns{ c0, c1, c2, c3 } {}
    template<typename T>
    constexpr TMatrix4x4<T>::TMatrix4x4(T (&arr)[16]):
        columns{
            TVector4<T>(arr[0], arr[1], arr[2], arr[3]),
            TVector4<T>(arr[4], arr[5], arr[6], arr[7]),
            TVector4<T>(arr[8], arr[9], arr[10], arr[11]),
            TVector4<T>(arr[12], arr[13], arr[14], arr[15])
        } {}
    template<typename T>
    constexpr TMatrix4x4<T>::TMatrix4x4(const TMatrix4x4& m):
        columns{ m.columns[0], m.columns[1], m.columns[2], m.columns[3] } {}
    template<typename T>
    constexpr TMatrix4x4<T>::TMatrix4x4(TMatrix4x4&& m) noexcept:
        columns{ m.columns[0], m.columns[1], m.columns[2], m.columns[3] } {}

    template<typename T>
    constexpr TMatrix4x4<T>& TMatrix4x4<T>::operator=(const TMatrix4x4& m)
    {
        if (&m == this) {
            return *this;
        }
        columns[0] = m.columns[0];
        columns[1] = m.columns[1];
        columns[2] = m.columns[2];
        columns[3] = m.columns[3];
        return *this;
    }
    template<typename T>
    constexpr TMatrix4x4<T>& TMatrix4x4<T>::operator=(TMatrix4x4&& m) noexcept
    {
        columns[0] = m.columns[0];
        columns[1] = m.columns[1];
        columns[2] = m.columns[2];
        columns[3] = m.columns[3];
        return *this;
    }

    template<typename T>
    constexpr size_t TMatrix4x4<T>::Rows() { return 4; }
    template<typename T>
    constexpr size_t TMatrix4x4<T>::Columns() { return 4; }
    template<typename T>
    constexpr TVector4<T>& TMatrix4x4<T>::operator[](size_t i)
    {
        OI_ASSERT(0 <= i && i < Columns());
        return columns[i];
    }
    template<typename T>
    constexpr const TVector4<T>& TMatrix4x4<T>::operator[](size_t i) const
    {
        OI_ASSERT(0 <= i && i < Columns());
        return columns[i];
    }

    template<typename T>
    constexpr T* TMatrix4x4<T>::Data()
    {
        return columns[0].Data();
    }
    template<typename T>
    constexpr const T* TMatrix4x4<T>::Data() const
    {
        return columns[0].Data();
    }

    using Matrix4x4i = TMatrix4x4<int>;
    using Matrix4x4f = TMatrix4x4<float>;
    using Matrix4x4d = TMatrix4x4<double>;

    using Matrix4 = Matrix4x4f;
}
