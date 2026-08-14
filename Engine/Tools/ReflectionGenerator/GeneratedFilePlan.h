#pragma once

#include "HeaderScanJob.h"

#include <filesystem>
#include <variant>

namespace GE::Reflection::Generator
{
    /// Describes every filesystem path needed to generate reflection code for
    /// one source header. This is a path-only plan and contains no file text.
    struct GeneratedFilePlan
    {
        /// Absolute path of the original C++ header scanned by Clang.
        std::filesystem::path source_header_path;

        /// Absolute destination path of the generated declaration header.
        std::filesystem::path generated_header_path;

        /// Absolute destination path of the generated implementation source.
        std::filesystem::path generated_source_path;
    };

    /// Identifies why generated output paths could not be planned safely.
    enum class GeneratedFilePlanErrorCode
    {
        /// HeaderScanJob::module_root must be an absolute path.
        ModuleRootNotAbsolute,

        /// The module-relative source header path must not be empty.
        EmptyModuleRelativeHeaderPath,

        /// The source header identity must be relative to the module root.
        ModuleRelativeHeaderPathNotRelative,

        /// Parent traversal would allow generated files to escape the output
        /// directory and is therefore rejected.
        ModuleRelativeHeaderPathContainsParentTraversal,

        /// The relative source header path must end with a usable file name.
        MissingHeaderFileName,

        /// HeaderScanJob::header_path must be an absolute path.
        HeaderPathNotAbsolute,

        /// The absolute source path must equal module_root joined with the
        /// module-relative source header path.
        HeaderPathDoesNotMatchModuleRelativePath,

        /// HeaderScanJob::output_directory must be an absolute path.
        OutputDirectoryNotAbsolute
    };

    /// Contains the reason and offending path when file planning fails.
    struct GeneratedFilePlanError
    {
        /// Machine-readable category used by the caller to format an error.
        GeneratedFilePlanErrorCode code;

        /// The input path that violated the selected planning rule.
        std::filesystem::path path;
    };

    /// On success contains GeneratedFilePlan; on failure contains one
    /// structural path error. No filesystem query is performed by planning.
    using GeneratedFilePlanResult =
        std::variant<GeneratedFilePlan, GeneratedFilePlanError>;

    /// Builds deterministic .reflection.generated.h/.cpp output paths for one
    /// header scan job.
    ///
    /// Input: a HeaderScanJob containing an absolute module root, a safe
    /// module-relative header identity, an absolute source header path, and an
    /// absolute output directory.
    ///
    /// Result: GeneratedFilePlan when all structural path invariants hold, or
    /// GeneratedFilePlanError when an invariant is violated. The function does
    /// not create directories, read source files, or write generated files.
    [[nodiscard]] GeneratedFilePlanResult BuildGeneratedFilePlan(
        const HeaderScanJob& job);
}
