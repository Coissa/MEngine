#include "ProjectDescriptor.h"

#include "AssetFile.h"
#include "AssetKey.h"

#include <fstream>
#include <iomanip>
#include <string>

namespace GE::Asset
{
    namespace
    {
        [[nodiscard]] Core::EngineError ProjectError(Core::ErrorCode code, std::string operation, std::string message,
                                                     std::optional<std::string> resource = std::nullopt)
        {
            return {.domain =
                        code == Core::ErrorCode::IoFailure ? Core::ErrorDomain::Filesystem : Core::ErrorDomain::Asset,
                    .code = code,
                    .operation = std::move(operation),
                    .message = std::move(message),
                    .resource = std::move(resource)};
        }
    } // namespace

    Core::Result<ProjectDescriptor> ProjectDescriptorSerializer::Load(std::istream& input)
    {
        std::string magic;
        unsigned int version = 0;
        std::string token;
        std::string uuid_text;
        if (!(input >> magic >> version) || magic != "MENGINE_PROJECT")
        {
            return Core::Result<ProjectDescriptor>::Failure(ProjectError(
                Core::ErrorCode::ParseFailure, "LoadProjectDescriptor", "Invalid project descriptor header"));
        }
        if (version != CurrentVersion)
        {
            return Core::Result<ProjectDescriptor>::Failure(ProjectError(Core::ErrorCode::UnsupportedVersion,
                                                                         "LoadProjectDescriptor",
                                                                         "Unsupported project descriptor version"));
        }
        if (!(input >> token >> uuid_text) || token != "namespace")
        {
            return Core::Result<ProjectDescriptor>::Failure(
                ProjectError(Core::ErrorCode::ParseFailure, "LoadProjectDescriptor", "Expected project namespace"));
        }
        auto namespace_id = ParseAssetId(uuid_text);
        if (!namespace_id)
        {
            return Core::Result<ProjectDescriptor>::Failure(namespace_id.Error());
        }
        std::string startup_scene;
        input >> token;
        if (token == "startup_scene")
        {
            input >> std::quoted(startup_scene) >> token;
            auto normalized = NormalizeAssetRelativePath(startup_scene);
            if (!normalized)
                return Core::Result<ProjectDescriptor>::Failure(normalized.Error());
            startup_scene = std::move(normalized).Value();
        }
        if (!input || token != "end_project")
        {
            return Core::Result<ProjectDescriptor>::Failure(
                ProjectError(Core::ErrorCode::ParseFailure, "LoadProjectDescriptor", "Expected end_project token"));
        }
        input >> std::ws;
        if (!input.eof())
        {
            return Core::Result<ProjectDescriptor>::Failure(ProjectError(
                Core::ErrorCode::ParseFailure, "LoadProjectDescriptor", "Unexpected trailing project descriptor data"));
        }
        return Core::Result<ProjectDescriptor>::Success({std::move(namespace_id).Value(), std::move(startup_scene)});
    }

    Core::Result<ProjectDescriptor> ProjectDescriptorSerializer::LoadFromFile(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return Core::Result<ProjectDescriptor>::Failure(
                ProjectError(Core::ErrorCode::NotFound, "LoadProjectDescriptorFile",
                             "Could not open project descriptor", path.generic_string()));
        }
        auto result = Load(input);
        if (!result && !result.Error().resource.has_value())
        {
            result.Error().resource = path.generic_string();
        }
        return result;
    }

    Core::Result<void> ProjectDescriptorSerializer::Save(const ProjectDescriptor& descriptor, std::ostream& output)
    {
        if (!descriptor.namespace_id.IsValid())
        {
            return Core::Result<void>::Failure(ProjectError(Core::ErrorCode::InvalidArgument, "SaveProjectDescriptor",
                                                            "Project namespace UUID is invalid"));
        }
        output << "MENGINE_PROJECT " << CurrentVersion << '\n'
               << "namespace " << ToString(descriptor.namespace_id) << '\n';
        if (!descriptor.startup_scene.empty())
            output << "startup_scene " << std::quoted(descriptor.startup_scene) << '\n';
        output << "end_project\n";
        if (!output)
        {
            return Core::Result<void>::Failure(ProjectError(Core::ErrorCode::IoFailure, "SaveProjectDescriptor",
                                                            "Could not write project descriptor"));
        }
        return Core::Result<void>::Success();
    }

    Core::Result<void> ProjectDescriptorSerializer::SaveToFile(const ProjectDescriptor& descriptor,
                                                               const std::filesystem::path& path)
    {
        const std::filesystem::path temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return Core::Result<void>::Failure(ProjectError(Core::ErrorCode::IoFailure, "SaveProjectDescriptorFile",
                                                            "Could not open temporary project descriptor",
                                                            temporary.generic_string()));
        }
        auto saved = Save(descriptor, output);
        output.flush();
        const bool stream_ok = static_cast<bool>(output);
        output.close();
        if (!saved || !stream_ok)
        {
            Detail::RemoveTemporaryFile(temporary);
            if (!saved)
                return saved;
            return Core::Result<void>::Failure(ProjectError(Core::ErrorCode::IoFailure, "SaveProjectDescriptorFile",
                                                            "Could not flush project descriptor",
                                                            temporary.generic_string()));
        }
        auto replaced = Detail::ReplaceFileAtomically(temporary, path);
        if (!replaced)
            Detail::RemoveTemporaryFile(temporary);
        return replaced;
    }
} // namespace GE::Asset
