#pragma once

#include <cmath>

#include "Vector3.h"
#include "Vector4.h"
#include "Quaternion.h"

namespace OiEngine::Math {
    template<typename T>
    constexpr TVector3<T> operator*(const TQuaternion<T>& q, const TVector3<T>& v)
    {
        const TVector3<T> QuatVector(q.x, q.y, q.z);
        const TVector3<T> uv(Cross(QuatVector, v));
        const TVector3<T> uuv(Cross(QuatVector, uv));

        return v + ((uv * q.w) + uuv) * static_cast<T>(2);
    }

    template<typename T>
    constexpr TVector4<T> operator*(const TQuaternion<T>& q, const TVector4<T>& v)
    {
        TVector3<T> ret = q * TVector3<T>(v.x, v.y, v.z);
        return TVector4<T>{ret.x, ret.y, ret.z, v.w};
    }
}
