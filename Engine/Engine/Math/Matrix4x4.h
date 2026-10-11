#pragma once

#include "Vector4.h"
#include "VectorGeometric.h"

namespace OiEngine::Math {
    // 注意：模板 struct 不加 OI_API——头文件内实例化即可；若标为 dllimport，
    // 静态成员（zero/identity）在 DLL 客户端会因未导出而链接失败
    template<typename T>
    struct TMatrix4x4 {
        using ColType = TVector4<T>;
        using RowType = TVector4<T>;

        constexpr explicit TMatrix4x4(T diagonal = 0);
        constexpr TMatrix4x4(const ColType& c0, const ColType& c1, const ColType& c2, const ColType& c3);
        constexpr explicit TMatrix4x4(T (&arr)[16]);
        constexpr TMatrix4x4(const TMatrix4x4& m);
        constexpr TMatrix4x4(TMatrix4x4&& m) noexcept;

        constexpr TMatrix4x4& operator=(const TMatrix4x4& m);
        constexpr TMatrix4x4& operator=(TMatrix4x4&& m) noexcept;

        static constexpr size_t Rows();
        static constexpr size_t Columns();
        constexpr ColType& operator[](size_t i);
        constexpr const ColType& operator[](size_t i) const;

        constexpr T *Data();
        constexpr const T *Data() const;

        static const TMatrix4x4 zero;
        static const TMatrix4x4 identity;

    private:
        ColType columns[4];
    };

    using Matrix4x4i = TMatrix4x4<int>;
    using Matrix4x4f = TMatrix4x4<float>;
    using Matrix4x4d = TMatrix4x4<double>;

    using Matrix4 = Matrix4x4f;

    template<typename T>
    const TMatrix4x4<T> TMatrix4x4<T>::zero = TMatrix4x4();
    template<typename T>
    const TMatrix4x4<T> TMatrix4x4<T>::identity = TMatrix4x4(1);

    template<typename T>
    constexpr TMatrix4x4<T>::TMatrix4x4(T diagonal):
        columns{
            ColType(diagonal),
            ColType(0, diagonal),
            ColType(0, 0, diagonal),
            ColType(0, 0, 0, diagonal)
        } {}
    template<typename T>
    constexpr TMatrix4x4<T>::TMatrix4x4(const ColType& c0, const ColType& c1, const ColType& c2, const ColType& c3):
        columns{ c0, c1, c2, c3 } {}
    template<typename T>
    constexpr TMatrix4x4<T>::TMatrix4x4(T (&arr)[16]):
        columns{
            ColType(arr[0], arr[1], arr[2], arr[3]),
            ColType(arr[4], arr[5], arr[6], arr[7]),
            ColType(arr[8], arr[9], arr[10], arr[11]),
            ColType(arr[12], arr[13], arr[14], arr[15])
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
    constexpr TMatrix4x4<T>::ColType& TMatrix4x4<T>::operator[](size_t i)
    {
        OI_ASSERT(0 <= i && i < Columns());
        return columns[i];
    }
    template<typename T>
    constexpr const TMatrix4x4<T>::ColType& TMatrix4x4<T>::operator[](size_t i) const
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

    // 注意：参数需写成可直接推导的类型（TVector4<T>），不能写 typename TMatrix4x4<T>::ColType，
    // 否则与泛型标量版本的 operator* 无法偏序比较，m * v 会产生二义
    template<typename T>
    constexpr TMatrix4x4<T>::ColType operator*(const TMatrix4x4<T>& m, const TVector4<T>& v)
    {
        const typename TMatrix4x4<T>::ColType Mov0(v[0], v[0], v[0], v[0]);
        const typename TMatrix4x4<T>::ColType Mov1(v[1], v[1], v[1], v[1]);
        const typename TMatrix4x4<T>::ColType Mul0 = m[0] * Mov0;
        const typename TMatrix4x4<T>::ColType Mul1 = m[1] * Mov1;
        const typename TMatrix4x4<T>::ColType Add0 = Mul0 + Mul1;
        const typename TMatrix4x4<T>::ColType Mov2(v[2], v[2], v[2], v[2]);
        const typename TMatrix4x4<T>::ColType Mov3(v[3], v[3], v[3], v[3]);
        const typename TMatrix4x4<T>::ColType Mul2 = m[2] * Mov2;
        const typename TMatrix4x4<T>::ColType Mul3 = m[3] * Mov3;
        const typename TMatrix4x4<T>::ColType Add1 = Mul2 + Mul3;
        const typename TMatrix4x4<T>::ColType Add2 = Add0 + Add1;
        return Add2;

        /*
            return typename TMatrix4x4<T>::ColType(
                m[0][0] * v[0] + m[1][0] * v[1] + m[2][0] * v[2] + m[3][0] * v[3],
                m[0][1] * v[0] + m[1][1] * v[1] + m[2][1] * v[2] + m[3][1] * v[3],
                m[0][2] * v[0] + m[1][2] * v[1] + m[2][2] * v[2] + m[3][2] * v[3],
                m[0][3] * v[0] + m[1][3] * v[1] + m[2][3] * v[2] + m[3][3] * v[3]);
        */
    }
    template<typename T>
    constexpr TMatrix4x4<T>::RowType operator*(const TVector4<T>& v, const TMatrix4x4<T>& m)
    {
        return typename TMatrix4x4<T>::RowType(
            Dot(m[0], v),
            Dot(m[1], v),
            Dot(m[2], v),
            Dot(m[3], v));
    }
    template<typename T>
    constexpr TMatrix4x4<T> operator*(const TMatrix4x4<T>& m1, const TMatrix4x4<T>& m2)
    {
        const typename TMatrix4x4<T>::ColType& SrcA0 = m1[0];
        const typename TMatrix4x4<T>::ColType& SrcA1 = m1[1];
        const typename TMatrix4x4<T>::ColType& SrcA2 = m1[2];
        const typename TMatrix4x4<T>::ColType& SrcA3 = m1[3];

        const typename TMatrix4x4<T>::ColType& SrcB0 = m2[0];
        const typename TMatrix4x4<T>::ColType& SrcB1 = m2[1];
        const typename TMatrix4x4<T>::ColType& SrcB2 = m2[2];
        const typename TMatrix4x4<T>::ColType& SrcB3 = m2[3];

        TMatrix4x4<T> Result;
        // note: the following lines are decomposed to have consistent results between simd and non simd code (prevent rounding error because of operation order)
        //Result[0] = SrcA3 * SrcB0.w + SrcA2 * SrcB0.z + SrcA1 * SrcB0.y + SrcA0 * SrcB0.x;
        //Result[1] = SrcA3 * SrcB1.w + SrcA2 * SrcB1.z + SrcA1 * SrcB1.y + SrcA0 * SrcB1.x;
        //Result[2] = SrcA3 * SrcB2.w + SrcA2 * SrcB2.z + SrcA1 * SrcB2.y + SrcA0 * SrcB2.x;
        //Result[3] = SrcA3 * SrcB3.w + SrcA2 * SrcB3.z + SrcA1 * SrcB3.y + SrcA0 * SrcB3.x;

        typename TMatrix4x4<T>::ColType tmp;
        tmp =  SrcA0 * SrcB0.x;
        tmp += SrcA1 * SrcB0.y;
        tmp += SrcA2 * SrcB0.z;
        tmp += SrcA3 * SrcB0.w;
        Result[0] = tmp;
        tmp =  SrcA0 * SrcB1.x;
        tmp += SrcA1 * SrcB1.y;
        tmp += SrcA2 * SrcB1.z;
        tmp += SrcA3 * SrcB1.w;
        Result[1] = tmp;
        tmp =  SrcA0 * SrcB2.x;
        tmp += SrcA1 * SrcB2.y;
        tmp += SrcA2 * SrcB2.z;
        tmp += SrcA3 * SrcB2.w;
        Result[2] = tmp;
        tmp =  SrcA0 * SrcB3.x;
        tmp += SrcA1 * SrcB3.y;
        tmp += SrcA2 * SrcB3.z;
        tmp += SrcA3 * SrcB3.w;
        Result[3] = tmp;

        return Result;
    }
}
