#pragma once

#include <cstdint>

namespace GE::RenderCore
{
    // 跨 Runtime、RenderCore 与 Renderer 传递的稳定值标识；不拥有任何 CPU/GPU 对象。
    struct MeshHandle final
    {
        std::uint32_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] constexpr bool operator==(
            const MeshHandle&) const noexcept = default;
    };

    struct TextureHandle final
    {
        std::uint32_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] constexpr bool operator==(
            const TextureHandle&) const noexcept = default;
    };

    struct MaterialHandle final
    {
        std::uint32_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] constexpr bool operator==(
            const MaterialHandle&) const noexcept = default;
    };
}
