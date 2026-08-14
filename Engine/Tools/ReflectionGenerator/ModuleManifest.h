#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace GE::Reflection::Generator
{
    struct ModuleManifest
    {
        std::string module_name;
        std::filesystem::path module_root;
        std::filesystem::path output_directory;
        std::vector<std::filesystem::path> header_files;
        std::filesystem::path compile_command_source;
    };

    enum class ModuleManifestValidationResult
    {
        Valid,
        EmptyModuleName,
        EmptyModuleRoot,
        ModuleRootNotRelative,
        EmptyOutputDirectory,
        OutputDirectoryNotRelative,
        OutputDirectoryContainsParentTraversal,
        EmptyHeaderFiles,
        EmptyHeaderFile,
        HeaderFileNotRelative,
        HeaderFileContainsParentTraversal,
        EmptyCompileCommandSource,
        CompileCommandSourceNotRelative,
        CompileCommandSourceContainsParentTraversal,
        DuplicateHeaderFile
    };

    [[nodiscard]] inline bool IsModuleRelativePath(
        const std::filesystem::path& path)
    {
        return path.is_relative()
            && !path.has_root_name()
            && !path.has_root_directory();
    }

    [[nodiscard]] inline bool ContainsParentTraversal(
        const std::filesystem::path& path)
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

    [[nodiscard]] inline ModuleManifestValidationResult
    ValidateModuleManifest(const ModuleManifest& manifest)
    {
        if (manifest.module_name.empty())
        {
            return ModuleManifestValidationResult::EmptyModuleName;
        }

        if (manifest.module_root.empty())
        {
            return ModuleManifestValidationResult::EmptyModuleRoot;
        }

        if (!IsModuleRelativePath(manifest.module_root))
        {
            return ModuleManifestValidationResult::ModuleRootNotRelative;
        }

        if (manifest.output_directory.empty())
        {
            return ModuleManifestValidationResult::EmptyOutputDirectory;
        }

        if (!IsModuleRelativePath(manifest.output_directory))
        {
            return ModuleManifestValidationResult::OutputDirectoryNotRelative;
        }

        if (ContainsParentTraversal(manifest.output_directory))
        {
            return ModuleManifestValidationResult::
                OutputDirectoryContainsParentTraversal;
        }

        if (manifest.header_files.empty())
        {
            return ModuleManifestValidationResult::EmptyHeaderFiles;
        }

        if (manifest.compile_command_source.empty())
        {
            return ModuleManifestValidationResult::
                EmptyCompileCommandSource;
        }

        if (!IsModuleRelativePath(manifest.compile_command_source))
        {
            return ModuleManifestValidationResult::
                CompileCommandSourceNotRelative;
        }

        if (ContainsParentTraversal(manifest.compile_command_source))
        {
            return ModuleManifestValidationResult::
                CompileCommandSourceContainsParentTraversal;
        }

        std::unordered_set<std::filesystem::path> unique_header_files;
        for (const std::filesystem::path& header_file : manifest.header_files)
        {
            if (header_file.empty())
            {
                return ModuleManifestValidationResult::EmptyHeaderFile;
            }

            if (!IsModuleRelativePath(header_file))
            {
                return ModuleManifestValidationResult::HeaderFileNotRelative;
            }

            if (ContainsParentTraversal(header_file))
            {
                return ModuleManifestValidationResult::
                    HeaderFileContainsParentTraversal;
            }

            if (!unique_header_files.insert(
                    header_file.lexically_normal()).second)
            {
                return ModuleManifestValidationResult::DuplicateHeaderFile;
            }
        }

        return ModuleManifestValidationResult::Valid;
    }
}
