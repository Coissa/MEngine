#pragma once

#include "GeneratedCodeFile.h"

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace GE::Reflection::Generator
{
    /// One per-header registration function consumed by a module aggregator.
    struct ReflectionHeaderRegistration
    {
        std::filesystem::path generated_header_path;
        std::string registration_function_name;
    };

    /// All per-header registrations belonging to one explicit module manifest.
    struct ReflectionModuleRenderInput
    {
        std::string module_name;
        std::filesystem::path output_directory;
        std::vector<ReflectionHeaderRegistration> header_registrations;
    };

    struct ReflectionModuleRenderOutput
    {
        GeneratedCodeFile header_file;
        GeneratedCodeFile source_file;
        std::string registration_function_name;
    };

    enum class ReflectionModuleRenderErrorCode
    {
        EmptyModuleName,
        InvalidModuleName,
        OutputDirectoryNotAbsolute,
        EmptyHeaderRegistrations,
        GeneratedHeaderPathNotAbsolute,
        GeneratedHeaderOutsideOutputDirectory,
        EmptyRegistrationFunctionName,
        DuplicateGeneratedHeaderPath,
        DuplicateRegistrationFunctionName
    };

    struct ReflectionModuleRenderError
    {
        ReflectionModuleRenderErrorCode code;
        std::string module_name;
        std::filesystem::path path;
        std::string registration_function_name;
    };

    using ReflectionModuleRenderResult =
        std::variant<
            ReflectionModuleRenderOutput,
            ReflectionModuleRenderError>;

    /// Renders a module-level function that invokes every per-header
    /// registration function in manifest order and stops on the first error.
    [[nodiscard]] ReflectionModuleRenderResult
    RenderReflectionModuleRegistration(
        const ReflectionModuleRenderInput& input);
}
