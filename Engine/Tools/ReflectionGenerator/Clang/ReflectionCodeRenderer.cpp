#include "ReflectionCodeRenderer.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

namespace GE::Reflection::Generator
{
    namespace
    {
        constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
        constexpr std::uint64_t FnvPrime = 1099511628211ull;

        ReflectionCodeRenderResult MakeError(
            ReflectionCodeRenderErrorCode code,
            std::filesystem::path path = {},
            std::optional<GeneratedFilePlanError> file_plan_error = std::nullopt,
            ClangDeclarationValidationResult declaration_errors = {})
        {
            return ReflectionCodeRenderError{
                code,
                std::move(path),
                std::move(file_plan_error),
                std::move(declaration_errors)};
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

        std::string GlobalTypeName(std::string_view qualified_name)
        {
            if (qualified_name.starts_with("::"))
            {
                return std::string{qualified_name};
            }

            return "::" + std::string{qualified_name};
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

        std::string BuildRegistrationFunctionName(
            const HeaderScanJob& job)
        {
            const std::string identity =
                job.module_name + "\n" +
                job.module_relative_header_path.generic_string();
            std::ostringstream name;
            name << "RegisterGeneratedReflection_"
                 << std::hex
                 << std::setfill('0')
                 << std::setw(16)
                 << HashString(identity);
            return name.str();
        }

        std::string RenderHeader(
            const ClangHeaderScanOutput& scan_output,
            std::string_view source_header_include,
            std::string_view registration_function_name)
        {
            std::ostringstream output;
            output << "#pragma once\n\n"
                   << "#include \""
                   << EscapeCppStringLiteral(source_header_include)
                   << "\"\n\n"
                   << "#include \"TypeRegistry.h\"\n"
                   << "#include \"TypeTraits.h\"\n\n"
                   << "#include <string_view>\n\n"
                   << "namespace GE::Reflection\n"
                   << "{\n";

            for (const ClangRecordDeclaration& record : scan_output.records)
            {
                const std::string type_name =
                    GlobalTypeName(record.qualified_name);

                output << "    template <>\n"
                       << "    struct TypeTraits<" << type_name << ">\n"
                       << "    {\n"
                       << "        static constexpr std::string_view StableName =\n"
                       << "            \""
                       << EscapeCppStringLiteral(record.qualified_name)
                       << "\";\n"
                       << "    };\n\n"
                       << "    template <>\n"
                       << "    struct GeneratedTypeAccess<" << type_name << ">\n"
                       << "    {\n";

                for (std::size_t field_index = 0;
                     field_index < record.fields.size();
                     ++field_index)
                {
                    const ClangFieldDeclaration& field =
                        record.fields[field_index];
                    if (!field.is_const)
                    {
                        output << "        [[nodiscard]] static void* GetField"
                               << field_index
                               << "Mutable(void* owner) noexcept;\n";
                    }

                    output << "        [[nodiscard]] static const void* GetField"
                           << field_index
                           << "Const(const void* owner) noexcept;\n";
                }

                output << "    };\n\n";
            }

            output << "    [[nodiscard]]\n"
                   << "    TypeRegistry::RegisterTypeResult "
                   << registration_function_name << "(\n"
                   << "        TypeRegistry& registry);\n"
                   << "}\n";
            return output.str();
        }

        void RenderAccessors(
            std::ostringstream& output,
            const ClangRecordDeclaration& record)
        {
            const std::string type_name = GlobalTypeName(record.qualified_name);

            for (std::size_t field_index = 0;
                 field_index < record.fields.size();
                 ++field_index)
            {
                const ClangFieldDeclaration& field = record.fields[field_index];
                if (!field.is_const)
                {
                    output << "    void* GeneratedTypeAccess<" << type_name
                           << ">::GetField" << field_index
                           << "Mutable(void* owner) noexcept\n"
                           << "    {\n"
                           << "        return std::addressof(\n"
                           << "            static_cast<" << type_name
                           << "*>(owner)->" << field.name << ");\n"
                           << "    }\n\n";
                }

                output << "    const void* GeneratedTypeAccess<" << type_name
                       << ">::GetField" << field_index
                       << "Const(const void* owner) noexcept\n"
                       << "    {\n"
                       << "        return std::addressof(\n"
                       << "            static_cast<const " << type_name
                       << "*>(owner)->" << field.name << ");\n"
                       << "    }\n\n";
            }
        }

        void RenderTypeRegistration(
            std::ostringstream& output,
            const ClangRecordDeclaration& record)
        {
            const std::string type_name = GlobalTypeName(record.qualified_name);

            output << "        {\n"
                   << "            const TypeId owner_type_id = GetTypeId<"
                   << type_name << ">();\n"
                   << "            std::vector<FieldInfo> fields;\n"
                   << "            fields.reserve(" << record.fields.size()
                   << ");\n";

            for (std::size_t field_index = 0;
                 field_index < record.fields.size();
                 ++field_index)
            {
                const ClangFieldDeclaration& field = record.fields[field_index];
                output << "            fields.emplace_back(\n"
                       << "                owner_type_id,\n"
                       << "                GetTypeId<" << field.type_spelling
                       << ">(),\n"
                       << "                \""
                       << EscapeCppStringLiteral(field.name) << "\",\n"
                       << "                FieldInfo::FieldFlags::"
                       << (field.is_const ? "ReadOnly" : "None") << ",\n";

                if (field.is_const)
                {
                    output << "                nullptr,\n";
                }
                else
                {
                    output << "                &GeneratedTypeAccess<"
                           << type_name << ">::GetField" << field_index
                           << "Mutable,\n";
                }

                output << "                &GeneratedTypeAccess<" << type_name
                       << ">::GetField" << field_index << "Const);\n";
            }

            output << "\n"
                   << "            const TypeRegistry::RegisterTypeResult result =\n"
                   << "                registry.RegisterType(TypeInfo(\n"
                   << "                    owner_type_id,\n"
                   << "                    TypeKind::"
                   << (record.kind == ClangRecordDeclarationKind::Class
                           ? "Class"
                           : "Struct")
                   << ",\n"
                   << "                    std::string(GetStableTypeName<"
                   << type_name << ">()),\n"
                   << "                    sizeof(" << type_name << "),\n"
                   << "                    alignof(" << type_name << "),\n"
                   << "                    std::move(fields)));\n"
                   << "            if (result != TypeRegistry::RegisterTypeResult::Success)\n"
                   << "            {\n"
                   << "                return result;\n"
                   << "            }\n"
                   << "        }\n";
        }

        std::string RenderSource(
            const ClangHeaderScanOutput& scan_output,
            std::string_view generated_header_include,
            std::string_view registration_function_name)
        {
            std::ostringstream output;
            output << "#include \""
                   << EscapeCppStringLiteral(generated_header_include)
                   << "\"\n\n"
                   << "#include \"FieldInfo.h\"\n"
                   << "#include \"TypeInfo.h\"\n\n"
                   << "#include <memory>\n"
                   << "#include <string>\n"
                   << "#include <utility>\n"
                   << "#include <vector>\n\n"
                   << "namespace GE::Reflection\n"
                   << "{\n";

            for (const ClangRecordDeclaration& record : scan_output.records)
            {
                RenderAccessors(output, record);
            }

            output << "    TypeRegistry::RegisterTypeResult "
                   << registration_function_name << "(\n"
                   << "        TypeRegistry& registry)\n"
                   << "    {\n";

            if (scan_output.records.empty())
            {
                output << "        static_cast<void>(registry);\n";
            }

            for (const ClangRecordDeclaration& record : scan_output.records)
            {
                RenderTypeRegistration(output, record);
            }

            output << "\n"
                   << "        return TypeRegistry::RegisterTypeResult::Success;\n"
                   << "    }\n"
                   << "}\n";
            return output.str();
        }
    }

    ReflectionCodeRenderResult RenderReflectionCode(
        const HeaderScanJob& job,
        const ClangHeaderScanOutput& scan_output,
        const GeneratedFilePlan& file_plan)
    {
        if (scan_output.module_name != job.module_name)
        {
            return MakeError(
                ReflectionCodeRenderErrorCode::ModuleNameMismatch,
                scan_output.header_path);
        }

        if (scan_output.header_path.lexically_normal()
            != job.header_path.lexically_normal())
        {
            return MakeError(
                ReflectionCodeRenderErrorCode::ScanHeaderPathMismatch,
                scan_output.header_path);
        }

        const GeneratedFilePlanResult expected_plan_result =
            BuildGeneratedFilePlan(job);
        const auto* expected_plan =
            std::get_if<GeneratedFilePlan>(&expected_plan_result);
        if (expected_plan == nullptr)
        {
            return MakeError(
                ReflectionCodeRenderErrorCode::FilePlanningFailed,
                job.header_path,
                std::get<GeneratedFilePlanError>(expected_plan_result));
        }

        if (file_plan.source_header_path.lexically_normal()
                != expected_plan->source_header_path.lexically_normal()
            || file_plan.generated_header_path.lexically_normal()
                != expected_plan->generated_header_path.lexically_normal()
            || file_plan.generated_source_path.lexically_normal()
                != expected_plan->generated_source_path.lexically_normal())
        {
            return MakeError(
                ReflectionCodeRenderErrorCode::FilePlanMismatch,
                file_plan.source_header_path);
        }

        ClangDeclarationValidationResult declaration_errors =
            ValidateClangDeclarations(scan_output);
        if (!declaration_errors.empty())
        {
            return MakeError(
                ReflectionCodeRenderErrorCode::DeclarationValidationFailed,
                scan_output.header_path,
                std::nullopt,
                std::move(declaration_errors));
        }

        const std::filesystem::path source_header_include =
            file_plan.source_header_path.lexically_relative(
                file_plan.generated_header_path.parent_path());
        if (source_header_include.empty()
            || source_header_include.is_absolute())
        {
            return MakeError(
                ReflectionCodeRenderErrorCode::
                    SourceHeaderIncludePathUnavailable,
                file_plan.source_header_path);
        }

        const std::filesystem::path generated_header_include =
            file_plan.generated_header_path.lexically_relative(
                file_plan.generated_source_path.parent_path());
        if (generated_header_include.empty()
            || generated_header_include.is_absolute())
        {
            return MakeError(
                ReflectionCodeRenderErrorCode::
                    GeneratedHeaderIncludePathUnavailable,
                file_plan.generated_header_path);
        }

        const std::string registration_function_name =
            BuildRegistrationFunctionName(job);
        return ReflectionCodeRenderOutput{
            GeneratedCodeFile{
                file_plan.generated_header_path,
                RenderHeader(
                    scan_output,
                    source_header_include.generic_string(),
                    registration_function_name)},
            GeneratedCodeFile{
                file_plan.generated_source_path,
                RenderSource(
                    scan_output,
                    generated_header_include.generic_string(),
                    registration_function_name)},
            registration_function_name};
    }
}
