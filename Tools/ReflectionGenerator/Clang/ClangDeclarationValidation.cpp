#include "ClangDeclarationValidation.h"

#include <unordered_set>

namespace GE::Reflection::Generator
{
    namespace
    {
        ClangDeclarationValidationError MakeRecordError(ClangDeclarationValidationErrorCode code,
                                                        const ClangHeaderScanOutput& scan_output,
                                                        const ClangRecordDeclaration& record)
        {
            return ClangDeclarationValidationError{code,
                                                   scan_output.module_name,
                                                   scan_output.header_path,
                                                   record.qualified_name,
                                                   {},
                                                   record.source_file,
                                                   record.line,
                                                   record.column};
        }

        ClangDeclarationValidationError MakeFieldError(ClangDeclarationValidationErrorCode code,
                                                       const ClangHeaderScanOutput& scan_output,
                                                       const ClangRecordDeclaration& record,
                                                       const ClangFieldDeclaration& field)
        {
            return ClangDeclarationValidationError{code,
                                                   scan_output.module_name,
                                                   scan_output.header_path,
                                                   record.qualified_name,
                                                   field.name,
                                                   field.source_file,
                                                   field.line,
                                                   field.column,
                                                   {},
                                                   {}};
        }

        ClangDeclarationValidationError MakeFunctionError(ClangDeclarationValidationErrorCode code,
                                                          const ClangHeaderScanOutput& scan_output,
                                                          const ClangRecordDeclaration& record,
                                                          const ClangFunctionDeclaration& function)
        {
            return ClangDeclarationValidationError{code,
                                                   scan_output.module_name,
                                                   scan_output.header_path,
                                                   record.qualified_name,
                                                   {},
                                                   function.source_file,
                                                   function.line,
                                                   function.column,
                                                   function.name,
                                                   {}};
        }

        ClangDeclarationValidationError MakeParameterError(ClangDeclarationValidationErrorCode code,
                                                           const ClangHeaderScanOutput& scan_output,
                                                           const ClangRecordDeclaration& record,
                                                           const ClangFunctionDeclaration& function,
                                                           const ClangParameterDeclaration& parameter)
        {
            return ClangDeclarationValidationError{code,
                                                   scan_output.module_name,
                                                   scan_output.header_path,
                                                   record.qualified_name,
                                                   {},
                                                   parameter.source_file,
                                                   parameter.line,
                                                   parameter.column,
                                                   function.name,
                                                   parameter.name};
        }
    } // namespace

    ClangDeclarationValidationResult ValidateClangDeclarations(const ClangHeaderScanOutput& scan_output)
    {
        ClangDeclarationValidationResult errors;

        for (const ClangRecordDeclaration& record : scan_output.records)
        {
            if (record.qualified_name.empty())
            {
                errors.push_back(MakeRecordError(ClangDeclarationValidationErrorCode::EmptyRecordQualifiedName,
                                                 scan_output, record));
            }
            if (record.is_component &&
                (!record.supported_component_base || !record.is_serializable || record.serialization_version == 0))
                errors.push_back(MakeRecordError(ClangDeclarationValidationErrorCode::InvalidComponentDeclaration,
                                                 scan_output, record));
            if (record.is_serializable && !record.has_explicit_stable_name)
            {
                errors.push_back(MakeRecordError(
                    ClangDeclarationValidationErrorCode::SerializableTypeRequiresStableName, scan_output, record));
            }

            std::unordered_set<std::string> field_names;
            field_names.reserve(record.fields.size());

            for (const ClangFieldDeclaration& field : record.fields)
            {
                if (field.name.empty())
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::EmptyFieldName, scan_output,
                                                    record, field));
                }
                else if (!field_names.insert(field.name).second)
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::DuplicateFieldName,
                                                    scan_output, record, field));
                }

                if (field.type_spelling.empty())
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::EmptyFieldTypeSpelling,
                                                    scan_output, record, field));
                }

                if (field.is_bit_field)
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::UnsupportedBitField,
                                                    scan_output, record, field));
                }

                if (field.access == ClangFieldAccess::Unspecified)
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::UnspecifiedFieldAccess,
                                                    scan_output, record, field));
                }

                if (field.is_pointer)
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::UnsupportedPointerField,
                                                    scan_output, record, field));
                }
                if (field.is_reference)
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::UnsupportedReferenceField,
                                                    scan_output, record, field));
                }
                if (field.is_array)
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::UnsupportedArrayField,
                                                    scan_output, record, field));
                }
                if (record.is_serializable && !field.is_transient && field.serialized_name.empty())
                {
                    errors.push_back(
                        MakeFieldError(ClangDeclarationValidationErrorCode::PersistentFieldRequiresSerializedName,
                                       scan_output, record, field));
                }
                if (field.is_required && field.is_transient)
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::RequiredTransientField,
                                                    scan_output, record, field));
                }
                if (record.is_serializable && !field.is_transient && field.is_const)
                {
                    errors.push_back(MakeFieldError(ClangDeclarationValidationErrorCode::PersistentReadOnlyField,
                                                    scan_output, record, field));
                }
            }

            std::unordered_set<std::string> function_names;
            function_names.reserve(record.functions.size());

            for (const ClangFunctionDeclaration& function : record.functions)
            {
                if (function.name.empty())
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::EmptyFunctionName,
                                                       scan_output, record, function));
                }
                else if (!function_names.insert(function.name).second)
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::DuplicateFunctionName,
                                                       scan_output, record, function));
                }

                if (function.return_type_spelling.empty())
                {
                    errors.push_back(
                        MakeFunctionError(ClangDeclarationValidationErrorCode::EmptyFunctionReturnTypeSpelling,
                                          scan_output, record, function));
                }
                if (function.is_static)
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedStaticFunction,
                                                       scan_output, record, function));
                }
                if (function.is_variadic)
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedVariadicFunction,
                                                       scan_output, record, function));
                }
                if (function.is_deleted)
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedDeletedFunction,
                                                       scan_output, record, function));
                }
                if (function.is_volatile)
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedVolatileFunction,
                                                       scan_output, record, function));
                }
                if (function.is_ref_qualified)
                {
                    errors.push_back(
                        MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedRefQualifiedFunction,
                                          scan_output, record, function));
                }
                if (function.is_template)
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedFunctionTemplate,
                                                       scan_output, record, function));
                }
                if (function.is_special_member)
                {
                    errors.push_back(
                        MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedSpecialMemberFunction,
                                          scan_output, record, function));
                }
                if (function.is_operator)
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedOperatorFunction,
                                                       scan_output, record, function));
                }
                if (function.returns_reference)
                {
                    errors.push_back(MakeFunctionError(ClangDeclarationValidationErrorCode::UnsupportedReferenceReturn,
                                                       scan_output, record, function));
                }

                for (const ClangParameterDeclaration& parameter : function.parameters)
                {
                    if (parameter.name.empty())
                    {
                        errors.push_back(MakeParameterError(ClangDeclarationValidationErrorCode::EmptyParameterName,
                                                            scan_output, record, function, parameter));
                    }
                    if (parameter.type_spelling.empty())
                    {
                        errors.push_back(
                            MakeParameterError(ClangDeclarationValidationErrorCode::EmptyParameterTypeSpelling,
                                               scan_output, record, function, parameter));
                    }
                    if (parameter.is_rvalue_reference)
                    {
                        errors.push_back(
                            MakeParameterError(ClangDeclarationValidationErrorCode::UnsupportedRValueReferenceParameter,
                                               scan_output, record, function, parameter));
                    }
                    else if (parameter.is_lvalue_reference && !parameter.is_const)
                    {
                        errors.push_back(MakeParameterError(
                            ClangDeclarationValidationErrorCode::UnsupportedNonConstReferenceParameter, scan_output,
                            record, function, parameter));
                    }
                }
            }
        }

        return errors;
    }
} // namespace GE::Reflection::Generator
