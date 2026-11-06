#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace GE::Resource
{
    class ResourceManager;

    template <typename Tag> class ResourceHandle final
    {
    public:
        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return m_index != std::numeric_limits<std::uint32_t>::max() && m_generation != 0;
        }
        [[nodiscard]] constexpr std::uint32_t Index() const noexcept { return m_index; }
        [[nodiscard]] constexpr std::uint32_t Generation() const noexcept { return m_generation; }

        constexpr bool operator==(const ResourceHandle&) const noexcept = default;
        constexpr auto operator<=>(const ResourceHandle&) const noexcept = default;

    private:
        constexpr ResourceHandle(std::uint32_t index, std::uint32_t generation) noexcept
            : m_index(index), m_generation(generation)
        {
        }

        std::uint32_t m_index = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t m_generation = 0;

        friend class ResourceManager;
    };

    struct StaticMeshResourceTag final
    {
    };
    struct TextureResourceTag final
    {
    };
    struct MaterialResourceTag final
    {
    };

    using StaticMeshHandle = ResourceHandle<StaticMeshResourceTag>;
    using TextureHandle = ResourceHandle<TextureResourceTag>;
    using MaterialHandle = ResourceHandle<MaterialResourceTag>;
} // namespace GE::Resource
