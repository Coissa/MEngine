#pragma once

#include <compare>

namespace GE::Math
{
    inline constexpr float Pi = 3.14159265358979323846F;
    inline constexpr float TwoPi = 2.0F * Pi;

    class Radians final
    {
    public:
        constexpr explicit Radians(float value = 0.0F) noexcept : m_value(value) {}
        [[nodiscard]] constexpr float Value() const noexcept { return m_value; }
        constexpr auto operator<=>(const Radians&) const noexcept = default;

    private:
        float m_value;
    };

    class Degrees final
    {
    public:
        constexpr explicit Degrees(float value = 0.0F) noexcept : m_value(value) {}
        [[nodiscard]] constexpr float Value() const noexcept { return m_value; }
        constexpr auto operator<=>(const Degrees&) const noexcept = default;

    private:
        float m_value;
    };

    [[nodiscard]] constexpr Radians ToRadians(Degrees degrees) noexcept
    {
        return Radians{degrees.Value() * Pi / 180.0F};
    }

    [[nodiscard]] constexpr Degrees ToDegrees(Radians radians) noexcept
    {
        return Degrees{radians.Value() * 180.0F / Pi};
    }

    [[nodiscard]] constexpr Radians operator+(Radians left, Radians right) noexcept
    {
        return Radians{left.Value() + right.Value()};
    }

    [[nodiscard]] constexpr Radians operator-(Radians left, Radians right) noexcept
    {
        return Radians{left.Value() - right.Value()};
    }

    [[nodiscard]] constexpr Radians operator*(Radians angle, float scalar) noexcept
    {
        return Radians{angle.Value() * scalar};
    }

    [[nodiscard]] constexpr Radians operator/(Radians angle, float scalar) noexcept
    {
        return Radians{angle.Value() / scalar};
    }
}
