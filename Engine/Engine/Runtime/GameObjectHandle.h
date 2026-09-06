#pragma once

#include <cstdint>

namespace GE::Runtime
{
    struct GameObjectId final
    {
        std::uint64_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return value != 0;
        }

        [[nodiscard]] constexpr bool operator==(
            const GameObjectId&) const noexcept = default;
    };

    // Handle 只在所属 World 内有效。generation 阻止槽位复用后误用旧引用。
    struct GameObjectHandle final
    {
        std::uint32_t index = 0;
        std::uint32_t generation = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return generation != 0;
        }

        [[nodiscard]] constexpr bool operator==(
            const GameObjectHandle&) const noexcept = default;
    };
}
