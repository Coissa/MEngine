#include "HeaderScanJobResolver.h"

namespace GE::Reflection::Generator
{
    [[nodiscard]]HeaderScanJobResolutionResult ResolveHeaderScanJobs(
        const GenerationPlan& plan,
        const CompilationDatabase& compilation_database)
    {
        std::vector<ResolvedHeaderScanJob> resolved_jobs;
        resolved_jobs.reserve(plan.jobs.size());
        for (const auto& job : plan.jobs){

            std::filesystem::path cmpcmdsource = job.compile_command_source;
            CompileCommandLookupResult lookup_result = FindCompileCommand(compilation_database, cmpcmdsource);
            const auto* lookup_error = std::get_if<CompileCommandLookupError>(&lookup_result);
            if (lookup_error)
            {
                return HeaderScanJobResolutionError(
                    *lookup_error,
                    job.module_name,
                    cmpcmdsource
                );
            }

            resolved_jobs.emplace_back(ResolvedHeaderScanJob(job, std::get<std::reference_wrapper<const CompileCommand>>(lookup_result)));
        }
        return resolved_jobs;
    }
}
