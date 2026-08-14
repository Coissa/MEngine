#pragma once

#include <filesystem>
#include <string>

namespace GE::Reflection::Generator
{
    /// One generated file held entirely in memory. Rendering creates this DTO;
    /// filesystem output consumes it without depending on Clang declarations.
    struct GeneratedCodeFile
    {
        /// Absolute destination path chosen by GeneratedFilePlan.
        std::filesystem::path path;

        /// Complete binary-safe file contents to place at path.
        std::string contents;
    };
}
