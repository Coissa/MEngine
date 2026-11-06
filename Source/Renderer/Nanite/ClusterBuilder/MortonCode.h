#pragma once

#include <Core/Math/vector.h>

#include <cstdint>

namespace GE::Render::Nanite
{
    // Calculates a 30-bit Morton code from a position normalized to [0, 1]^3.
    // Each component uses 10 bits. Values outside the unit cube are clamped.
    [[nodiscard]] std::uint32_t CalculateMortonCode(Math::Vec3 normalized_position) noexcept;
} // namespace GE::Render::Nanite
