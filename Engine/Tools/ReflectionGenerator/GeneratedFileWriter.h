#pragma once

#include "GeneratedCodeFile.h"

#include <filesystem>
#include <span>
#include <system_error>
#include <variant>
#include <vector>

namespace GE::Reflection::Generator
{
    /// Describes whether one requested output changed the filesystem.
    enum class GeneratedFileWriteDisposition
    {
        Written,
        Unchanged
    };

    /// Successful outcome for one generated file.
    struct GeneratedFileWriteRecord
    {
        std::filesystem::path path;
        GeneratedFileWriteDisposition disposition;
    };

    /// Successful outcome for the complete input batch, in input order.
    struct GeneratedFilesWriteOutput
    {
        std::vector<GeneratedFileWriteRecord> files;
    };

    /// Identifies the stage at which filesystem output failed.
    enum class GeneratedFileWriteErrorCode
    {
        EmptyPath,
        PathNotAbsolute,
        DuplicateOutputPath,
        CreateParentDirectoryFailed,
        InspectDestinationFailed,
        DestinationNotRegularFile,
        ReadDestinationFailed,
        CreateTemporaryFileFailed,
        WriteTemporaryFileFailed,
        FlushTemporaryFileFailed,
        CommitTemporaryFileFailed
    };

    /// Describes one failed output operation.
    struct GeneratedFileWriteError
    {
        GeneratedFileWriteErrorCode code;

        /// Final destination involved in the failure.
        std::filesystem::path path;

        /// Temporary path when a staging or commit operation was attempted.
        std::filesystem::path temporary_path;

        /// Native filesystem error, when one was supplied by the platform.
        std::error_code filesystem_error;

        /// Files that were already committed or found unchanged before the
        /// failure. Each individual Written record was atomically replaced.
        std::vector<GeneratedFileWriteRecord> completed_files;
    };

    using GeneratedFilesWriteResult =
        std::variant<GeneratedFilesWriteOutput, GeneratedFileWriteError>;

    /// Writes generated files without exposing partially-written destinations.
    ///
    /// Every path must be non-empty, absolute, and unique. Existing regular
    /// files with identical bytes are reported as Unchanged. Changed contents
    /// are first flushed to a uniquely named temporary file in the destination
    /// directory and then atomically moved over that single destination.
    ///
    /// A batch is not a cross-file transaction: if a later commit fails, files
    /// listed in GeneratedFileWriteError::completed_files remain committed.
    [[nodiscard]] GeneratedFilesWriteResult WriteGeneratedCodeFiles(
        std::span<const GeneratedCodeFile> files);
}
