#include "HeaderScanJobBuilder.h"

namespace GE::Reflection::Generator
{
    namespace
    {
        std::filesystem::path NormalizeDirectoryPath(
            const std::filesystem::path& path)
        {
            std::filesystem::path normalized = path.lexically_normal();
            if (normalized.has_relative_path() && normalized.filename().empty())
            {
                normalized = normalized.parent_path();
            }

            return normalized;
        }
    }

    std::vector<HeaderScanJob> BuildHeaderScanJobs(
        const ModuleManifest& manifest,
        const std::filesystem::path& manifest_directory)
    {
        std::vector<HeaderScanJob> result;
        result.reserve(manifest.header_files.size());

        const std::filesystem::path module_root =
            NormalizeDirectoryPath(
                manifest_directory / manifest.module_root);
        const std::filesystem::path output_directory =
            NormalizeDirectoryPath(
                module_root / manifest.output_directory);
        const std::filesystem::path compile_command_source =
            (module_root / manifest.compile_command_source).lexically_normal();

        for (const std::filesystem::path& header_file : manifest.header_files)
        {
            const std::filesystem::path relative_header_path =
                header_file.lexically_normal();
            result.push_back(HeaderScanJob{
                manifest.module_name,
                module_root,
                relative_header_path,
                (module_root / relative_header_path).lexically_normal(),
                output_directory,
                compile_command_source});
        }

        return result;
    }
}
