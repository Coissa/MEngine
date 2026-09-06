#pragma once

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>

namespace GE::Runtime
{
    class World;

    class SceneSerializer final
    {
    public:
        static constexpr unsigned int CurrentVersion = 1;

        [[nodiscard]] static bool Save(
            const World& world,
            std::ostream& output,
            std::string& out_error);
        [[nodiscard]] static std::unique_ptr<World> Load(
            std::istream& input,
            std::string& out_error);

        [[nodiscard]] static bool SaveToFile(
            const World& world,
            const std::filesystem::path& path,
            std::string& out_error);
        [[nodiscard]] static std::unique_ptr<World> LoadFromFile(
            const std::filesystem::path& path,
            std::string& out_error);
    };
}
