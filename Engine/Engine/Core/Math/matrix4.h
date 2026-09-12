#pragma once

#include "angle.h"
#include "vector.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace GE::Math
{
    struct Mat4 final
    {
        std::array<float, 16> values{
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1};

        constexpr Mat4() noexcept = default;
        constexpr Mat4(
            float m00, float m01, float m02, float m03,
            float m10, float m11, float m12, float m13,
            float m20, float m21, float m22, float m23,
            float m30, float m31, float m32, float m33) noexcept
            : values{m00, m01, m02, m03, m10, m11, m12, m13,
                m20, m21, m22, m23, m30, m31, m32, m33}
        {
        }

        [[nodiscard]] constexpr float& operator[](std::size_t index) noexcept { return values[index]; }
        [[nodiscard]] constexpr const float& operator[](std::size_t index) const noexcept { return values[index]; }
        [[nodiscard]] constexpr float& At(std::size_t row, std::size_t column) noexcept { return values[row * 4 + column]; }
        [[nodiscard]] constexpr const float& At(std::size_t row, std::size_t column) const noexcept { return values[row * 4 + column]; }
        [[nodiscard]] constexpr float* data() noexcept { return values.data(); }
        [[nodiscard]] constexpr const float* data() const noexcept { return values.data(); }
        constexpr bool operator==(const Mat4&) const noexcept = default;
    };

    [[nodiscard]] constexpr Mat4 ZeroMatrix() noexcept
    {
        return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    }

    [[nodiscard]] constexpr Mat4 operator*(const Mat4& left, const Mat4& right) noexcept
    {
        Mat4 result = ZeroMatrix();
        for (std::size_t row = 0; row < 4; ++row)
        {
            for (std::size_t column = 0; column < 4; ++column)
            {
                for (std::size_t inner = 0; inner < 4; ++inner)
                {
                    result.At(row, column) += left.At(row, inner) * right.At(inner, column);
                }
            }
        }
        return result;
    }

    [[nodiscard]] constexpr Vec4 Transform(Vec4 value, const Mat4& matrix) noexcept
    {
        return {
            value[0] * matrix.At(0, 0) + value[1] * matrix.At(1, 0) + value[2] * matrix.At(2, 0) + value[3] * matrix.At(3, 0),
            value[0] * matrix.At(0, 1) + value[1] * matrix.At(1, 1) + value[2] * matrix.At(2, 1) + value[3] * matrix.At(3, 1),
            value[0] * matrix.At(0, 2) + value[1] * matrix.At(1, 2) + value[2] * matrix.At(2, 2) + value[3] * matrix.At(3, 2),
            value[0] * matrix.At(0, 3) + value[1] * matrix.At(1, 3) + value[2] * matrix.At(2, 3) + value[3] * matrix.At(3, 3)};
    }

    [[nodiscard]] constexpr Vec3 TransformPoint(Vec3 value, const Mat4& matrix) noexcept
    {
        const Vec4 transformed = Transform({value[0], value[1], value[2], 1}, matrix);
        return {transformed[0], transformed[1], transformed[2]};
    }

    [[nodiscard]] constexpr Vec3 TransformVector(Vec3 value, const Mat4& matrix) noexcept
    {
        const Vec4 transformed = Transform({value[0], value[1], value[2], 0}, matrix);
        return {transformed[0], transformed[1], transformed[2]};
    }

    [[nodiscard]] constexpr Mat4 TranslationMatrix(Vec3 translation) noexcept
    {
        return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
            translation[0], translation[1], translation[2], 1};
    }

    [[nodiscard]] constexpr Mat4 ScaleMatrix(Vec3 scale) noexcept
    {
        return {scale[0], 0, 0, 0, 0, scale[1], 0, 0,
            0, 0, scale[2], 0, 0, 0, 0, 1};
    }

    [[nodiscard]] inline Mat4 RotationXMatrix(Radians angle) noexcept
    {
        const float c = std::cos(angle.Value());
        const float s = std::sin(angle.Value());
        return {1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1};
    }

    [[nodiscard]] inline Mat4 RotationYMatrix(Radians angle) noexcept
    {
        const float c = std::cos(angle.Value());
        const float s = std::sin(angle.Value());
        return {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1};
    }

    [[nodiscard]] inline Mat4 RotationZMatrix(Radians angle) noexcept
    {
        const float c = std::cos(angle.Value());
        const float s = std::sin(angle.Value());
        return {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    }

    [[nodiscard]] inline Mat4 EulerXYZMatrix(Vec3 rotation_degrees) noexcept
    {
        return RotationXMatrix(ToRadians(Degrees{rotation_degrees[0]}))
            * RotationYMatrix(ToRadians(Degrees{rotation_degrees[1]}))
            * RotationZMatrix(ToRadians(Degrees{rotation_degrees[2]}));
    }

    [[nodiscard]] constexpr Mat4 Transpose(const Mat4& matrix) noexcept
    {
        Mat4 result = ZeroMatrix();
        for (std::size_t row = 0; row < 4; ++row)
        {
            for (std::size_t column = 0; column < 4; ++column)
            {
                result.At(row, column) = matrix.At(column, row);
            }
        }
        return result;
    }

    [[nodiscard]] inline Mat4 PerspectiveFovLH_ZO(
        Radians vertical_field_of_view,
        float aspect,
        float near_plane,
        float far_plane) noexcept
    {
        const float y_scale = 1.0F / std::tan(vertical_field_of_view.Value() * 0.5F);
        const float x_scale = y_scale / aspect;
        const float depth_scale = far_plane / (far_plane - near_plane);
        return {x_scale, 0, 0, 0, 0, y_scale, 0, 0,
            0, 0, depth_scale, 1, 0, 0, -near_plane * depth_scale, 0};
    }
}
