#pragma once

#include "HeaderScanJob.h"

#include <filesystem>
#include <optional>
#include <system_error>

namespace GE::Reflection::Generator
{
    enum class HeaderScanJobValidationErrorCode
    {
        EmptyModuleName,

        ModuleRootNotAbsolute,
        ModuleRootNotFound,
        ModuleRootNotDirectory,

        EmptyModuleRelativeHeaderPath,
        ModuleRelativeHeaderPathNotRelative,
        ModuleRelativeHeaderPathContainsParentTraversal,

        HeaderPathNotAbsolute,
        HeaderPathDoesNotMatchModuleRelativePath,
        HeaderFileNotFound,
        HeaderPathNotRegularFile,

        OutputDirectoryNotAbsolute,
        OutputPathNotDirectory,

        CompileCommandSourceNotAbsolute,
        CompileCommandSourceNotFound,
        CompileCommandSourceNotRegularFile,

        FileSystemQueryFailed
    };

    struct HeaderScanJobValidationError
    {
        HeaderScanJobValidationErrorCode code;
        std::filesystem::path path;
        std::error_code filesystem_error;
    };

    [[nodiscard]]
    std::optional<HeaderScanJobValidationError>
    ValidateHeaderScanJob(const HeaderScanJob& job);
}
