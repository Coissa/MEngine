#pragma once

#include "AssetId.h"

#include <Core/Diagnostics/Result.h>

#include <filesystem>
#include <iosfwd>
#include <string>

namespace GE::Asset
{
    struct ProjectDescriptor final
    {
        AssetId namespace_id;
        std::string startup_scene;
    };

    class ProjectDescriptorSerializer final
    {
    public:
        static constexpr unsigned int CurrentVersion = 1;

        [[nodiscard]] static Core::Result<ProjectDescriptor> Load(std::istream& input);
        [[nodiscard]] static Core::Result<ProjectDescriptor> LoadFromFile(const std::filesystem::path& path);
        [[nodiscard]] static Core::Result<void> Save(const ProjectDescriptor& descriptor, std::ostream& output);
        [[nodiscard]] static Core::Result<void> SaveToFile(const ProjectDescriptor& descriptor,
                                                           const std::filesystem::path& path);
    };
} // namespace GE::Asset
