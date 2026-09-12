#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace GE::Math
{
    struct Vec2 final
    {
        std::array<float, 2> values{};
        constexpr Vec2() noexcept = default;
        constexpr Vec2(float x, float y) noexcept : values{x, y} {}
        [[nodiscard]] constexpr float& operator[](std::size_t index) noexcept { return values[index]; }
        [[nodiscard]] constexpr const float& operator[](std::size_t index) const noexcept { return values[index]; }
        [[nodiscard]] constexpr float* data() noexcept { return values.data(); }
        [[nodiscard]] constexpr const float* data() const noexcept { return values.data(); }
        constexpr bool operator==(const Vec2&) const noexcept = default;
    };

    struct Vec3 final
    {
        std::array<float, 3> values{};
        constexpr Vec3() noexcept = default;
        constexpr Vec3(float x, float y, float z) noexcept : values{x, y, z} {}
        [[nodiscard]] constexpr float& operator[](std::size_t index) noexcept { return values[index]; }
        [[nodiscard]] constexpr const float& operator[](std::size_t index) const noexcept { return values[index]; }
        [[nodiscard]] constexpr float* data() noexcept { return values.data(); }
        [[nodiscard]] constexpr const float* data() const noexcept { return values.data(); }
        constexpr bool operator==(const Vec3&) const noexcept = default;
    };

    struct Vec4 final
    {
        std::array<float, 4> values{};
        constexpr Vec4() noexcept = default;
        constexpr Vec4(float x, float y, float z, float w) noexcept : values{x, y, z, w} {}
        [[nodiscard]] constexpr float& operator[](std::size_t index) noexcept { return values[index]; }
        [[nodiscard]] constexpr const float& operator[](std::size_t index) const noexcept { return values[index]; }
        [[nodiscard]] constexpr float* data() noexcept { return values.data(); }
        [[nodiscard]] constexpr const float* data() const noexcept { return values.data(); }
        constexpr bool operator==(const Vec4&) const noexcept = default;
    };

    [[nodiscard]] constexpr Vec3 operator+(Vec3 left, Vec3 right) noexcept
    {
        return {left[0] + right[0], left[1] + right[1], left[2] + right[2]};
    }

    [[nodiscard]] constexpr Vec3 operator-(Vec3 left, Vec3 right) noexcept
    {
        return {left[0] - right[0], left[1] - right[1], left[2] - right[2]};
    }

    [[nodiscard]] constexpr Vec3 operator*(Vec3 value, float scalar) noexcept
    {
        return {value[0] * scalar, value[1] * scalar, value[2] * scalar};
    }

    [[nodiscard]] constexpr Vec3 operator/(Vec3 value, float scalar) noexcept
    {
        return {value[0] / scalar, value[1] / scalar, value[2] / scalar};
    }

    constexpr Vec3& operator+=(Vec3& left, Vec3 right) noexcept
    {
        left = left + right;
        return left;
    }

    [[nodiscard]] constexpr float Dot(Vec3 left, Vec3 right) noexcept
    {
        return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
    }

    [[nodiscard]] constexpr Vec3 Cross(Vec3 left, Vec3 right) noexcept
    {
        return {
            left[1] * right[2] - left[2] * right[1],
            left[2] * right[0] - left[0] * right[2],
            left[0] * right[1] - left[1] * right[0]};
    }

    [[nodiscard]] constexpr float LengthSquared(Vec3 value) noexcept { return Dot(value, value); }
    [[nodiscard]] inline float Length(Vec3 value) noexcept { return std::sqrt(LengthSquared(value)); }

    [[nodiscard]] inline bool Normalize(Vec3& value) noexcept
    {
        const float length_squared = LengthSquared(value);
        if (!std::isfinite(length_squared) || length_squared <= 0.0F)
        {
            return false;
        }
        value = value * (1.0F / std::sqrt(length_squared));
        return true;
    }

    [[nodiscard]] inline bool IsFinite(Vec2 value) noexcept
    {
        return std::isfinite(value[0]) && std::isfinite(value[1]);
    }

    [[nodiscard]] inline bool IsFinite(Vec3 value) noexcept
    {
        return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
    }

    [[nodiscard]] inline bool IsFinite(Vec4 value) noexcept
    {
        return std::isfinite(value[0]) && std::isfinite(value[1])
            && std::isfinite(value[2]) && std::isfinite(value[3]);
    }
}
