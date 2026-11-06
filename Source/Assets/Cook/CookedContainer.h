#pragma once

#include <Assets/Registry/AssetTypeId.h>
#include <Core/Diagnostics/Result.h>
#include <Core/FileSystem/FileMount.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace GE::Cook
{
    inline constexpr std::uint32_t CookedContainerVersion = 1;
    inline constexpr std::uint8_t CookedLittleEndian = 1;

    using ContentHash = std::array<std::byte, 32>;

    [[nodiscard]] ContentHash ComputeSha256(std::span<const std::byte> bytes);

    struct CookedSection final
    {
        std::uint32_t type = 0;
        std::uint32_t flags = 0;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    class CookedContainer final
    {
    public:
        [[nodiscard]] Asset::AssetTypeId AssetType() const noexcept { return m_asset_type; }
        [[nodiscard]] std::uint32_t PayloadVersion() const noexcept { return m_payload_version; }
        [[nodiscard]] const ContentHash& Hash() const noexcept { return m_hash; }
        [[nodiscard]] std::span<const CookedSection> Sections() const noexcept { return m_sections; }
        [[nodiscard]] std::span<const std::byte> SectionData(std::size_t section_index) const noexcept;

    private:
        Asset::AssetTypeId m_asset_type;
        std::uint32_t m_payload_version = 0;
        ContentHash m_hash{};
        std::vector<CookedSection> m_sections;
        FileSystem::ByteBuffer m_bytes;

        friend Core::Result<CookedContainer> ParseCookedContainer(std::span<const std::byte> bytes);
    };

    [[nodiscard]] Core::Result<CookedContainer> ParseCookedContainer(std::span<const std::byte> bytes);

    [[nodiscard]] Core::Result<CookedContainer> ReadCookedContainer(const FileSystem::IReadOnlyFileMount& mount,
                                                                    const FileSystem::LogicalPath& path);

} // namespace GE::Cook
