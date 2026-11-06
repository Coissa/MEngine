#pragma once

#include <Core/Diagnostics/Result.h>

#include <cstddef>
#include <filesystem>
#include <span>

namespace GE::FileSystem
{
    class IAtomicFileReplacer
    {
    public:
        virtual ~IAtomicFileReplacer() = default;
        [[nodiscard]] virtual Core::Result<void> Replace(const std::filesystem::path& temporary_path,
                                                         const std::filesystem::path& target_path) const = 0;
    };

    class NativeAtomicFileReplacer final : public IAtomicFileReplacer
    {
    public:
        [[nodiscard]] Core::Result<void> Replace(const std::filesystem::path& temporary_path,
                                                 const std::filesystem::path& target_path) const override;
    };

    [[nodiscard]] Core::Result<void> PublishFileAtomically(const std::filesystem::path& target_path,
                                                           std::span<const std::byte> bytes,
                                                           const IAtomicFileReplacer& replacer);

    [[nodiscard]] Core::Result<void> PublishFileAtomically(const std::filesystem::path& target_path,
                                                           std::span<const std::byte> bytes);
} // namespace GE::FileSystem
