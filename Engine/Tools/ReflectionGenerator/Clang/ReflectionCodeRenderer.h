#pragma once

#include "ClangDeclarationValidation.h"
#include "GeneratedCodeFile.h"
#include "GeneratedFilePlan.h"
#include "HeaderScanJob.h"

#include <filesystem>
#include <optional>
#include <string>
#include <variant>

namespace GE::Reflection::Generator
{
    struct ReflectionCodeRenderOutput
    {
        GeneratedCodeFile header_file;
        GeneratedCodeFile source_file;
        std::string registration_function_name;
    };

    enum class ReflectionCodeRenderErrorCode
    {
        ModuleNameMismatch,
        ScanHeaderPathMismatch,
        FilePlanningFailed,
        FilePlanMismatch,
        SourceHeaderIncludePathUnavailable,
        GeneratedHeaderIncludePathUnavailable,
        DeclarationValidationFailed
    };

    struct ReflectionCodeRenderError
    {
        ReflectionCodeRenderErrorCode code;
        std::filesystem::path path;
        std::optional<GeneratedFilePlanError> file_plan_error;
        ClangDeclarationValidationResult declaration_errors;
    };

    using ReflectionCodeRenderResult =
        std::variant<ReflectionCodeRenderOutput, ReflectionCodeRenderError>;

    [[nodiscard]] ReflectionCodeRenderResult RenderReflectionCode(
        const HeaderScanJob& job,
        const ClangHeaderScanOutput& scan_output,
        const GeneratedFilePlan& file_plan);
}
