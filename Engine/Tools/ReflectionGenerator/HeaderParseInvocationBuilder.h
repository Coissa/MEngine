#pragma once

#include "HeaderParseInvocation.h"
#include "ResolvedHeaderScanJob.h"
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace GE::Reflection::Generator
{
    enum class HeaderParseInvocationBuildErrorCode
    {
        EmptyArguments,
        WorkingDirectoryNotAbsolute,
        SourceArgumentNotFound,
        MultipleSourceArguments
    };

    struct HeaderParseInvocationBuildError
    {
        HeaderParseInvocationBuildErrorCode code;
        std::string module_name;
        std::filesystem::path compile_command_source;
        std::size_t source_argument_match_count;
    };

    using HeaderParseInvocationBuildResult =
    std::variant<
        std::vector<HeaderParseInvocation>,
        HeaderParseInvocationBuildError>;

    [[nodiscard]]
    HeaderParseInvocationBuildResult BuildHeaderParseInvocations(
        std::span<const ResolvedHeaderScanJob> resolved_jobs);
}
