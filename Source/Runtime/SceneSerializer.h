#pragma once

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>

namespace GE::Serialization
{
    struct Context;
}

namespace GE::Runtime
{
    class ComponentRegistry;
    class World;

    class SceneSerializer final
    {
    public:
        static constexpr unsigned int CurrentVersion = 5;
        [[nodiscard]] static bool Save(const World& world, std::ostream& output, const Serialization::Context& context,
                                       std::string& out_error);
        [[nodiscard]] static std::unique_ptr<World> Load(std::istream& input, const ComponentRegistry& registry,
                                                         const Serialization::Context& context, std::string& out_error);
        [[nodiscard]] static bool SaveToFile(const World& world, const std::filesystem::path& path,
                                             const Serialization::Context& context, std::string& out_error);
        [[nodiscard]] static std::unique_ptr<World> LoadFromFile(const std::filesystem::path& path,
                                                                 const ComponentRegistry& registry,
                                                                 const Serialization::Context& context,
                                                                 std::string& out_error);
    };
} // namespace GE::Runtime
