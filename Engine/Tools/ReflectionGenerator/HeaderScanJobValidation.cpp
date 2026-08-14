#include "HeaderScanJobValidation.h"

namespace GE::Reflection::Generator
{
    namespace
    {
        bool IsRelativePath(const std::filesystem::path& path)
        {
            return path.is_relative()
                && !path.has_root_name()
                && !path.has_root_directory();
        }

        bool ContainsParentTraversal(const std::filesystem::path& path)
        {
            for (const std::filesystem::path& component : path)
            {
                if (component == "..")
                {
                    return true;
                }
            }

            return false;
        }
    }

    std::optional<HeaderScanJobValidationError>
    ValidateHeaderScanJob(const HeaderScanJob& job)
    {
        if (job.module_name.empty())
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::EmptyModuleName,
                {},
                {}
            };
        }

        if (!job.module_root.is_absolute())
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::ModuleRootNotAbsolute,
                job.module_root,
                {}};
        }

        std::error_code filesystem_error;
        const bool module_root_exists =
            std::filesystem::exists(job.module_root, filesystem_error);
        if (filesystem_error)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::FileSystemQueryFailed,
                job.module_root,
                filesystem_error};
        }

        if (!module_root_exists)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::ModuleRootNotFound,
                job.module_root,
                {}};
        }

        filesystem_error.clear();
        const bool module_root_is_directory =
            std::filesystem::is_directory(job.module_root, filesystem_error);
        if (filesystem_error)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::FileSystemQueryFailed,
                job.module_root,
                filesystem_error};
        }

        if (!module_root_is_directory)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::ModuleRootNotDirectory,
                job.module_root,
                {}};
        }

        if (job.module_relative_header_path.empty())
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::
                    EmptyModuleRelativeHeaderPath,
                job.module_relative_header_path,
                {}};
        }

        if (!IsRelativePath(job.module_relative_header_path))
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::
                    ModuleRelativeHeaderPathNotRelative,
                job.module_relative_header_path,
                {}};
        }

        if (ContainsParentTraversal(job.module_relative_header_path))
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::
                    ModuleRelativeHeaderPathContainsParentTraversal,
                job.module_relative_header_path,
                {}};
        }

        if (!job.header_path.is_absolute())
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::HeaderPathNotAbsolute,
                job.header_path,
                std::error_code{}
            };
        }

        const std::filesystem::path expected_header_path =
            (job.module_root / job.module_relative_header_path)
                .lexically_normal();
        if (job.header_path.lexically_normal() != expected_header_path)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::
                    HeaderPathDoesNotMatchModuleRelativePath,
                job.header_path,
                {}};
        }

        filesystem_error.clear();
        const bool header_exists =
            std::filesystem::exists(job.header_path, filesystem_error);
        if (filesystem_error)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::FileSystemQueryFailed,
                job.header_path,
                filesystem_error
            };
        }

        if (!header_exists)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::HeaderFileNotFound,
                job.header_path,
                {}
            };
        }

        filesystem_error.clear();
        const bool header_is_regular_file =
            std::filesystem::is_regular_file(job.header_path, filesystem_error);
        if (filesystem_error)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::FileSystemQueryFailed,
                job.header_path,
                filesystem_error
            };
        }

        if (!header_is_regular_file)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::HeaderPathNotRegularFile,
                job.header_path,
                {}
            };
        }

        if (!job.output_directory.is_absolute())
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::OutputDirectoryNotAbsolute,
                job.output_directory,
                {}
            };
        }

        filesystem_error.clear();
        const bool output_exists =
            std::filesystem::exists(job.output_directory, filesystem_error);
        if (filesystem_error)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::FileSystemQueryFailed,
                job.output_directory,
                filesystem_error
            };
        }

        if (output_exists)
        {
            filesystem_error.clear();
            const bool output_is_directory =
                std::filesystem::is_directory(
                    job.output_directory,
                    filesystem_error);
            if (filesystem_error)
            {
                return HeaderScanJobValidationError{
                    HeaderScanJobValidationErrorCode::FileSystemQueryFailed,
                    job.output_directory,
                    filesystem_error
                };
            }

            if (!output_is_directory)
            {
                return HeaderScanJobValidationError{
                    HeaderScanJobValidationErrorCode::OutputPathNotDirectory,
                    job.output_directory,
                    {}
                };
            }
        }

        if (!job.compile_command_source.is_absolute())
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::CompileCommandSourceNotAbsolute,
                job.compile_command_source,
                {}
            };
        }

        filesystem_error.clear();
        const bool compile_command_source_exists =
            std::filesystem::exists(
                job.compile_command_source,
                filesystem_error);
        if (filesystem_error)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::FileSystemQueryFailed,
                job.compile_command_source,
                filesystem_error
            };
        }

        if (!compile_command_source_exists)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::CompileCommandSourceNotFound,
                job.compile_command_source,
                {}
            };
        }

        filesystem_error.clear();
        const bool compile_command_source_is_regular_file =
            std::filesystem::is_regular_file(
                job.compile_command_source,
                filesystem_error);
        if (filesystem_error)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::FileSystemQueryFailed,
                job.compile_command_source,
                filesystem_error
            };
        }

        if (!compile_command_source_is_regular_file)
        {
            return HeaderScanJobValidationError{
                HeaderScanJobValidationErrorCode::CompileCommandSourceNotRegularFile,
                job.compile_command_source,
                {}
            };
        }

        return std::nullopt;
    }
}
