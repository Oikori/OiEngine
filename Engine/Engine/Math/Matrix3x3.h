#pragma once

#include "Vector3.h"
#include "VectorGeometric.h"

namespace OiEngine::Math {
    // 注意：模板 struct 不加 OI_API——头文件内实例化即可；若标为 dllimport，
    // 静态成员（zero/identity）在 DLL 客户端会因未导出而链接失败
    template<typename T>
    struct TMatrix3x3 {
        using ColType = TVector3<T>;
        using RowType = TVector3<T>;

        constexpr explicit TMatrix3x3(T diagonal = 0);
        constexpr TMatrix3x3(const ColType& c0, const ColType& c1, const ColType& c2);
        constexpr explicit TMatrix3x3(T (&arr)[9]);
        constexpr TMatrix3x3(const TMatrix3x3& m);
        constexpr TMatrix3x3(TMatrix3x3&& m) noexcept;

        constexpr TMatrix3x3& operator=(const TMatrix3x3& m);
        constexpr TMatrix3x3& operator=(TMatrix3x3&& m) noexcept;

        static constexpr size_t Rows();
        static constexpr size_t Columns();
        constexpr ColType& operator[](size_t i);
        constexpr const ColType& operator[](size_t i) const;

        constexpr T *Data();
        constexpr const T *Data() const;

        static const TMatrix3x3 zero;
        static const TMatrix3x3 identity;

    private:
        ColType columns[3];
    };

    using Matrix3x3i = TMatrix3x3<int>;
    using Matrix3x3f = TMatrix3x3<float>;
    using Matrix3x3d = TMatrix3x3<double>;

    using Matrix3 = Matrix3x3f;

    template<typename T>
    const TMatrix3x3<T> TMatrix3x3<T>::zero = TMatrix3x3();
    template<typename T>
    const TMatrix3x3<T> TMatrix3x3<T>::identity = TMatrix3x3(1);

    template<typename T>
    constexpr TMatrix3x3<T>::TMatrix3x3(T diagonal):
        columns{
            ColType(diagonal),
            ColType(0, diagonal),
            ColType(0, 0, diagonal)
        } {}
    template<typename T>
    constexpr TMatrix3x3<T>::TMatrix3x3(const ColType& c0, const ColType& c1, const ColType& c2):
        columns{ c0, c1, c2 } {}
    template<typename T>
    constexpr TMatrix3x3<T>::TMatrix3x3(T (&arr)[9]):
        columns{
            ColType(arr[0], arr[1], arr[2]),
            ColType(arr[3], arr[4], arr[5]),
            ColType(arr[6], arr[7], arr[8])
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
    constexpr TMatrix3x3<T>::ColType& TMatrix3x3<T>::operator[](size_t i)
    {
        OI_ASSERT(0 <= i && i < Columns());
        return columns[i];
    }
    template<typename T>
    constexpr const TMatrix3x3<T>::ColType& TMatrix3x3<T>::operator[](size_t i) const
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

    // 注意：参数需写成可直接推导的类型（TVector3<T>），不能写 typename TMatrix3x3<T>::ColType，
    // 否则与泛型标量版本的 operator* 无法偏序比较，m * v 会产生二义
    template<typename T>
    constexpr TMatrix3x3<T>::ColType operator*(const TMatrix3x3<T>& m, const TVector3<T>& v)
    {
        typename TMatrix3x3<T>::ColType const Mov0(v[0], v[0], v[0]);
        typename TMatrix3x3<T>::ColType const Mov1(v[1], v[1], v[1]);
        typename TMatrix3x3<T>::ColType const Mov2(v[2], v[2], v[2]);
        typename TMatrix3x3<T>::ColType const Mul0 = m[0] * Mov0;
        typename TMatrix3x3<T>::ColType const Mul1 = m[1] * Mov1;
        typename TMatrix3x3<T>::ColType const Mul2 = m[2] * Mov2;
        return Mul0 + Mul1 + Mul2;
    }
    template<typename T>
    constexpr TMatrix3x3<T>::RowType operator*(const TVector3<T>& v, const TMatrix3x3<T>& m)
    {
        return typename TMatrix3x3<T>::RowType(
            Dot(m[0], v),
            Dot(m[1], v),
            Dot(m[2], v));
    }
    template<typename T>
    constexpr TMatrix3x3<T> operator*(const TMatrix3x3<T>& m1, const TMatrix3x3<T>& m2)
    {
        const typename TMatrix3x3<T>::ColType& SrcA0 = m1[0];
        const typename TMatrix3x3<T>::ColType& SrcA1 = m1[1];
        const typename TMatrix3x3<T>::ColType& SrcA2 = m1[2];

        const typename TMatrix3x3<T>::ColType& SrcB0 = m2[0];
        const typename TMatrix3x3<T>::ColType& SrcB1 = m2[1];
        const typename TMatrix3x3<T>::ColType& SrcB2 = m2[2];

        TMatrix3x3<T> Result;
        // note: the following lines are decomposed to have consistent results between simd and non simd code (prevent rounding error because of operation order)
        //Result[0] = SrcA2 * SrcB1.z + SrcA1 * SrcB1.y + SrcA0 * SrcB1.x;
        //Result[1] = SrcA2 * SrcB1.z + SrcA1 * SrcB1.y + SrcA0 * SrcB1.x;
        //Result[2] = SrcA2 * SrcB2.z + SrcA1 * SrcB2.y + SrcA0 * SrcB2.x;

        typename TMatrix3x3<T>::ColType tmp;
        tmp = SrcA0 * SrcB0.x;
        tmp += SrcA1 * SrcB0.y;
        tmp += SrcA2 * SrcB0.z;
        Result[0] = tmp;
        tmp = SrcA0 * SrcB1.x;
        tmp += SrcA1 * SrcB1.y;
        tmp += SrcA2 * SrcB1.z;
        Result[1] = tmp;
        tmp = SrcA0 * SrcB2.x;
        tmp += SrcA1 * SrcB2.y;
        tmp += SrcA2 * SrcB2.z;
        Result[2] = tmp;
        return Result;
    }
}
