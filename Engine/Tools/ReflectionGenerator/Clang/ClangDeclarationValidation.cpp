#include "ClangDeclarationValidation.h"

#include <unordered_set>

namespace GE::Reflection::Generator
{
    namespace
    {
        ClangDeclarationValidationError MakeRecordError(
            ClangDeclarationValidationErrorCode code,
            const ClangHeaderScanOutput& scan_output,
            const ClangRecordDeclaration& record)
        {
            return ClangDeclarationValidationError{
                code,
                scan_output.module_name,
                scan_output.header_path,
                record.qualified_name,
                {},
                record.source_file,
                record.line,
                record.column};
        }

        ClangDeclarationValidationError MakeFieldError(
            ClangDeclarationValidationErrorCode code,
            const ClangHeaderScanOutput& scan_output,
            const ClangRecordDeclaration& record,
            const ClangFieldDeclaration& field)
        {
            return ClangDeclarationValidationError{
                code,
                scan_output.module_name,
                scan_output.header_path,
                record.qualified_name,
                field.name,
                field.source_file,
                field.line,
                field.column};
        }
    }

    ClangDeclarationValidationResult ValidateClangDeclarations(
        const ClangHeaderScanOutput& scan_output)
    {
        ClangDeclarationValidationResult errors;

        for (const ClangRecordDeclaration& record : scan_output.records)
        {
            if (record.qualified_name.empty())
            {
                errors.push_back(MakeRecordError(
                    ClangDeclarationValidationErrorCode::
                        EmptyRecordQualifiedName,
                    scan_output,
                    record));
            }

            std::unordered_set<std::string> field_names;
            field_names.reserve(record.fields.size());

            for (const ClangFieldDeclaration& field : record.fields)
            {
                if (field.name.empty())
                {
                    errors.push_back(MakeFieldError(
                        ClangDeclarationValidationErrorCode::EmptyFieldName,
                        scan_output,
                        record,
                        field));
                }
                else if (!field_names.insert(field.name).second)
                {
                    errors.push_back(MakeFieldError(
                        ClangDeclarationValidationErrorCode::
                            DuplicateFieldName,
                        scan_output,
                        record,
                        field));
                }

                if (field.type_spelling.empty())
                {
                    errors.push_back(MakeFieldError(
                        ClangDeclarationValidationErrorCode::
                            EmptyFieldTypeSpelling,
                        scan_output,
                        record,
                        field));
                }

                if (field.is_bit_field)
                {
                    errors.push_back(MakeFieldError(
                        ClangDeclarationValidationErrorCode::
                            UnsupportedBitField,
                        scan_output,
                        record,
                        field));
                }

                if (field.access == ClangFieldAccess::Unspecified)
                {
                    errors.push_back(MakeFieldError(
                        ClangDeclarationValidationErrorCode::
                            UnspecifiedFieldAccess,
                        scan_output,
                        record,
                        field));
                }
            }
        }

        return errors;
    }
}
