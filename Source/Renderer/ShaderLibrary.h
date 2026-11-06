#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace GE::Render
{
    bool LoadShaderBytecode(const std::filesystem::path& source, std::string_view entry, std::vector<std::byte>& output,
                            std::string& error);
}
