#include "AssetFile.h"

#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace GE::Asset::Detail
{
    Core::Result<void> ReplaceFileAtomically(
        const std::filesystem::path& temporary_path,
        const std::filesystem::path& target_path)
    {
#ifdef _WIN32
        if (::MoveFileExW(temporary_path.c_str(), target_path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
        {
            return Core::Result<void>::Failure({
                .domain = Core::ErrorDomain::Filesystem,
                .code = Core::ErrorCode::IoFailure,
                .operation = "ReplaceAssetFile",
                .message = "Could not replace the target file",
                .resource = target_path.generic_string(),
                .native_code = static_cast<std::int64_t>(::GetLastError())});
        }
#else
        std::error_code error;
        std::filesystem::rename(temporary_path, target_path, error);
        if (error)
        {
            return Core::Result<void>::Failure({
                .domain = Core::ErrorDomain::Filesystem,
                .code = Core::ErrorCode::IoFailure,
                .operation = "ReplaceAssetFile",
                .message = "Could not replace the target file",
                .resource = target_path.generic_string(),
                .native_code = static_cast<std::int64_t>(error.value())});
        }
#endif
        return Core::Result<void>::Success();
    }

    void RemoveTemporaryFile(const std::filesystem::path& path) noexcept
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
}
