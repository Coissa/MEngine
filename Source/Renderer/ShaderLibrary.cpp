#include "ShaderLibrary.h"
#include <fstream>
namespace GE::Render
{
    bool LoadShaderBytecode(const std::filesystem::path& source, std::string_view entry, std::vector<std::byte>& output,
                            std::string& error)
    {
        const auto path = source.parent_path() / (source.stem().string() + "." + std::string(entry) + ".cso");
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0)
        {
            error = "Cannot read shader bytecode: " + path.string();
            return false;
        }
        output.resize(static_cast<size_t>(input.tellg()));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char*>(output.data()), output.size()))
        {
            error = "Incomplete shader bytecode: " + path.string();
            return false;
        }
        return true;
    }
} // namespace GE::Render
