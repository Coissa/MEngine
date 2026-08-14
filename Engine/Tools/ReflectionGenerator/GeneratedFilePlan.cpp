#include "GeneratedFilePlan.h"

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

        GeneratedFilePlanResult MakeError(
            GeneratedFilePlanErrorCode code,
            const std::filesystem::path& path)
        {
            return GeneratedFilePlanError{code, path};
        }
    }

    GeneratedFilePlanResult BuildGeneratedFilePlan(
        const HeaderScanJob& job)
    {
        if (!job.module_root.is_absolute())
        {
            return MakeError(
                GeneratedFilePlanErrorCode::ModuleRootNotAbsolute,
                job.module_root);
        }

        if (job.module_relative_header_path.empty())
        {
            return MakeError(
                GeneratedFilePlanErrorCode::
                    EmptyModuleRelativeHeaderPath,
                job.module_relative_header_path);
        }

        if (!IsRelativePath(job.module_relative_header_path))
        {
            return MakeError(
                GeneratedFilePlanErrorCode::
                    ModuleRelativeHeaderPathNotRelative,
                job.module_relative_header_path);
        }

        if (ContainsParentTraversal(job.module_relative_header_path))
        {
            return MakeError(
                GeneratedFilePlanErrorCode::
                    ModuleRelativeHeaderPathContainsParentTraversal,
                job.module_relative_header_path);
        }

        const std::filesystem::path relative_header_path =
            job.module_relative_header_path.lexically_normal();
        const std::filesystem::path header_filename =
            relative_header_path.filename();
        if (header_filename.empty() || header_filename == ".")
        {
            return MakeError(
                GeneratedFilePlanErrorCode::MissingHeaderFileName,
                job.module_relative_header_path);
        }

        if (!job.header_path.is_absolute())
        {
            return MakeError(
                GeneratedFilePlanErrorCode::HeaderPathNotAbsolute,
                job.header_path);
        }

        const std::filesystem::path expected_header_path =
            (job.module_root / relative_header_path).lexically_normal();
        if (job.header_path.lexically_normal() != expected_header_path)
        {
            return MakeError(
                GeneratedFilePlanErrorCode::
                    HeaderPathDoesNotMatchModuleRelativePath,
                job.header_path);
        }

        if (!job.output_directory.is_absolute())
        {
            return MakeError(
                GeneratedFilePlanErrorCode::OutputDirectoryNotAbsolute,
                job.output_directory);
        }

        std::filesystem::path generated_header_filename = header_filename;
        generated_header_filename += ".reflection.generated.h";

        std::filesystem::path generated_source_filename = header_filename;
        generated_source_filename += ".reflection.generated.cpp";

        const std::filesystem::path generated_directory =
            (job.output_directory / relative_header_path.parent_path())
                .lexically_normal();

        return GeneratedFilePlan{
            job.header_path.lexically_normal(),
            (generated_directory / generated_header_filename)
                .lexically_normal(),
            (generated_directory / generated_source_filename)
                .lexically_normal()};
    }
}
