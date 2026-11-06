#pragma once

#include "LogicalPath.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <unordered_map>
#include <vector>

namespace GE::FileSystem
{
    using ByteBuffer = std::vector<std::byte>;

    class IReadOnlyFileMount
    {
    public:
        virtual ~IReadOnlyFileMount() = default;

        [[nodiscard]] virtual Core::Result<std::uint64_t> FileSize(const LogicalPath& path) const = 0;
        [[nodiscard]] virtual Core::Result<ByteBuffer> ReadAll(const LogicalPath& path) const = 0;
        [[nodiscard]] virtual Core::Result<ByteBuffer> ReadRange(const LogicalPath& path, std::uint64_t offset,
                                                                 std::uint64_t size) const = 0;
    };

    class MemoryFileMount final : public IReadOnlyFileMount
    {
    public:
        [[nodiscard]] Core::Result<void> AddFile(LogicalPath path, ByteBuffer bytes);
        [[nodiscard]] Core::Result<std::uint64_t> FileSize(const LogicalPath& path) const override;
        [[nodiscard]] Core::Result<ByteBuffer> ReadAll(const LogicalPath& path) const override;
        [[nodiscard]] Core::Result<ByteBuffer> ReadRange(const LogicalPath& path, std::uint64_t offset,
                                                         std::uint64_t size) const override;

    private:
        std::unordered_map<std::string, ByteBuffer> m_files;
    };

    class LooseFileMount final : public IReadOnlyFileMount
    {
    public:
        [[nodiscard]] static Core::Result<LooseFileMount> Open(const std::filesystem::path& root);

        [[nodiscard]] Core::Result<std::uint64_t> FileSize(const LogicalPath& path) const override;
        [[nodiscard]] Core::Result<ByteBuffer> ReadAll(const LogicalPath& path) const override;
        [[nodiscard]] Core::Result<ByteBuffer> ReadRange(const LogicalPath& path, std::uint64_t offset,
                                                         std::uint64_t size) const override;

    private:
        explicit LooseFileMount(std::filesystem::path canonical_root) : m_root(std::move(canonical_root)) {}

        [[nodiscard]] Core::Result<std::filesystem::path> Resolve(const LogicalPath& path) const;

        std::filesystem::path m_root;
    };
} // namespace GE::FileSystem
