#include "ReflectionModuleRenderer.h"

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace GE::Reflection::Generator
{
    namespace
    {
        constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
        constexpr std::uint64_t FnvPrime = 1099511628211ull;

        ReflectionModuleRenderResult MakeError(
            ReflectionModuleRenderErrorCode code,
            const ReflectionModuleRenderInput& input,
            std::filesystem::path path = {},
            std::string registration_function_name = {})
        {
            return ReflectionModuleRenderError{
                code,
                input.module_name,
                std::move(path),
                std::move(registration_function_name)};
        }

        bool IsSafeModuleName(std::string_view module_name)
        {
            for (const unsigned char character : module_name)
            {
                if (!std::isalnum(character)
                    && character != '_'
                    && character != '-'
                    && character != '.')
                {
                    return false;
                }
            }
            return true;
        }

        bool ContainsParentTraversal(const std::filesystem::path& path)
        {
            for (const std::filesystem::path& component : path)
            {
                if (component == "..")
                {
                    return true;
                }
            }
            return false;
        }

        std::string EscapeCppStringLiteral(std::string_view value)
        {
            std::string escaped;
            escaped.reserve(value.size());
            for (const char character : value)
            {
                switch (character)
                {
                case '\\':
                    escaped += "\\\\";
                    break;
                case '"':
                    escaped += "\\\"";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                default:
                    escaped += character;
                    break;
                }
            }
            return escaped;
        }

        std::uint64_t HashString(std::string_view value)
        {
            std::uint64_t hash = FnvOffsetBasis;
            for (const unsigned char byte : value)
            {
                hash ^= byte;
                hash *= FnvPrime;
            }
            return hash;
        }

        std::string BuildModuleRegistrationFunctionName(
            std::string_view module_name)
        {
            std::ostringstream output;
            output << "RegisterGeneratedReflectionModule_"
                   << std::hex
                   << std::setfill('0')
                   << std::setw(16)
                   << HashString(module_name);
            return output.str();
        }
    }

    ReflectionModuleRenderResult RenderReflectionModuleRegistration(
        const ReflectionModuleRenderInput& input)
    {
        if (input.module_name.empty())
        {
            return MakeError(
                ReflectionModuleRenderErrorCode::EmptyModuleName,
                input);
        }
        if (!IsSafeModuleName(input.module_name))
        {
            return MakeError(
                ReflectionModuleRenderErrorCode::InvalidModuleName,
                input);
        }
        if (!input.output_directory.is_absolute())
        {
            return MakeError(
                ReflectionModuleRenderErrorCode::
                    OutputDirectoryNotAbsolute,
                input,
                input.output_directory);
        }
        if (input.header_registrations.empty())
        {
            return MakeError(
                ReflectionModuleRenderErrorCode::
                    EmptyHeaderRegistrations,
                input);
        }

        std::unordered_set<std::filesystem::path> header_paths;
        std::unordered_set<std::string> function_names;
        std::vector<std::filesystem::path> relative_header_paths;
        relative_header_paths.reserve(input.header_registrations.size());

        for (const ReflectionHeaderRegistration& registration :
             input.header_registrations)
        {
            if (!registration.generated_header_path.is_absolute())
            {
                return MakeError(
                    ReflectionModuleRenderErrorCode::
                        GeneratedHeaderPathNotAbsolute,
                    input,
                    registration.generated_header_path,
                    registration.registration_function_name);
            }

            const std::filesystem::path normalized_header_path =
                registration.generated_header_path.lexically_normal();
            const std::filesystem::path relative_header_path =
                normalized_header_path.lexically_relative(
                    input.output_directory.lexically_normal());
            if (relative_header_path.empty()
                || relative_header_path.is_absolute()
                || ContainsParentTraversal(relative_header_path))
            {
                return MakeError(
                    ReflectionModuleRenderErrorCode::
                        GeneratedHeaderOutsideOutputDirectory,
                    input,
                    normalized_header_path,
                    registration.registration_function_name);
            }
            if (registration.registration_function_name.empty())
            {
                return MakeError(
                    ReflectionModuleRenderErrorCode::
                        EmptyRegistrationFunctionName,
                    input,
                    normalized_header_path);
            }
            if (!header_paths.insert(normalized_header_path).second)
            {
                return MakeError(
                    ReflectionModuleRenderErrorCode::
                        DuplicateGeneratedHeaderPath,
                    input,
                    normalized_header_path,
                    registration.registration_function_name);
            }
            if (!function_names.insert(
                    registration.registration_function_name).second)
            {
                return MakeError(
                    ReflectionModuleRenderErrorCode::
                        DuplicateRegistrationFunctionName,
                    input,
                    normalized_header_path,
                    registration.registration_function_name);
            }

            relative_header_paths.push_back(relative_header_path);
        }

        const std::string module_function_name =
            BuildModuleRegistrationFunctionName(input.module_name);
        const std::filesystem::path generated_header_path =
            input.output_directory /
            (input.module_name + ".reflection.module.generated.h");
        const std::filesystem::path generated_source_path =
            input.output_directory /
            (input.module_name + ".reflection.module.generated.cpp");

        std::ostringstream header;
        header << "#pragma once\n\n"
               << "#include \"TypeRegistry.h\"\n\n"
               << "namespace GE::Reflection\n"
               << "{\n"
               << "    [[nodiscard]]\n"
               << "    TypeRegistry::RegisterTypeResult "
               << module_function_name << "(\n"
               << "        TypeRegistry& registry);\n"
               << "}\n";

        std::ostringstream source;
        source << "#include \""
               << EscapeCppStringLiteral(
                      generated_header_path.filename().generic_string())
               << "\"\n";
        for (const std::filesystem::path& relative_header_path :
             relative_header_paths)
        {
            source << "#include \""
                   << EscapeCppStringLiteral(
                          relative_header_path.generic_string())
                   << "\"\n";
        }
        source << "\n"
               << "namespace GE::Reflection\n"
               << "{\n"
               << "    TypeRegistry::RegisterTypeResult "
               << module_function_name << "(\n"
               << "        TypeRegistry& registry)\n"
               << "    {\n";

        for (const ReflectionHeaderRegistration& registration :
             input.header_registrations)
        {
            source << "        {\n"
                   << "            const TypeRegistry::RegisterTypeResult result =\n"
                   << "                "
                   << registration.registration_function_name
                   << "(registry);\n"
                   << "            if (result != TypeRegistry::RegisterTypeResult::Success)\n"
                   << "            {\n"
                   << "                return result;\n"
                   << "            }\n"
                   << "        }\n";
        }

        source << "\n"
               << "        return TypeRegistry::RegisterTypeResult::Success;\n"
               << "    }\n"
               << "}\n";

        return ReflectionModuleRenderOutput{
            GeneratedCodeFile{
                generated_header_path.lexically_normal(),
                header.str()},
            GeneratedCodeFile{
                generated_source_path.lexically_normal(),
                source.str()},
            module_function_name};
    }
}
