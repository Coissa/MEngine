#include "FileMount.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

namespace GE::FileSystem
{
    namespace
    {
        [[nodiscard]] Core::EngineError FileError(
            Core::ErrorCode code,
            std::string operation,
            std::string message,
            const LogicalPath& path,
            std::optional<std::int64_t> native_code = std::nullopt)
        {
            return {
                .domain = Core::ErrorDomain::Filesystem,
                .code = code,
                .operation = std::move(operation),
                .message = std::move(message),
                .resource = path.String(),
                .native_code = native_code};
        }

        [[nodiscard]] Core::Result<ByteBuffer> SliceBytes(
            const ByteBuffer& source,
            const LogicalPath& path,
            std::uint64_t offset,
            std::uint64_t size,
            std::string operation)
        {
            const std::uint64_t source_size = source.size();
            if (offset > source_size || size > source_size - offset)
            {
                return Core::Result<ByteBuffer>::Failure(FileError(
                    Core::ErrorCode::InvalidArgument,
                    std::move(operation),
                    "Requested range is outside the file",
                    path));
            }
            const auto begin = source.begin() + static_cast<std::size_t>(offset);
            return Core::Result<ByteBuffer>::Success(ByteBuffer(
                begin, begin + static_cast<std::size_t>(size)));
        }

        [[nodiscard]] bool IsWithinRoot(
            const std::filesystem::path& root,
            const std::filesystem::path& candidate)
        {
            auto root_part = root.begin();
            auto candidate_part = candidate.begin();
            while (root_part != root.end() && candidate_part != candidate.end())
            {
#ifdef _WIN32
                std::wstring left = root_part->wstring();
                std::wstring right = candidate_part->wstring();
                std::transform(left.begin(), left.end(), left.begin(), towlower);
                std::transform(right.begin(), right.end(), right.begin(), towlower);
                if (left != right) return false;
#else
                if (*root_part != *candidate_part) return false;
#endif
                ++root_part;
                ++candidate_part;
            }
            return root_part == root.end();
        }
    }

    Core::Result<void> MemoryFileMount::AddFile(
        LogicalPath path, ByteBuffer bytes)
    {
        if (path.Empty())
        {
            return Core::Result<void>::Failure({
                .domain = Core::ErrorDomain::Filesystem,
                .code = Core::ErrorCode::InvalidArgument,
                .operation = "AddMemoryFile",
                .message = "Logical path is empty"});
        }
        if (!m_files.emplace(path.String(), std::move(bytes)).second)
        {
            return Core::Result<void>::Failure({
                .domain = Core::ErrorDomain::Filesystem,
                .code = Core::ErrorCode::Conflict,
                .operation = "AddMemoryFile",
                .message = "A file already exists at the logical path",
                .resource = path.String()});
        }
        return Core::Result<void>::Success();
    }

    Core::Result<std::uint64_t> MemoryFileMount::FileSize(
        const LogicalPath& path) const
    {
        const auto found = m_files.find(path.String());
        if (found == m_files.end())
        {
            return Core::Result<std::uint64_t>::Failure(FileError(
                Core::ErrorCode::NotFound, "MemoryFileSize",
                "File was not found in the mount", path));
        }
        return Core::Result<std::uint64_t>::Success(found->second.size());
    }

    Core::Result<ByteBuffer> MemoryFileMount::ReadAll(
        const LogicalPath& path) const
    {
        const auto found = m_files.find(path.String());
        if (found == m_files.end())
        {
            return Core::Result<ByteBuffer>::Failure(FileError(
                Core::ErrorCode::NotFound, "ReadMemoryFile",
                "File was not found in the mount", path));
        }
        return Core::Result<ByteBuffer>::Success(found->second);
    }

    Core::Result<ByteBuffer> MemoryFileMount::ReadRange(
        const LogicalPath& path,
        std::uint64_t offset,
        std::uint64_t size) const
    {
        const auto found = m_files.find(path.String());
        if (found == m_files.end())
        {
            return Core::Result<ByteBuffer>::Failure(FileError(
                Core::ErrorCode::NotFound, "ReadMemoryFileRange",
                "File was not found in the mount", path));
        }
        return SliceBytes(found->second, path, offset, size,
            "ReadMemoryFileRange");
    }

    Core::Result<LooseFileMount> LooseFileMount::Open(
        const std::filesystem::path& root)
    {
        std::error_code error;
        const std::filesystem::path canonical_root =
            std::filesystem::canonical(root, error);
        if (error || !std::filesystem::is_directory(canonical_root, error))
        {
            return Core::Result<LooseFileMount>::Failure({
                .domain = Core::ErrorDomain::Filesystem,
                .code = Core::ErrorCode::NotFound,
                .operation = "OpenLooseFileMount",
                .message = "Mount root does not exist or is not a directory",
                .resource = root.generic_string(),
                .native_code = error
                    ? std::optional<std::int64_t>(error.value())
                    : std::nullopt});
        }
        return Core::Result<LooseFileMount>::Success(
            LooseFileMount(canonical_root));
    }

    Core::Result<std::filesystem::path> LooseFileMount::Resolve(
        const LogicalPath& path) const
    {
        if (path.Empty())
        {
            return Core::Result<std::filesystem::path>::Failure(FileError(
                Core::ErrorCode::InvalidArgument, "ResolveLooseFile",
                "Logical path is empty", path));
        }
        std::error_code error;
        const std::filesystem::path candidate =
            std::filesystem::weakly_canonical(m_root / path.String(), error);
        if (error || !IsWithinRoot(m_root, candidate))
        {
            return Core::Result<std::filesystem::path>::Failure(FileError(
                Core::ErrorCode::InvalidArgument, "ResolveLooseFile",
                "Logical path does not resolve inside the mount", path,
                error ? std::optional<std::int64_t>(error.value())
                      : std::nullopt));
        }
        return Core::Result<std::filesystem::path>::Success(candidate);
    }

    Core::Result<std::uint64_t> LooseFileMount::FileSize(
        const LogicalPath& path) const
    {
        auto resolved = Resolve(path);
        if (!resolved)
            return Core::Result<std::uint64_t>::Failure(resolved.Error());
        std::error_code error;
        const std::uintmax_t size = std::filesystem::file_size(
            resolved.Value(), error);
        if (error)
        {
            return Core::Result<std::uint64_t>::Failure(FileError(
                error == std::errc::no_such_file_or_directory
                    ? Core::ErrorCode::NotFound
                    : Core::ErrorCode::IoFailure,
                "LooseFileSize", "Could not query file size", path,
                error.value()));
        }
        return Core::Result<std::uint64_t>::Success(
            static_cast<std::uint64_t>(size));
    }

    Core::Result<ByteBuffer> LooseFileMount::ReadAll(
        const LogicalPath& path) const
    {
        auto size = FileSize(path);
        if (!size) return Core::Result<ByteBuffer>::Failure(size.Error());
        return ReadRange(path, 0, size.Value());
    }

    Core::Result<ByteBuffer> LooseFileMount::ReadRange(
        const LogicalPath& path,
        std::uint64_t offset,
        std::uint64_t size) const
    {
        auto resolved = Resolve(path);
        if (!resolved)
            return Core::Result<ByteBuffer>::Failure(resolved.Error());

        std::error_code error;
        const std::uintmax_t file_size = std::filesystem::file_size(
            resolved.Value(), error);
        if (error)
        {
            return Core::Result<ByteBuffer>::Failure(FileError(
                error == std::errc::no_such_file_or_directory
                    ? Core::ErrorCode::NotFound
                    : Core::ErrorCode::IoFailure,
                "ReadLooseFileRange", "Could not query file size", path,
                error.value()));
        }
        if (offset > file_size || size > file_size - offset
            || size > std::numeric_limits<std::size_t>::max()
            || size > static_cast<std::uint64_t>(
                std::numeric_limits<std::streamsize>::max())
            || offset > static_cast<std::uint64_t>(
                std::numeric_limits<std::streamoff>::max()))
        {
            return Core::Result<ByteBuffer>::Failure(FileError(
                Core::ErrorCode::InvalidArgument, "ReadLooseFileRange",
                "Requested range is outside the file", path));
        }

        std::ifstream input(resolved.Value(), std::ios::binary);
        if (!input)
        {
            return Core::Result<ByteBuffer>::Failure(FileError(
                Core::ErrorCode::IoFailure, "ReadLooseFileRange",
                "Could not open file", path));
        }
        input.seekg(static_cast<std::streamoff>(offset));
        ByteBuffer bytes(static_cast<std::size_t>(size));
        if (size != 0)
        {
            input.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(size));
        }
        if (!input || static_cast<std::uint64_t>(input.gcount()) != size)
        {
            return Core::Result<ByteBuffer>::Failure(FileError(
                Core::ErrorCode::IoFailure, "ReadLooseFileRange",
                "Could not read the complete requested range", path));
        }
        return Core::Result<ByteBuffer>::Success(std::move(bytes));
    }
}
