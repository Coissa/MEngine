#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace GE::Reflection::Generator
{
    struct CompileCommand
    {
        std::filesystem::path working_directory;
        std::filesystem::path source_file;
        std::vector<std::string> arguments;
    };

}