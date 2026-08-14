#pragma once

#include "CompilationDatabase.h"
#include "GenerationPlan.h"
#include "ResolvedHeaderScanJob.h"

#include <filesystem>
#include <string>
#include <variant>
#include <vector>


namespace GE::Reflection::Generator
{
    struct HeaderScanJobResolutionError
    {
        CompileCommandLookupError lookup_error_code;
        std::string module_name;
        std::filesystem::path compile_command_source;
    };

    using HeaderScanJobResolutionResult =
    std::variant<
        std::vector<ResolvedHeaderScanJob>,
        HeaderScanJobResolutionError>;

    [[nodiscard]] HeaderScanJobResolutionResult ResolveHeaderScanJobs(
        const GenerationPlan& plan,
        const CompilationDatabase& compilation_database
    );
}