#pragma once

#include "ModuleManifest.h"

#include <filesystem>
#include <string>
#include <variant>

namespace GE::Reflection::Generator
{
    enum class ModuleManifestLoadErrorCode
    {
        FileCannotBeOpened,
        FileReadError,
        DocumentParseError,
        InvalidField,
        ValidationFailed
    };

    struct ModuleManifestLoadError
    {
        ModuleManifestLoadErrorCode error_code;
        std::string error_message;
    };

    using ModuleManifestLoadResult = std::variant<ModuleManifest, ModuleManifestLoadError>;
    [[nodiscard]] ModuleManifestLoadResult LoadModuleManifest(const std::filesystem::path& manifest_path);
}