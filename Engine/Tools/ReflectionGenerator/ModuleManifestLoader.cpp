#include "ModuleManifestLoader.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace GE::Reflection::Generator
{
    namespace
    {
        ModuleManifestLoadResult MakeError(
            ModuleManifestLoadErrorCode code,
            std::string message)
        {
            return ModuleManifestLoadError{
                code,
                std::move(message)
            };
        }

        const char* ToString(ModuleManifestValidationResult result)
        {
            switch (result)
            {
            case ModuleManifestValidationResult::Valid:
                return "Valid";
            case ModuleManifestValidationResult::EmptyModuleName:
                return "EmptyModuleName";
            case ModuleManifestValidationResult::EmptyModuleRoot:
                return "EmptyModuleRoot";
            case ModuleManifestValidationResult::ModuleRootNotRelative:
                return "ModuleRootNotRelative";
            case ModuleManifestValidationResult::EmptyOutputDirectory:
                return "EmptyOutputDirectory";
            case ModuleManifestValidationResult::OutputDirectoryNotRelative:
                return "OutputDirectoryNotRelative";
            case ModuleManifestValidationResult::
                    OutputDirectoryContainsParentTraversal:
                return "OutputDirectoryContainsParentTraversal";
            case ModuleManifestValidationResult::EmptyHeaderFiles:
                return "EmptyHeaderFiles";
            case ModuleManifestValidationResult::EmptyHeaderFile:
                return "EmptyHeaderFile";
            case ModuleManifestValidationResult::HeaderFileNotRelative:
                return "HeaderFileNotRelative";
            case ModuleManifestValidationResult::
                    HeaderFileContainsParentTraversal:
                return "HeaderFileContainsParentTraversal";
            case ModuleManifestValidationResult::EmptyCompileCommandSource:
                return "EmptyCompileCommandSource";
            case ModuleManifestValidationResult::
                    CompileCommandSourceNotRelative:
                return "CompileCommandSourceNotRelative";
            case ModuleManifestValidationResult::
                    CompileCommandSourceContainsParentTraversal:
                return "CompileCommandSourceContainsParentTraversal";
            case ModuleManifestValidationResult::DuplicateHeaderFile:
                return "DuplicateHeaderFile";
            }

            return "UnknownValidationResult";
        }

        bool HasStringField(
            const nlohmann::json& document,
            std::string_view field_name)
        {
            const auto field = document.find(field_name);
            return field != document.end() && field->is_string();
        }

        std::filesystem::path Utf8Path(const std::string& value)
        {
            return std::filesystem::u8path(value);
        }
    }

    ModuleManifestLoadResult LoadModuleManifest(
        const std::filesystem::path& manifest_path)
    {
        std::ifstream input(manifest_path, std::ios::binary);
        if (!input.is_open())
        {
            return MakeError(
                ModuleManifestLoadErrorCode::FileCannotBeOpened,
                "Cannot open module manifest file");
        }

        const std::string contents{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}
        };

        if (input.bad())
        {
            return MakeError(
                ModuleManifestLoadErrorCode::FileReadError,
                "Failed while reading module manifest file");
        }

        nlohmann::json document;
        try
        {
            document = nlohmann::json::parse(contents);
        }
        catch (const nlohmann::json::parse_error& error)
        {
            return MakeError(
                ModuleManifestLoadErrorCode::DocumentParseError,
                error.what());
        }

        if (!document.is_object())
        {
            return MakeError(
                ModuleManifestLoadErrorCode::InvalidField,
                "Manifest root must be a JSON object");
        }

        if (!HasStringField(document, "module_name"))
        {
            return MakeError(
                ModuleManifestLoadErrorCode::InvalidField,
                "Field 'module_name' must exist and be a string");
        }

        if (!HasStringField(document, "module_root"))
        {
            return MakeError(
                ModuleManifestLoadErrorCode::InvalidField,
                "Field 'module_root' must exist and be a string");
        }

        if (!HasStringField(document, "output_directory"))
        {
            return MakeError(
                ModuleManifestLoadErrorCode::InvalidField,
                "Field 'output_directory' must exist and be a string");
        }

        if (!HasStringField(document, "compile_command_source"))
        {
            return MakeError(
                ModuleManifestLoadErrorCode::InvalidField,
                "Field 'compile_command_source' must exist and be a string");
        }

        const auto header_files_field = document.find("header_files");
        if (header_files_field == document.end() ||
            !header_files_field->is_array())
        {
            return MakeError(
                ModuleManifestLoadErrorCode::InvalidField,
                "Field 'header_files' must exist and be an array");
        }

        ModuleManifest manifest;
        manifest.module_name = document["module_name"].get<std::string>();
        manifest.module_root = Utf8Path(
            document["module_root"].get<std::string>());
        manifest.output_directory = Utf8Path(
            document["output_directory"].get<std::string>());
        manifest.compile_command_source = Utf8Path(
            document["compile_command_source"].get<std::string>());
        manifest.header_files.reserve(header_files_field->size());

        for (const nlohmann::json& header_file : *header_files_field)
        {
            if (!header_file.is_string())
            {
                return MakeError(
                    ModuleManifestLoadErrorCode::InvalidField,
                    "Every element of 'header_files' must be a string");
            }

            manifest.header_files.push_back(
                Utf8Path(header_file.get<std::string>()));
        }

        const ModuleManifestValidationResult validation_result =
            ValidateModuleManifest(manifest);
        if (validation_result != ModuleManifestValidationResult::Valid)
        {
            return MakeError(
                ModuleManifestLoadErrorCode::ValidationFailed,
                std::string{"Module manifest validation failed: "} +
                    ToString(validation_result));
        }

        return manifest;
    }
}
