#include "MortonCode.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace GE::Render::Nanite
{
    namespace
    {
        constexpr std::uint32_t MortonAxisBits = 10;
        constexpr std::uint32_t MortonAxisScale = 1U << MortonAxisBits;
        constexpr std::uint32_t MortonAxisMaximum = MortonAxisScale - 1U;

        [[nodiscard]] std::uint32_t QuantizeNormalizedCoordinate(float coordinate) noexcept
        {
            const float clamped = std::clamp(coordinate, 0.0F, 1.0F);
            return (std::min)(static_cast<std::uint32_t>(clamped * static_cast<float>(MortonAxisScale)),
                              MortonAxisMaximum);
        }

        [[nodiscard]] constexpr std::uint32_t SpreadBitsByThree(std::uint32_t value) noexcept
        {
            value &= 0x000003FFU;
            value = (value | (value << 16U)) & 0x030000FFU;
            value = (value | (value << 8U)) & 0x0300F00FU;
            value = (value | (value << 4U)) & 0x030C30C3U;
            value = (value | (value << 2U)) & 0x09249249U;
            return value;
        }
    } // namespace

    std::uint32_t CalculateMortonCode(Math::Vec3 normalized_position) noexcept
    {
        if (!Math::IsFinite(normalized_position))
        {
            return 0;
        }

        const std::uint32_t x = QuantizeNormalizedCoordinate(normalized_position[0]);
        const std::uint32_t y = QuantizeNormalizedCoordinate(normalized_position[1]);
        const std::uint32_t z = QuantizeNormalizedCoordinate(normalized_position[2]);

        return SpreadBitsByThree(x) | (SpreadBitsByThree(y) << 1U) | (SpreadBitsByThree(z) << 2U);
    }
} // namespace GE::Render::Nanite
