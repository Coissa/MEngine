#pragma once

#include <Engine/Core/Diagnostics/Result.h>

#include <compare>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace GE::Asset
{
    class AssetKey final
    {
    public:
        AssetKey() = default;

        [[nodiscard]] const std::string& SourcePath() const noexcept
        {
            return m_source_path;
        }
        [[nodiscard]] const std::string& SubassetKey() const noexcept
        {
            return m_subasset_key;
        }
        [[nodiscard]] bool IsValid() const noexcept
        {
            return !m_source_path.empty();
        }

        bool operator==(const AssetKey&) const noexcept = default;
        auto operator<=>(const AssetKey&) const noexcept = default;

    private:
        AssetKey(std::string source_path, std::string subasset_key)
            : m_source_path(std::move(source_path)),
              m_subasset_key(std::move(subasset_key))
        {
        }

        std::string m_source_path;
        std::string m_subasset_key;
        friend Core::Result<AssetKey> MakeAssetKey(
            std::string_view, std::string_view);
    };

    [[nodiscard]] Core::Result<std::string> NormalizeAssetRelativePath(
        std::string_view path);
    [[nodiscard]] Core::Result<AssetKey> MakeAssetKey(
        std::string_view source_path,
        std::string_view subasset_key = {});
    [[nodiscard]] std::string MakeAssetIdName(const AssetKey& key);
}

namespace std
{
    template<>
    struct hash<GE::Asset::AssetKey>
    {
        [[nodiscard]] size_t operator()(
            const GE::Asset::AssetKey& key) const noexcept
        {
            const size_t first = hash<string>{}(key.SourcePath());
            const size_t second = hash<string>{}(key.SubassetKey());
            return first ^ (second + static_cast<size_t>(0x9e3779b9u)
                + (first << 6) + (first >> 2));
        }
    };
}
