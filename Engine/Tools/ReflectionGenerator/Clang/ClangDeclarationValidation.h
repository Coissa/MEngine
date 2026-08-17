#pragma once

#include "ClangHeaderScanner.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace GE::Reflection::Generator
{
    enum class ClangDeclarationValidationErrorCode
    {
        EmptyRecordQualifiedName,
        EmptyFieldName,
        EmptyFieldTypeSpelling,
        UnsupportedBitField,
        UnspecifiedFieldAccess,
        DuplicateFieldName,
        EmptyFunctionName,
        EmptyFunctionReturnTypeSpelling,
        DuplicateFunctionName,
        UnsupportedStaticFunction,
        UnsupportedVariadicFunction,
        UnsupportedDeletedFunction,
        UnsupportedVolatileFunction,
        UnsupportedRefQualifiedFunction,
        UnsupportedFunctionTemplate,
        UnsupportedSpecialMemberFunction,
        UnsupportedOperatorFunction,
        UnsupportedReferenceReturn,
        EmptyParameterName,
        EmptyParameterTypeSpelling,
        UnsupportedNonConstReferenceParameter,
        UnsupportedRValueReferenceParameter
    };

    struct ClangDeclarationValidationError
    {
        ClangDeclarationValidationErrorCode code;
        std::string module_name;
        std::filesystem::path header_path;
        std::string record_qualified_name;
        std::string field_name;
        std::filesystem::path source_file;
        std::uint32_t line;
        std::uint32_t column;
        std::string function_name;
        std::string parameter_name;
    };

    using ClangDeclarationValidationResult =
        std::vector<ClangDeclarationValidationError>;

    [[nodiscard]]
    ClangDeclarationValidationResult ValidateClangDeclarations(
        const ClangHeaderScanOutput& scan_output);
}
