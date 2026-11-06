#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace GE::Reflection::Generator
{
    struct HeaderScanJob
    {
        std::string module_name;
        std::filesystem::path module_root;
        std::filesystem::path module_relative_header_path;
        std::filesystem::path header_path;
        std::filesystem::path output_directory;
        std::filesystem::path compile_command_source;
        std::vector<std::string> type_dependencies;
    };
} // namespace GE::Reflection::Generator
