#include <Assets/Registry/AssetTypeId.h>
#include <Assets/Cook/CookedContainer.h>
#include <Assets/Cook/CookedContainerPublisher.h>
#include <Core/FileSystem/AtomicFilePublisher.h>
#include <Core/FileSystem/FileMount.h>
#include <Core/FileSystem/LogicalPath.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace
{
    using GE::FileSystem::ByteBuffer;

    [[nodiscard]] ByteBuffer Bytes(std::string_view text)
    {
        ByteBuffer result(text.size());
        std::transform(text.begin(), text.end(), result.begin(),
                       [](char value) { return static_cast<std::byte>(static_cast<unsigned char>(value)); });
        return result;
    }

    [[nodiscard]] std::string Text(std::span<const std::byte> bytes)
    {
        std::string result(bytes.size(), '\0');
        std::transform(bytes.begin(), bytes.end(), result.begin(),
                       [](std::byte value) { return static_cast<char>(std::to_integer<unsigned char>(value)); });
        return result;
    }

    [[nodiscard]] GE::FileSystem::LogicalPath Path(std::string_view text)
    {
        auto result = GE::FileSystem::NormalizeLogicalPath(text);
        return result ? std::move(result).Value() : GE::FileSystem::LogicalPath{};
    }

    [[nodiscard]] bool CheckMountContract(const GE::FileSystem::IReadOnlyFileMount& mount)
    {
        const auto path = Path("Data/contract.bin");
        const auto missing = Path("Data/missing.bin");
        auto size = mount.FileSize(path);
        auto all = mount.ReadAll(path);
        auto middle = mount.ReadRange(path, 2, 3);
        auto empty = mount.ReadRange(path, 6, 0);
        return size && size.Value() == 6 && all && Text(all.Value()) == "abcdef" && middle &&
               Text(middle.Value()) == "cde" && empty && empty.Value().empty() && !mount.ReadRange(path, 5, 2) &&
               !mount.ReadRange(path, UINT64_MAX, 1) && !mount.FileSize(missing) && !mount.ReadAll(missing);
    }

    [[nodiscard]] bool TestLogicalPathsAndMounts(const std::filesystem::path& directory)
    {
        auto normalized = GE::FileSystem::NormalizeLogicalPath("Data\\./Folder//file.bin");
        if (!normalized || normalized.Value().String() != "Data/Folder/file.bin" ||
            GE::FileSystem::NormalizeLogicalPath("") || GE::FileSystem::NormalizeLogicalPath("../outside") ||
            GE::FileSystem::NormalizeLogicalPath("C:/absolute") || GE::FileSystem::NormalizeLogicalPath("/rooted"))
        {
            return false;
        }

        GE::FileSystem::MemoryFileMount memory;
        if (!memory.AddFile(Path("Data/contract.bin"), Bytes("abcdef")) ||
            memory.AddFile(Path("Data/contract.bin"), Bytes("duplicate")) || !CheckMountContract(memory))
        {
            return false;
        }

        std::filesystem::create_directories(directory / "Data");
        {
            std::ofstream output(directory / "Data" / "contract.bin", std::ios::binary);
            output << "abcdef";
        }
        auto loose = GE::FileSystem::LooseFileMount::Open(directory);
        return loose && CheckMountContract(loose.Value());
    }

    class AlwaysFailReplacer final : public GE::FileSystem::IAtomicFileReplacer
    {
    public:
        [[nodiscard]] GE::Core::Result<void> Replace(const std::filesystem::path&,
                                                     const std::filesystem::path& target_path) const override
        {
            return GE::Core::Result<void>::Failure({.domain = GE::Core::ErrorDomain::Filesystem,
                                                    .code = GE::Core::ErrorCode::IoFailure,
                                                    .operation = "InjectedReplaceFailure",
                                                    .message = "Injected replacement failure",
                                                    .resource = target_path.generic_string()});
        }
    };

    [[nodiscard]] std::string ReadTextFile(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    }

    [[nodiscard]] bool HasTemporarySibling(const std::filesystem::path& directory)
    {
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.path().extension() == ".tmp")
                return true;
        }
        return false;
    }

    [[nodiscard]] bool TestAtomicPublication(const std::filesystem::path& directory)
    {
        const auto target = directory / "published.bin";
        {
            std::ofstream output(target, std::ios::binary);
            output << "old";
        }
        const AlwaysFailReplacer failure;
        const ByteBuffer replacement = Bytes("replacement");
        if (GE::FileSystem::PublishFileAtomically(target, replacement, failure) || ReadTextFile(target) != "old" ||
            HasTemporarySibling(directory))
        {
            return false;
        }
        return GE::FileSystem::PublishFileAtomically(target, replacement) && ReadTextFile(target) == "replacement" &&
               !HasTemporarySibling(directory);
    }

    void WriteU32(ByteBuffer& bytes, std::size_t offset, std::uint32_t value)
    {
        for (std::size_t index = 0; index < 4; ++index)
        {
            bytes[offset + index] = static_cast<std::byte>(value & 0xff);
            value >>= 8;
        }
    }

    void WriteU64(ByteBuffer& bytes, std::size_t offset, std::uint64_t value)
    {
        for (std::size_t index = 0; index < 8; ++index)
        {
            bytes[offset + index] = static_cast<std::byte>(value & 0xff);
            value >>= 8;
        }
    }

    [[nodiscard]] bool Rejects(ByteBuffer bytes)
    {
        return !GE::Cook::ParseCookedContainer(bytes);
    }

    [[nodiscard]] bool TestCookedContainer(const std::filesystem::path& directory)
    {
        constexpr unsigned char expected_abc_sha256[] = {
            0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
            0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
        const auto abc_hash = GE::Cook::ComputeSha256(Bytes("abc"));
        for (std::size_t index = 0; index < abc_hash.size(); ++index)
        {
            if (abc_hash[index] != static_cast<std::byte>(expected_abc_sha256[index]))
            {
                return false;
            }
        }

        const ByteBuffer vertices = Bytes("vertex-payload");
        const ByteBuffer indices = Bytes("index-payload");
        const GE::Cook::CookedSectionSource sections[] = {{.type = 1, .flags = 0, .bytes = vertices},
                                                          {.type = 2, .flags = 7, .bytes = indices}};
        const GE::Cook::CookedContainerWriteRequest request{
            .asset_type = GE::Asset::MakeAssetTypeId("MEngine.Asset.Mesh"), .payload_version = 3, .sections = sections};

        auto first = GE::Cook::BuildCookedContainer(request);
        auto second = GE::Cook::BuildCookedContainer(request);
        if (!first || !second || first.Value() != second.Value())
            return false;

        auto parsed = GE::Cook::ParseCookedContainer(first.Value());
        if (!parsed || parsed.Value().AssetType() != request.asset_type || parsed.Value().PayloadVersion() != 3 ||
            parsed.Value().Sections().size() != 2 || Text(parsed.Value().SectionData(0)) != "vertex-payload" ||
            Text(parsed.Value().SectionData(1)) != "index-payload" || !parsed.Value().SectionData(99).empty())
        {
            return false;
        }

        ByteBuffer corrupt = first.Value();
        corrupt[0] = std::byte{0};
        if (!Rejects(corrupt))
            return false;
        corrupt = first.Value();
        WriteU32(corrupt, 8, GE::Cook::CookedContainerVersion + 1);
        if (!Rejects(corrupt))
            return false;
        corrupt = first.Value();
        corrupt.resize(40);
        if (!Rejects(corrupt))
            return false;
        corrupt = first.Value();
        WriteU64(corrupt, 84 + 8, UINT64_MAX);
        WriteU64(corrupt, 84 + 16, 2);
        if (!Rejects(corrupt))
            return false;
        corrupt = first.Value();
        const std::uint64_t first_payload_offset = 84 + 2 * 24;
        WriteU64(corrupt, 84 + 24 + 8, first_payload_offset + 1);
        if (!Rejects(corrupt))
            return false;
        corrupt = first.Value();
        corrupt.back() ^= std::byte{1};
        if (!Rejects(corrupt))
            return false;

        GE::FileSystem::MemoryFileMount memory;
        if (!memory.AddFile(Path("Cooked/mesh.masset"), first.Value()))
            return false;
        auto memory_read = GE::Cook::ReadCookedContainer(memory, Path("Cooked/mesh.masset"));
        if (!memory_read || memory_read.Value().Sections().size() != 2)
            return false;

        const auto target = directory / "mesh.masset";
        {
            std::ofstream output(target, std::ios::binary);
            output << "old-cooked";
        }
        if (!GE::Cook::PublishCookedContainer(request, target))
        {
            return false;
        }
        auto loose = GE::FileSystem::LooseFileMount::Open(directory);
        if (!loose)
            return false;
        auto loose_read = GE::Cook::ReadCookedContainer(loose.Value(), Path("mesh.masset"));
        return loose_read && loose_read.Value().Sections().size() == 2;
    }
} // namespace

int main()
{
    const std::filesystem::path directory = std::filesystem::temp_directory_path() / "MEngineFileCookFoundation";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory);

    const bool paths_and_mounts = TestLogicalPathsAndMounts(directory);
    const bool publication = paths_and_mounts && TestAtomicPublication(directory);
    const bool cooked = publication && TestCookedContainer(directory);
    std::filesystem::remove_all(directory, ignored);

    if (!paths_and_mounts)
        return 1;
    if (!publication)
        return 2;
    if (!cooked)
        return 3;
    std::cout << "[FileCookFoundationSmoke] Passed\n";
    return 0;
}
