#pragma once

#include <Engine/Core/Diagnostics/Result.h>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace GE::Asset
{
    class AssetId final
    {
    public:
        constexpr AssetId() noexcept = default;
        explicit constexpr AssetId(
            std::array<std::uint8_t, 16> bytes) noexcept
            : m_bytes(bytes)
        {
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            for (const std::uint8_t byte : m_bytes)
            {
                if (byte != 0) return true;
            }
            return false;
        }

        [[nodiscard]] constexpr const std::array<std::uint8_t, 16>& Bytes()
            const noexcept
        {
            return m_bytes;
        }

        constexpr bool operator==(const AssetId&) const noexcept = default;
        constexpr auto operator<=>(const AssetId&) const noexcept = default;

    private:
        std::array<std::uint8_t, 16> m_bytes{};

        friend Core::Result<AssetId> ParseAssetId(std::string_view text);
        friend AssetId MakeAssetIdV5(
            AssetId namespace_id, std::string_view name);
    };

    [[nodiscard]] Core::Result<AssetId> ParseAssetId(std::string_view text);
    [[nodiscard]] std::string ToString(AssetId id);
    [[nodiscard]] AssetId MakeAssetIdV5(
        AssetId namespace_id, std::string_view name);
}

namespace std
{
    template<>
    struct hash<GE::Asset::AssetId>
    {
        [[nodiscard]] size_t operator()(
            const GE::Asset::AssetId& id) const noexcept
        {
            const auto& bytes = id.Bytes();
            std::uint64_t high = 0;
            std::uint64_t low = 0;
            for (std::size_t index = 0; index < 8; ++index)
            {
                high = (high << 8) | bytes[index];
                low = (low << 8) | bytes[index + 8];
            }
            return static_cast<size_t>(high ^ low);
        }
    };
}
