#pragma once

#include <Core/Diagnostics/Result.h>

#include <filesystem>

namespace GE::Asset::Detail
{
    [[nodiscard]] Core::Result<void> ReplaceFileAtomically(const std::filesystem::path& temporary_path,
                                                           const std::filesystem::path& target_path);
    void RemoveTemporaryFile(const std::filesystem::path& path) noexcept;
} // namespace GE::Asset::Detail
