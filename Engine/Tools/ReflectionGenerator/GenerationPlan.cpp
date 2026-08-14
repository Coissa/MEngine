#include "GenerationPlan.h"

#include "HeaderScanJobBuilder.h"

#include <unordered_set>
#include <utility>

namespace GE::Reflection::Generator
{
    GenerationPlanBuildResult BuildGenerationPlan(
        std::span<const ModuleManifestInput> manifest_inputs)
    {
        if (manifest_inputs.empty())
        {
            return GenerationPlanBuildError{
                GenerationPlanBuildErrorCode::EmptyManifestInputs,
                {},
                {},
                std::nullopt
            };
        }

        std::size_t total_job_count = 0;
        for (const auto& manifest_input : manifest_inputs)
        {
            total_job_count += manifest_input.manifest.header_files.size();
        }

        GenerationPlan plan;
        plan.jobs.reserve(total_job_count);

        std::unordered_set<std::string> module_names;
        std::unordered_set<std::filesystem::path> header_paths;

        for (const auto& manifest_input : manifest_inputs)
        {
            const auto& path = manifest_input.manifest_path;
            const auto& manifest = manifest_input.manifest;

            if (!path.is_absolute())
            {
                return GenerationPlanBuildError{
                    GenerationPlanBuildErrorCode::ManifestPathNotAbsolute,
                    manifest.module_name,
                    path,
                    std::nullopt
                };
            }

            if (!module_names.insert(manifest.module_name).second)
            {
                return GenerationPlanBuildError{
                    GenerationPlanBuildErrorCode::DuplicateModuleName,
                    manifest.module_name,
                    path,
                    std::nullopt
                };
            }

            std::vector<HeaderScanJob> manifest_jobs = BuildHeaderScanJobs(manifest, path.parent_path());

            for (auto& job : manifest_jobs)
            {
                const auto validation_error = ValidateHeaderScanJob(job);
                if (validation_error)
                {
                    return GenerationPlanBuildError{
                        GenerationPlanBuildErrorCode::InvalidHeaderScanJob,
                        manifest.module_name,
                        validation_error->path,
                        validation_error
                    };
                }

                if (!header_paths.insert(job.header_path).second)
                {
                    return GenerationPlanBuildError{
                        GenerationPlanBuildErrorCode::DuplicateHeaderPath,
                        manifest.module_name,
                        job.header_path,
                        std::nullopt
                    };
                }

                plan.jobs.push_back(std::move(job));
            }
        }

        return plan;
    }
}
