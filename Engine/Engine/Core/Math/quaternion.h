#pragma once

#include "angle.h"
#include "matrix4.h"
#include "vector.h"

#include <cmath>

namespace GE::Math
{
    struct Quaternion final
    {
        float x = 0.0F;
        float y = 0.0F;
        float z = 0.0F;
        float w = 1.0F;

        constexpr bool operator==(const Quaternion&) const noexcept = default;
    };

    [[nodiscard]] inline bool Normalize(Quaternion& value) noexcept
    {
        const float length_squared = value.x * value.x + value.y * value.y
            + value.z * value.z + value.w * value.w;
        if (!std::isfinite(length_squared) || length_squared <= 0.0F)
        {
            return false;
        }
        const float inverse_length = 1.0F / std::sqrt(length_squared);
        value.x *= inverse_length;
        value.y *= inverse_length;
        value.z *= inverse_length;
        value.w *= inverse_length;
        return true;
    }

    [[nodiscard]] inline Quaternion QuaternionFromAxisAngle(
        Vec3 axis,
        Radians angle) noexcept
    {
        if (!Normalize(axis))
        {
            return {};
        }
        const float sine = std::sin(angle.Value() * 0.5F);
        const float cosine = std::cos(angle.Value() * 0.5F);
        return {axis[0] * sine, axis[1] * sine, axis[2] * sine, cosine};
    }

    [[nodiscard]] inline Mat4 RotationMatrix(Quaternion value) noexcept
    {
        if (!Normalize(value))
        {
            return {};
        }
        const float xx = value.x * value.x;
        const float yy = value.y * value.y;
        const float zz = value.z * value.z;
        const float xy = value.x * value.y;
        const float xz = value.x * value.z;
        const float yz = value.y * value.z;
        const float xw = value.x * value.w;
        const float yw = value.y * value.w;
        const float zw = value.z * value.w;
        return {
            1 - 2 * (yy + zz), 2 * (xy + zw), 2 * (xz - yw), 0,
            2 * (xy - zw), 1 - 2 * (xx + zz), 2 * (yz + xw), 0,
            2 * (xz + yw), 2 * (yz - xw), 1 - 2 * (xx + yy), 0,
            0, 0, 0, 1};
    }
}
