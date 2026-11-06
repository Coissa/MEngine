#include "CookedContainer.h"

#include <Core/Serialization/BinaryIO.h>

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>
#include <string>

namespace GE::Cook
{
    namespace
    {
        constexpr std::array<std::byte, 8> Magic = {std::byte{'M'}, std::byte{'E'}, std::byte{'C'}, std::byte{'O'},
                                                    std::byte{'O'}, std::byte{'K'}, std::byte{'1'}, std::byte{0}};
        constexpr std::size_t HeaderSize = 84;
        constexpr std::size_t SectionEntrySize = 24;
        constexpr std::size_t HashOffset = 52;
        constexpr std::size_t MaximumSectionCount = 1'000'000;

        [[nodiscard]] Core::EngineError ContainerError(Core::ErrorCode code, std::string operation, std::string message)
        {
            return {.domain = Core::ErrorDomain::Asset,
                    .code = code,
                    .operation = std::move(operation),
                    .message = std::move(message)};
        }

        [[nodiscard]] constexpr std::uint32_t RotateRight(std::uint32_t value, unsigned int count) noexcept
        {
            return (value >> count) | (value << (32 - count));
        }

        [[nodiscard]] ContentHash ComputeSha256Impl(std::span<const std::byte> input)
        {
            constexpr std::array<std::uint32_t, 64> constants = {
                0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
                0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
                0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
                0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
                0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
                0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
                0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
                0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
            std::array<std::uint32_t, 8> state = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                                  0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

            FileSystem::ByteBuffer padded(input.begin(), input.end());
            padded.push_back(std::byte{0x80});
            while ((padded.size() + 8) % 64 != 0)
                padded.push_back(std::byte{0});
            const std::uint64_t bit_count = static_cast<std::uint64_t>(input.size()) * 8;
            for (int shift = 56; shift >= 0; shift -= 8)
                padded.push_back(static_cast<std::byte>((bit_count >> shift) & 0xff));

            for (std::size_t block = 0; block < padded.size(); block += 64)
            {
                std::array<std::uint32_t, 64> words{};
                for (std::size_t index = 0; index < 16; ++index)
                {
                    const std::size_t at = block + index * 4;
                    words[index] = (std::to_integer<std::uint32_t>(padded[at]) << 24) |
                                   (std::to_integer<std::uint32_t>(padded[at + 1]) << 16) |
                                   (std::to_integer<std::uint32_t>(padded[at + 2]) << 8) |
                                   std::to_integer<std::uint32_t>(padded[at + 3]);
                }
                for (std::size_t index = 16; index < 64; ++index)
                {
                    const std::uint32_t x = words[index - 15];
                    const std::uint32_t y = words[index - 2];
                    const std::uint32_t sigma0 = RotateRight(x, 7) ^ RotateRight(x, 18) ^ (x >> 3);
                    const std::uint32_t sigma1 = RotateRight(y, 17) ^ RotateRight(y, 19) ^ (y >> 10);
                    words[index] = words[index - 16] + sigma0 + words[index - 7] + sigma1;
                }

                auto [a, b, c, d, e, f, g, h] = state;
                for (std::size_t index = 0; index < 64; ++index)
                {
                    const std::uint32_t sum1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
                    const std::uint32_t choose = (e & f) ^ (~e & g);
                    const std::uint32_t temp1 = h + sum1 + choose + constants[index] + words[index];
                    const std::uint32_t sum0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
                    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                    const std::uint32_t temp2 = sum0 + majority;
                    h = g;
                    g = f;
                    f = e;
                    e = d + temp1;
                    d = c;
                    c = b;
                    b = a;
                    a = temp1 + temp2;
                }
                state[0] += a;
                state[1] += b;
                state[2] += c;
                state[3] += d;
                state[4] += e;
                state[5] += f;
                state[6] += g;
                state[7] += h;
            }

            ContentHash result{};
            for (std::size_t word = 0; word < state.size(); ++word)
            {
                for (std::size_t byte = 0; byte < 4; ++byte)
                {
                    result[word * 4 + byte] = static_cast<std::byte>((state[word] >> (24 - byte * 8)) & 0xff);
                }
            }
            return result;
        }

        [[nodiscard]] ContentHash HashContainerBytes(std::span<const std::byte> bytes)
        {
            FileSystem::ByteBuffer hash_input(bytes.begin(), bytes.end());
            std::fill_n(hash_input.begin() + HashOffset, 32, std::byte{0});
            return ComputeSha256Impl(hash_input);
        }
    } // namespace

    ContentHash ComputeSha256(std::span<const std::byte> bytes)
    {
        return ComputeSha256Impl(bytes);
    }

    std::span<const std::byte> CookedContainer::SectionData(std::size_t section_index) const noexcept
    {
        if (section_index >= m_sections.size())
            return {};
        const CookedSection& section = m_sections[section_index];
        return std::span<const std::byte>(m_bytes).subspan(static_cast<std::size_t>(section.offset),
                                                           static_cast<std::size_t>(section.size));
    }

    Core::Result<CookedContainer> ParseCookedContainer(std::span<const std::byte> bytes)
    {
        const auto failure = [](Core::ErrorCode code, std::string message)
        {
            return Core::Result<CookedContainer>::Failure(
                ContainerError(code, "ParseCookedContainer", std::move(message)));
        };
        if (bytes.size() < HeaderSize)
            return failure(Core::ErrorCode::ParseFailure, "Cooked container header is truncated");
        if (!std::equal(Magic.begin(), Magic.end(), bytes.begin()))
            return failure(Core::ErrorCode::ParseFailure, "Cooked container magic is invalid");
        if (Serialization::ReadUnsignedAt<std::uint32_t>(bytes, 8) != CookedContainerVersion)
        {
            return failure(Core::ErrorCode::UnsupportedVersion, "Cooked container version is unsupported");
        }

        const std::uint64_t asset_type_value = Serialization::ReadUnsignedAt<std::uint64_t>(bytes, 12);
        const std::uint32_t payload_version = Serialization::ReadUnsignedAt<std::uint32_t>(bytes, 20);
        if (asset_type_value == 0 || payload_version == 0)
            return failure(Core::ErrorCode::ParseFailure, "Cooked asset type or payload version is invalid");
        if (std::to_integer<std::uint8_t>(bytes[24]) != CookedLittleEndian)
            return failure(Core::ErrorCode::UnsupportedVersion, "Cooked container byte order is unsupported");
        if (bytes[25] != std::byte{0} || bytes[26] != std::byte{0} || bytes[27] != std::byte{0} ||
            Serialization::ReadUnsignedAt<std::uint32_t>(bytes, 40) != 0)
        {
            return failure(Core::ErrorCode::ParseFailure, "Cooked container reserved fields are not zero");
        }

        const std::uint64_t total_length = Serialization::ReadUnsignedAt<std::uint64_t>(bytes, 28);
        const std::uint32_t section_count = Serialization::ReadUnsignedAt<std::uint32_t>(bytes, 36);
        const std::uint64_t table_offset = Serialization::ReadUnsignedAt<std::uint64_t>(bytes, 44);
        if (total_length != bytes.size())
            return failure(Core::ErrorCode::ParseFailure, "Cooked container length does not match the file");
        if (section_count > MaximumSectionCount || table_offset != HeaderSize)
            return failure(Core::ErrorCode::ParseFailure, "Cooked container section table is invalid");
        if (section_count > (std::numeric_limits<std::uint64_t>::max() - table_offset) / SectionEntrySize)
        {
            return failure(Core::ErrorCode::ParseFailure, "Cooked container section table overflows");
        }
        const std::uint64_t table_end = table_offset + section_count * SectionEntrySize;
        if (table_end > total_length)
            return failure(Core::ErrorCode::ParseFailure, "Cooked container section table is truncated");

        std::vector<CookedSection> sections;
        sections.reserve(section_count);
        for (std::size_t index = 0; index < section_count; ++index)
        {
            const std::size_t at = HeaderSize + index * SectionEntrySize;
            CookedSection section{.type = Serialization::ReadUnsignedAt<std::uint32_t>(bytes, at),
                                  .flags = Serialization::ReadUnsignedAt<std::uint32_t>(bytes, at + 4),
                                  .offset = Serialization::ReadUnsignedAt<std::uint64_t>(bytes, at + 8),
                                  .size = Serialization::ReadUnsignedAt<std::uint64_t>(bytes, at + 16)};
            if (section.type == 0 || section.offset < table_end || section.offset > total_length ||
                section.size > total_length - section.offset)
            {
                return failure(Core::ErrorCode::ParseFailure, "Cooked container section is out of bounds");
            }
            sections.push_back(section);
        }

        std::vector<std::size_t> order(sections.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](std::size_t left, std::size_t right) { return sections[left].offset < sections[right].offset; });
        std::uint64_t previous_end = table_end;
        for (const std::size_t index : order)
        {
            if (sections[index].offset < previous_end)
                return failure(Core::ErrorCode::ParseFailure, "Cooked container sections overlap");
            previous_end = sections[index].offset + sections[index].size;
        }

        ContentHash stored_hash{};
        std::copy_n(bytes.begin() + HashOffset, stored_hash.size(), stored_hash.begin());
        if (HashContainerBytes(bytes) != stored_hash)
            return failure(Core::ErrorCode::ParseFailure, "Cooked container content hash does not match");

        CookedContainer container;
        container.m_asset_type = Asset::AssetTypeId::FromValue(asset_type_value);
        container.m_payload_version = payload_version;
        container.m_hash = stored_hash;
        container.m_sections = std::move(sections);
        container.m_bytes.assign(bytes.begin(), bytes.end());
        return Core::Result<CookedContainer>::Success(std::move(container));
    }

    Core::Result<CookedContainer> ReadCookedContainer(const FileSystem::IReadOnlyFileMount& mount,
                                                      const FileSystem::LogicalPath& path)
    {
        auto bytes = mount.ReadAll(path);
        if (!bytes)
            return Core::Result<CookedContainer>::Failure(bytes.Error());
        auto parsed = ParseCookedContainer(bytes.Value());
        if (!parsed && !parsed.Error().resource.has_value())
            parsed.Error().resource = path.String();
        return parsed;
    }

} // namespace GE::Cook
