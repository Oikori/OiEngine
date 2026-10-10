#pragma once

#include "Engine/Math/Vector3.h"

namespace OiEngine::Math {
    template<typename T>
    struct OI_API TMatrix3x3 {
        constexpr explicit TMatrix3x3(T diagonal = 0);
        constexpr TMatrix3x3(const TVector3<T>& c0, const TVector3<T>& c1, const TVector3<T>& c2);
        constexpr explicit TMatrix3x3(T (&arr)[9]);
        constexpr TMatrix3x3(const TMatrix3x3& m);
        constexpr TMatrix3x3(TMatrix3x3&& m) noexcept;

        constexpr TMatrix3x3& operator=(const TMatrix3x3& m);
        constexpr TMatrix3x3& operator=(TMatrix3x3&& m) noexcept;

        static constexpr size_t Rows();
        static constexpr size_t Columns();
        constexpr TVector3<T>& operator[](size_t i);
        constexpr const TVector3<T>& operator[](size_t i) const;

        constexpr T *Data();
        constexpr const T *Data() const;

        static const TMatrix3x3 zero;
        static const TMatrix3x3 identity;

    private:
        union {
            TVector3<T> columns[3];
            struct { TVector3<T> c0, c1, c2; };
        };
    };

    template<typename T>
    const TMatrix3x3<T> TMatrix3x3<T>::zero = TMatrix3x3();
    template<typename T>
    const TMatrix3x3<T> TMatrix3x3<T>::identity = TMatrix3x3(1);

    template<typename T>
    constexpr TMatrix3x3<T>::TMatrix3x3(T diagonal):
        columns{
            TVector3<T>(diagonal),
            TVector3<T>(0, diagonal),
            TVector3<T>(0, 0, diagonal)
        } {}
    template<typename T>
    constexpr TMatrix3x3<T>::TMatrix3x3(const TVector3<T>& c0, const TVector3<T>& c1, const TVector3<T>& c2):
        columns{ c0, c1, c2 } {}
    template<typename T>
    constexpr TMatrix3x3<T>::TMatrix3x3(T (&arr)[9]):
        columns{
            TVector3<T>(arr[0], arr[1], arr[2]),
            TVector3<T>(arr[3], arr[4], arr[5]),
            TVector3<T>(arr[6], arr[7], arr[8])
        } {}
    template<typename T>
    constexpr TMatrix3x3<T>::TMatrix3x3(const TMatrix3x3& m):
        columns{ m.columns[0], m.columns[1], m.columns[2] } {}
    template<typename T>
    constexpr TMatrix3x3<T>::TMatrix3x3(TMatrix3x3&& m) noexcept:
        columns{ m.columns[0], m.columns[1], m.columns[2] } {}

    template<typename T>
    constexpr TMatrix3x3<T>& TMatrix3x3<T>::operator=(const TMatrix3x3& m)
    {
        if (&m == this) {
            return *this;
        }
        columns[0] = m.columns[0];
        columns[1] = m.columns[1];
        columns[2] = m.columns[2];
        return *this;
    }
    template<typename T>
    constexpr TMatrix3x3<T>& TMatrix3x3<T>::operator=(TMatrix3x3&& m) noexcept
    {
        columns[0] = m.columns[0];
        columns[1] = m.columns[1];
        columns[2] = m.columns[2];
        return *this;
    }

    template<typename T>
    constexpr size_t TMatrix3x3<T>::Rows() { return 3; }
    template<typename T>
    constexpr size_t TMatrix3x3<T>::Columns() { return 3; }
    template<typename T>
    constexpr TVector3<T>& TMatrix3x3<T>::operator[](size_t i)
    {
        OI_ASSERT(0 <= i && i < Columns());
        return columns[i];
    }
    template<typename T>
    constexpr const TVector3<T>& TMatrix3x3<T>::operator[](size_t i) const
    {
        OI_ASSERT(0 <= i && i < Columns());
        return columns[i];
    }

    template<typename T>
    constexpr T* TMatrix3x3<T>::Data()
    {
        return columns[0].Data();
    }
    template<typename T>
    constexpr const T* TMatrix3x3<T>::Data() const
    {
        return columns[0].Data();
    }

    using Matrix3x3i = TMatrix3x3<int>;
    using Matrix3x3f = TMatrix3x3<float>;
    using Matrix3x3d = TMatrix3x3<double>;

    using Matrix3 = Matrix3x3f;
}
