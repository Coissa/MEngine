#include "HeaderParseInvocationBuilder.h"

#include <cstddef>
#include <filesystem>
#include <utility>

namespace GE::Reflection::Generator
{
    [[nodiscard]]
    HeaderParseInvocationBuildResult BuildHeaderParseInvocations(
        std::span<const ResolvedHeaderScanJob> resolved_jobs)
    {
        std::vector<HeaderParseInvocation> invocations;
        invocations.reserve(resolved_jobs.size());

        for (const ResolvedHeaderScanJob& resolved_job : resolved_jobs)
        {
            const HeaderScanJob& job = resolved_job.job;
            const CompileCommand& command = resolved_job.compile_command.get();

            if (command.arguments.empty())
            {
                return HeaderParseInvocationBuildError{
                    HeaderParseInvocationBuildErrorCode::EmptyArguments,
                    job.module_name,
                    job.compile_command_source,
                    0};
            }

            if (!command.working_directory.is_absolute())
            {
                return HeaderParseInvocationBuildError{
                    HeaderParseInvocationBuildErrorCode::WorkingDirectoryNotAbsolute,
                    job.module_name,
                    job.compile_command_source,
                    0};
            }

            std::vector<std::string> arguments = command.arguments;
            const std::filesystem::path normalized_source_file =
                (command.source_file.is_absolute()
                    ? command.source_file
                    : command.working_directory / command.source_file)
                .lexically_normal();

            std::size_t source_argument_index = 0;
            std::size_t source_argument_match_count = 0;

            for (std::size_t argument_index = 1;
                 argument_index < arguments.size();
                 ++argument_index)
            {
                const std::filesystem::path argument_path{arguments[argument_index]};
                if (argument_path.empty())
                {
                    continue;
                }

                const std::filesystem::path normalized_argument_path =
                    (argument_path.is_absolute()
                        ? argument_path
                        : command.working_directory / argument_path)
                    .lexically_normal();

                if (normalized_argument_path == normalized_source_file)
                {
                    source_argument_index = argument_index;
                    ++source_argument_match_count;
                }
            }

            if (source_argument_match_count == 0)
            {
                return HeaderParseInvocationBuildError{
                    HeaderParseInvocationBuildErrorCode::SourceArgumentNotFound,
                    job.module_name,
                    job.compile_command_source,
                    source_argument_match_count};
            }

            if (source_argument_match_count > 1)
            {
                return HeaderParseInvocationBuildError{
                    HeaderParseInvocationBuildErrorCode::MultipleSourceArguments,
                    job.module_name,
                    job.compile_command_source,
                    source_argument_match_count};
            }

            arguments[source_argument_index] = job.header_path.string();
            invocations.push_back(HeaderParseInvocation{
                job,
                command.working_directory,
                std::move(arguments)});
        }

        return invocations;
    }
}
