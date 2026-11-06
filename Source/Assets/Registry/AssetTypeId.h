#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace GE::Asset
{
    class AssetTypeId final
    {
    public:
        constexpr AssetTypeId() noexcept = default;
        [[nodiscard]] static constexpr AssetTypeId FromValue(std::uint64_t value) noexcept
        {
            return AssetTypeId(value);
        }
        [[nodiscard]] constexpr bool IsValid() const noexcept { return m_value != 0; }
        [[nodiscard]] constexpr std::uint64_t Value() const noexcept { return m_value; }
        constexpr bool operator==(const AssetTypeId&) const noexcept = default;
        constexpr auto operator<=>(const AssetTypeId&) const noexcept = default;

    private:
        explicit constexpr AssetTypeId(std::uint64_t value) noexcept : m_value(value) {}
        std::uint64_t m_value = 0;
        friend constexpr AssetTypeId MakeAssetTypeId(std::string_view stable_name) noexcept;
    };

    [[nodiscard]] constexpr AssetTypeId MakeAssetTypeId(std::string_view stable_name) noexcept
    {
        if (stable_name.empty())
            return {};
        std::uint64_t hash = 14695981039346656037ull;
        for (const char byte : stable_name)
        {
            hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(byte));
            hash *= 1099511628211ull;
        }
        return AssetTypeId(hash == 0 ? 1 : hash);
    }
} // namespace GE::Asset

namespace std
{
    template <> struct hash<GE::Asset::AssetTypeId>
    {
        [[nodiscard]] size_t operator()(const GE::Asset::AssetTypeId& id) const noexcept
        {
            return static_cast<size_t>(id.Value());
        }
    };
} // namespace std
