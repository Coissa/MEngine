#include "AtomicFilePublisher.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace GE::FileSystem
{
    namespace
    {
        [[nodiscard]] Core::EngineError PublishError(std::string operation, std::string message,
                                                     const std::filesystem::path& path,
                                                     std::optional<std::int64_t> native_code = std::nullopt)
        {
            return {.domain = Core::ErrorDomain::Filesystem,
                    .code = Core::ErrorCode::IoFailure,
                    .operation = std::move(operation),
                    .message = std::move(message),
                    .resource = path.generic_string(),
                    .native_code = native_code};
        }

        [[nodiscard]] std::filesystem::path TemporarySibling(const std::filesystem::path& target)
        {
            static std::atomic<std::uint64_t> sequence{0};
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            std::filesystem::path name = target.filename();
            name += ".mengine-" + std::to_string(stamp) + "-" +
                    std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)) + ".tmp";
            return target.parent_path() / name;
        }

        void RemoveTemporaryFile(const std::filesystem::path& path) noexcept
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    } // namespace

    Core::Result<void> NativeAtomicFileReplacer::Replace(const std::filesystem::path& temporary_path,
                                                         const std::filesystem::path& target_path) const
    {
#ifdef _WIN32
        if (::MoveFileExW(temporary_path.c_str(), target_path.c_str(),
                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
        {
            return Core::Result<void>::Failure(PublishError("ReplacePublishedFile", "Could not replace target file",
                                                            target_path, static_cast<std::int64_t>(::GetLastError())));
        }
#else
        std::error_code error;
        std::filesystem::rename(temporary_path, target_path, error);
        if (error)
        {
            return Core::Result<void>::Failure(
                PublishError("ReplacePublishedFile", "Could not replace target file", target_path, error.value()));
        }
#endif
        return Core::Result<void>::Success();
    }

    Core::Result<void> PublishFileAtomically(const std::filesystem::path& target_path, std::span<const std::byte> bytes,
                                             const IAtomicFileReplacer& replacer)
    {
        if (target_path.empty() || target_path.filename().empty())
        {
            return Core::Result<void>::Failure({.domain = Core::ErrorDomain::Filesystem,
                                                .code = Core::ErrorCode::InvalidArgument,
                                                .operation = "PublishFileAtomically",
                                                .message = "Target path must name a file"});
        }
        if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        {
            return Core::Result<void>::Failure({.domain = Core::ErrorDomain::Filesystem,
                                                .code = Core::ErrorCode::InvalidArgument,
                                                .operation = "PublishFileAtomically",
                                                .message = "File is too large for the output stream",
                                                .resource = target_path.generic_string()});
        }

        const std::filesystem::path temporary = TemporarySibling(target_path);
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return Core::Result<void>::Failure(
                PublishError("PublishFileAtomically", "Could not open temporary file", temporary));
        }
        if (!bytes.empty())
        {
            output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        output.flush();
        const bool write_succeeded = static_cast<bool>(output);
        output.close();
        if (!write_succeeded)
        {
            RemoveTemporaryFile(temporary);
            return Core::Result<void>::Failure(
                PublishError("PublishFileAtomically", "Could not write temporary file", temporary));
        }

        auto replaced = replacer.Replace(temporary, target_path);
        if (!replaced)
            RemoveTemporaryFile(temporary);
        return replaced;
    }

    Core::Result<void> PublishFileAtomically(const std::filesystem::path& target_path, std::span<const std::byte> bytes)
    {
        const NativeAtomicFileReplacer replacer;
        return PublishFileAtomically(target_path, bytes, replacer);
    }
} // namespace GE::FileSystem
