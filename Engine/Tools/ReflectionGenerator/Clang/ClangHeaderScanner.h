#pragma once

#include "HeaderParseInvocation.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace GE::Reflection::Generator
{
    enum class ClangDiagnosticSeverity
    {
        Note,
        Warning,
        Error,
        Fatal
    };

    struct ClangDiagnostic
    {
        ClangDiagnosticSeverity severity;
        std::filesystem::path source_file;
        std::uint32_t line;
        std::uint32_t column;
        std::string message;
    };

    enum class ClangRecordDeclarationKind
    {
        Class,
        Struct
    };

    enum class ClangFieldAccess
    {
        Public,
        Protected,
        Private,
        Unspecified
    };

    struct ClangFieldDeclaration
    {
        std::string name;
        std::string type_spelling;
        std::filesystem::path source_file;
        std::uint32_t line;
        std::uint32_t column;
        ClangFieldAccess access;
        bool is_const;
        bool is_bit_field;
    };

    struct ClangParameterDeclaration
    {
        std::string name;
        std::string type_spelling;
        std::filesystem::path source_file;
        std::uint32_t line;
        std::uint32_t column;
        bool is_lvalue_reference;
        bool is_rvalue_reference;
        bool is_const;
    };

    struct ClangFunctionDeclaration
    {
        std::string name;
        std::string return_type_spelling;
        std::filesystem::path source_file;
        std::uint32_t line;
        std::uint32_t column;
        ClangFieldAccess access;
        std::vector<ClangParameterDeclaration> parameters;
        bool is_const;
        bool is_static;
        bool is_variadic;
        bool is_deleted;
        bool is_volatile;
        bool is_ref_qualified;
        bool is_template;
        bool is_special_member;
        bool is_operator;
        bool returns_void;
        bool returns_reference;
    };

    struct ClangRecordDeclaration
    {
        ClangRecordDeclarationKind kind;
        std::string qualified_name;
        std::filesystem::path source_file;
        std::uint32_t line;
        std::uint32_t column;
        std::vector<ClangFieldDeclaration> fields;
        std::vector<ClangFunctionDeclaration> functions;
    };

    struct ClangHeaderScanOutput
    {
        std::string module_name;
        std::filesystem::path header_path;
        std::vector<ClangRecordDeclaration> records;
        std::vector<ClangDiagnostic> diagnostics;
    };

    enum class ClangHeaderScanErrorCode
    {
        EmptyArguments,
        HeaderPathNotAbsolute,
        WorkingDirectoryNotAbsolute,
        TranslationUnitCreationFailed,
        ToolExecutionFailed
    };

    struct ClangHeaderScanError
    {
        ClangHeaderScanErrorCode code;
        std::string module_name;
        std::filesystem::path header_path;
        std::vector<ClangDiagnostic> diagnostics;
    };



    using ClangHeaderScanResult =
        std::variant<ClangHeaderScanOutput, ClangHeaderScanError>;

    [[nodiscard]]
    ClangHeaderScanResult ScanHeaderWithClang(
        const HeaderParseInvocation& invocation);
}
