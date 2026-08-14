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

    struct ClangRecordDeclaration
    {
        ClangRecordDeclarationKind kind;
        std::string qualified_name;
        std::filesystem::path source_file;
        std::uint32_t line;
        std::uint32_t column;
        std::vector<ClangFieldDeclaration> fields;
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
