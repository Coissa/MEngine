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

        bool HasReflectedFunctions(
            const ClangHeaderScanOutput& scan_output)
        {
            for (const ClangRecordDeclaration& record : scan_output.records)
            {
                if (!record.functions.empty())
                {
                    return true;
                }
            }

            return false;
        }

        std::string RenderHeader(
            const ClangHeaderScanOutput& scan_output,
            std::string_view source_header_include,
            std::string_view registration_function_name)
        {
            const bool has_functions = HasReflectedFunctions(scan_output);
            std::ostringstream output;
            output << "#pragma once\n\n"
                   << "#include \""
                   << EscapeCppStringLiteral(source_header_include)
                   << "\"\n\n"
                   << "#include \"TypeRegistry.h\"\n"
                   << "#include \"TypeTraits.h\"\n";
            if (has_functions)
            {
                output << "#include \"ValueView.h\"\n";
            }
            output << "\n";
            if (has_functions)
            {
                output << "#include <span>\n";
            }
            output << "#include <string_view>\n\n"
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
                       << EscapeCppStringLiteral(record.stable_name.empty()
                              ? record.qualified_name : record.stable_name)
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

                for (std::size_t function_index = 0;
                     function_index < record.functions.size();
                     ++function_index)
                {
                    const ClangFunctionDeclaration& function =
                        record.functions[function_index];
                    output << "        static void InvokeFunction"
                           << function_index
                           << (function.is_const ? "Const" : "Mutable")
                           << "(\n"
                           << (function.is_const
                                   ? "            const void* owner,\n"
                                   : "            void* owner,\n")
                           << "            std::span<const ConstValueView> arguments,\n"
                           << "            void* return_storage);\n";
                }

                output << "    };\n\n";
            }

            for (const ClangEnumDeclaration& enum_declaration : scan_output.enums)
            {
                output << "    template <>\n"
                       << "    struct TypeTraits<"
                       << GlobalTypeName(enum_declaration.qualified_name) << ">\n"
                       << "    {\n"
                       << "        static constexpr std::string_view StableName =\n"
                       << "            \""
                       << EscapeCppStringLiteral(enum_declaration.stable_name.empty()
                              ? enum_declaration.qualified_name
                              : enum_declaration.stable_name)
                       << "\";\n"
                       << "    };\n\n";
            }

            output << "    [[nodiscard]]\n"
                   << "    Core::Result<void> "
                   << registration_function_name << "(\n"
                   << "        TypeRegistry& registry);\n"
                   << "}\n";
            return output.str();
        }

        void RenderFunctionCallArguments(
            std::ostringstream& output,
            const ClangFunctionDeclaration& function)
        {
            for (std::size_t parameter_index = 0;
                 parameter_index < function.parameters.size();
                 ++parameter_index)
            {
                if (parameter_index != 0)
                {
                    output << ",\n";
                }

                output << "                *static_cast<const std::remove_cvref_t<"
                       << function.parameters[parameter_index].type_spelling
                       << ">*>(arguments[" << parameter_index
                       << "].Data())";
            }
        }

        void RenderFunctionInvokers(
            std::ostringstream& output,
            const ClangRecordDeclaration& record)
        {
            const std::string type_name = GlobalTypeName(record.qualified_name);

            for (std::size_t function_index = 0;
                 function_index < record.functions.size();
                 ++function_index)
            {
                const ClangFunctionDeclaration& function =
                    record.functions[function_index];
                output << "    void GeneratedTypeAccess<" << type_name
                       << ">::InvokeFunction" << function_index
                       << (function.is_const ? "Const" : "Mutable")
                       << "(\n"
                       << (function.is_const
                               ? "        const void* owner,\n"
                               : "        void* owner,\n")
                       << "        std::span<const ConstValueView> arguments,\n"
                       << "        void* return_storage)\n"
                       << "    {\n";

                if (function.parameters.empty())
                {
                    output << "        static_cast<void>(arguments);\n";
                }
                if (function.returns_void)
                {
                    output << "        static_cast<void>(return_storage);\n"
                           << "        static_cast<"
                           << (function.is_const ? "const " : "")
                           << type_name << "*>(owner)->" << function.name
                           << "(";
                }
                else
                {
                    output << "        *static_cast<std::remove_cvref_t<"
                           << function.return_type_spelling
                           << ">*>(return_storage) =\n"
                           << "            static_cast<"
                           << (function.is_const ? "const " : "")
                           << type_name << "*>(owner)->" << function.name
                           << "(";
                }

                if (!function.parameters.empty())
                {
                    output << "\n";
                    RenderFunctionCallArguments(output, function);
                    output << ");\n";
                }
                else
                {
                    output << ");\n";
                }
                output << "    }\n\n";
            }
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
                       << "                ";
                bool has_flag = false;
                const auto render_flag = [&](std::string_view name)
                {
                    if (has_flag) output << " | ";
                    output << "FieldInfo::FieldFlags::" << name;
                    has_flag = true;
                };
                if (field.is_const) render_flag("ReadOnly");
                if (field.is_transient) render_flag("Transient");
                if (field.is_required) render_flag("Required");
                if (!has_flag) render_flag("None");
                output << ",\n";

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
                       << ">::GetField" << field_index << "Const,\n"
                       << "                \""
                       << EscapeCppStringLiteral(field.serialized_name)
                       << "\");\n";
            }

            if (!record.functions.empty())
            {
                output << "            std::vector<FunctionInfo> functions;\n"
                       << "            functions.reserve("
                       << record.functions.size() << ");\n";
            }

            for (std::size_t function_index = 0;
                 function_index < record.functions.size();
                 ++function_index)
            {
                const ClangFunctionDeclaration& function =
                    record.functions[function_index];
                output << "            {\n"
                       << "                std::vector<ParameterInfo> parameters;\n"
                       << "                parameters.reserve("
                       << function.parameters.size() << ");\n";
                for (const ClangParameterDeclaration& parameter :
                     function.parameters)
                {
                    output << "                parameters.emplace_back(\n"
                           << "                    GetTypeId<"
                           << parameter.type_spelling << ">(),\n"
                           << "                    \""
                           << EscapeCppStringLiteral(parameter.name)
                           << "\");\n";
                }

                output << "                functions.emplace_back(\n"
                       << "                    owner_type_id,\n"
                       << "                    GetTypeId<"
                       << function.return_type_spelling << ">(),\n"
                       << "                    std::move(parameters),\n"
                       << "                    \""
                       << EscapeCppStringLiteral(function.name) << "\",\n"
                       << "                    FunctionInfo::FunctionFlags::"
                       << (function.is_const ? "Const" : "None") << ",\n";
                if (function.is_const)
                {
                    output << "                    nullptr,\n"
                           << "                    &GeneratedTypeAccess<"
                           << type_name << ">::InvokeFunction"
                           << function_index << "Const);\n";
                }
                else
                {
                    output << "                    &GeneratedTypeAccess<"
                           << type_name << ">::InvokeFunction"
                           << function_index << "Mutable,\n"
                           << "                    nullptr);\n";
                }
                output << "            }\n";
            }

            output << "\n"
                   << "            Core::Result<void> result =\n"
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
                   << "                    std::move(fields)";
            output << ",\n";
            if (!record.functions.empty())
                output << "                    std::move(functions),\n";
            else
                output << "                    {},\n";
            output << "                    TypeFlags::"
                   << (record.is_serializable ? "Serializable" : "None")
                   << ",\n"
                   << "                    MakeTypeOperations<" << type_name
                   << ">()));\n"
                   << "            if (!result) return result;\n"
                   << "        }\n";
        }

        void RenderEnumRegistration(
            std::ostringstream& output,
            const ClangEnumDeclaration& enum_declaration)
        {
            const std::string type_name =
                GlobalTypeName(enum_declaration.qualified_name);
            output << "        {\n"
                   << "            EnumInfo enum_info;\n"
                   << "            enum_info.underlying_type_id = GetTypeId<"
                   << enum_declaration.underlying_type_spelling << ">();\n"
                   << "            enum_info.values = {";
            for (std::size_t index = 0; index < enum_declaration.values.size(); ++index)
            {
                if (index != 0) output << ", ";
                output << "{\""
                       << EscapeCppStringLiteral(enum_declaration.values[index].name)
                       << "\", " << enum_declaration.values[index].raw_value << "ull}";
            }
            output << "};\n"
                   << "            enum_info.read_raw_value = &ReadEnumRaw<"
                   << type_name << ">;\n"
                   << "            enum_info.write_raw_value = &WriteEnumRaw<"
                   << type_name << ">;\n"
                   << "            Core::Result<void> result = registry.RegisterType(TypeInfo(\n"
                   << "                GetTypeId<" << type_name << ">(), TypeKind::Enum,\n"
                   << "                std::string(GetStableTypeName<" << type_name << ">()),\n"
                   << "                sizeof(" << type_name << "), alignof(" << type_name
                   << "), {}, {}, TypeFlags::None,\n"
                   << "                MakeTypeOperations<" << type_name
                   << ">(), std::move(enum_info)));\n"
                   << "            if (!result) return result;\n"
                   << "        }\n";
        }

        std::string RenderSource(
            const ClangHeaderScanOutput& scan_output,
            std::string_view generated_header_include,
            std::string_view registration_function_name)
        {
            const bool has_functions = HasReflectedFunctions(scan_output);
            std::ostringstream output;
            output << "#include \""
                   << EscapeCppStringLiteral(generated_header_include)
                   << "\"\n\n"
                   << "#include \"FieldInfo.h\"\n";
            if (has_functions)
            {
                output << "#include \"FunctionInfo.h\"\n"
                       << "#include \"ParameterInfo.h\"\n";
            }
            output << "#include \"TypeInfo.h\"\n\n"
                   << "#include <limits>\n"
                   << "#include <memory>\n"
                   << "#include <new>\n"
                   << "#include <string>\n";
            if (has_functions)
            {
                output << "#include <type_traits>\n";
            }
            output << "#include <type_traits>\n"
                   << "#include <utility>\n"
                   << "#include <vector>\n\n"
                   << "namespace GE::Reflection\n"
                   << "{\n"
                   << "    namespace\n"
                   << "    {\n"
                   << "        template<typename T>\n"
                   << "        void DefaultConstruct(void* storage)\n"
                   << "        { std::construct_at(static_cast<T*>(storage)); }\n"
                   << "        template<typename T>\n"
                   << "        void Destroy(void* object) noexcept\n"
                   << "        { std::destroy_at(static_cast<T*>(object)); }\n"
                   << "        template<typename T>\n"
                   << "        void CopyAssign(void* target, const void* source)\n"
                   << "        { *static_cast<T*>(target) = *static_cast<const T*>(source); }\n"
                   << "        template<typename T>\n"
                   << "        TypeOperations MakeTypeOperations() noexcept\n"
                   << "        {\n"
                   << "            if constexpr (std::is_default_constructible_v<T>\n"
                   << "                && std::is_destructible_v<T>\n"
                   << "                && std::is_copy_assignable_v<T>)\n"
                   << "                return {&DefaultConstruct<T>, &Destroy<T>, &CopyAssign<T>};\n"
                   << "            return {};\n"
                   << "        }\n"
                   << "        template<typename T>\n"
                   << "        std::uint64_t ReadEnumRaw(const void* value) noexcept\n"
                   << "        {\n"
                   << "            using U = std::make_unsigned_t<std::underlying_type_t<T>>;\n"
                   << "            return static_cast<std::uint64_t>(\n"
                   << "                static_cast<U>(*static_cast<const T*>(value)));\n"
                   << "        }\n"
                   << "        template<typename T>\n"
                   << "        bool WriteEnumRaw(void* value, std::uint64_t raw) noexcept\n"
                   << "        {\n"
                   << "            using U = std::make_unsigned_t<std::underlying_type_t<T>>;\n"
                   << "            if (raw > static_cast<std::uint64_t>((std::numeric_limits<U>::max)()))\n"
                   << "                return false;\n"
                   << "            *static_cast<T*>(value) = static_cast<T>(static_cast<U>(raw));\n"
                   << "            return true;\n"
                   << "        }\n"
                   << "    }\n\n";

            for (const ClangRecordDeclaration& record : scan_output.records)
            {
                RenderAccessors(output, record);
                RenderFunctionInvokers(output, record);
            }

            output << "    Core::Result<void> "
                   << registration_function_name << "(\n"
                   << "        TypeRegistry& registry)\n"
                   << "    {\n";

            if (scan_output.records.empty() && scan_output.enums.empty())
            {
                output << "        static_cast<void>(registry);\n";
            }

            for (const ClangRecordDeclaration& record : scan_output.records)
            {
                RenderTypeRegistration(output, record);
            }

            for (const ClangEnumDeclaration& enum_declaration : scan_output.enums)
            {
                RenderEnumRegistration(output, enum_declaration);
            }

            output << "\n"
                   << "        return Core::Result<void>::Success();\n"
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
