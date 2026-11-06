#include "CookedContainerPublisher.h"

#include <Core/FileSystem/AtomicFilePublisher.h>
#include <Core/Serialization/BinaryIO.h>

#include <algorithm>
#include <array>
#include <limits>

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

        [[nodiscard]] Core::EngineError BuildError(std::string message)
        {
            return {.domain = Core::ErrorDomain::Asset,
                    .code = Core::ErrorCode::InvalidArgument,
                    .operation = "BuildCookedContainer",
                    .message = std::move(message)};
        }

    } // namespace

    Core::Result<FileSystem::ByteBuffer> BuildCookedContainer(const CookedContainerWriteRequest& request)
    {
        if (!request.asset_type.IsValid() || request.payload_version == 0)
        {
            return Core::Result<FileSystem::ByteBuffer>::Failure(
                BuildError("Asset type and payload version must be valid"));
        }
        if (request.sections.size() > MaximumSectionCount)
        {
            return Core::Result<FileSystem::ByteBuffer>::Failure(BuildError("Too many sections"));
        }

        std::uint64_t total_length = HeaderSize;
        if (request.sections.size() > (std::numeric_limits<std::uint64_t>::max() - total_length) / SectionEntrySize)
        {
            return Core::Result<FileSystem::ByteBuffer>::Failure(BuildError("Section table size overflows"));
        }
        total_length += request.sections.size() * SectionEntrySize;
        for (const CookedSectionSource& section : request.sections)
        {
            if (section.type == 0 || section.bytes.size() > std::numeric_limits<std::uint64_t>::max() - total_length)
            {
                return Core::Result<FileSystem::ByteBuffer>::Failure(
                    BuildError("Section type is invalid or total size overflows"));
            }
            total_length += section.bytes.size();
        }
        if (total_length > std::numeric_limits<std::size_t>::max())
        {
            return Core::Result<FileSystem::ByteBuffer>::Failure(BuildError("Container is too large for this process"));
        }

        FileSystem::ByteBuffer output;
        output.reserve(static_cast<std::size_t>(total_length));
        output.insert(output.end(), Magic.begin(), Magic.end());
        Serialization::WriteUnsigned(output, CookedContainerVersion);
        Serialization::WriteUnsigned(output, request.asset_type.Value());
        Serialization::WriteUnsigned(output, request.payload_version);
        output.push_back(static_cast<std::byte>(CookedLittleEndian));
        output.insert(output.end(), 3, std::byte{0});
        Serialization::WriteUnsigned(output, total_length);
        Serialization::WriteUnsigned(output, static_cast<std::uint32_t>(request.sections.size()));
        Serialization::WriteUnsigned(output, std::uint32_t{0});
        Serialization::WriteUnsigned(output, static_cast<std::uint64_t>(HeaderSize));
        output.insert(output.end(), 32, std::byte{0});

        std::uint64_t payload_offset = HeaderSize + request.sections.size() * SectionEntrySize;
        for (const CookedSectionSource& section : request.sections)
        {
            Serialization::WriteUnsigned(output, section.type);
            Serialization::WriteUnsigned(output, section.flags);
            Serialization::WriteUnsigned(output, payload_offset);
            Serialization::WriteUnsigned(output, static_cast<std::uint64_t>(section.bytes.size()));
            payload_offset += section.bytes.size();
        }
        for (const CookedSectionSource& section : request.sections)
            Serialization::WriteBytes(output, section.bytes);

        const ContentHash hash = ComputeSha256(output);
        std::copy(hash.begin(), hash.end(), output.begin() + HashOffset);
        return Core::Result<FileSystem::ByteBuffer>::Success(std::move(output));
    }

    Core::Result<void> PublishCookedContainer(const CookedContainerWriteRequest& request,
                                              const std::filesystem::path& target_path)
    {
        auto bytes = BuildCookedContainer(request);
        if (!bytes)
            return Core::Result<void>::Failure(bytes.Error());
        return FileSystem::PublishFileAtomically(target_path, bytes.Value());
    }
} // namespace GE::Cook
