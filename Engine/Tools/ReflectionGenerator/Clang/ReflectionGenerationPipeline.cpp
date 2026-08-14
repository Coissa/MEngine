#include "ReflectionGenerationPipeline.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <span>
#include <string>
#include <utility>

namespace GE::Reflection::Generator
{
    namespace
    {
        ReflectionGenerationError MakeError(
            ReflectionGenerationErrorCode code,
            std::size_t job_index = ReflectionGenerationError::NoJobIndex,
            std::string module_name = {},
            std::filesystem::path path = {})
        {
            return ReflectionGenerationError{
                code,
                job_index,
                std::move(module_name),
                std::move(path),
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt};
        }
    }

    ReflectionGenerationResult RunReflectionGeneration(
        const ReflectionGenerationRequest& request)
    {
        if (request.manifest_paths.empty())
        {
            return MakeError(
                ReflectionGenerationErrorCode::EmptyManifestPaths);
        }

        if (!request.compilation_database_path.is_absolute())
        {
            return MakeError(
                ReflectionGenerationErrorCode::
                    CompilationDatabasePathNotAbsolute,
                ReflectionGenerationError::NoJobIndex,
                {},
                request.compilation_database_path);
        }

        std::vector<ModuleManifestInput> manifest_inputs;
        manifest_inputs.reserve(request.manifest_paths.size());
        for (const std::filesystem::path& manifest_path :
             request.manifest_paths)
        {
            if (!manifest_path.is_absolute())
            {
                return MakeError(
                    ReflectionGenerationErrorCode::ManifestPathNotAbsolute,
                    ReflectionGenerationError::NoJobIndex,
                    {},
                    manifest_path);
            }

            ModuleManifestLoadResult load_result =
                LoadModuleManifest(manifest_path);
            const auto* load_error =
                std::get_if<ModuleManifestLoadError>(&load_result);
            if (load_error != nullptr)
            {
                ReflectionGenerationError error = MakeError(
                    ReflectionGenerationErrorCode::ManifestLoadFailed,
                    ReflectionGenerationError::NoJobIndex,
                    {},
                    manifest_path);
                error.manifest_load_error = *load_error;
                return error;
            }

            manifest_inputs.push_back(ModuleManifestInput{
                manifest_path.lexically_normal(),
                std::get<ModuleManifest>(std::move(load_result))});
        }

        GenerationPlanBuildResult plan_result =
            BuildGenerationPlan(manifest_inputs);
        const auto* plan_error =
            std::get_if<GenerationPlanBuildError>(&plan_result);
        if (plan_error != nullptr)
        {
            ReflectionGenerationError error = MakeError(
                ReflectionGenerationErrorCode::GenerationPlanBuildFailed,
                ReflectionGenerationError::NoJobIndex,
                plan_error->module_name,
                plan_error->path);
            error.plan_build_error = *plan_error;
            return error;
        }
        GenerationPlan plan = std::get<GenerationPlan>(
            std::move(plan_result));

        CompilationDatabaseLoadResult database_result =
            LoadCompilationDatabase(
                request.compilation_database_path.lexically_normal());
        const auto* database_error =
            std::get_if<CompilationDatabaseLoadError>(&database_result);
        if (database_error != nullptr)
        {
            ReflectionGenerationError error = MakeError(
                ReflectionGenerationErrorCode::
                    CompilationDatabaseLoadFailed,
                ReflectionGenerationError::NoJobIndex,
                {},
                request.compilation_database_path);
            error.compilation_database_error = *database_error;
            return error;
        }
        CompilationDatabase compilation_database =
            std::get<CompilationDatabase>(std::move(database_result));

        HeaderScanJobResolutionResult resolution_result =
            ResolveHeaderScanJobs(plan, compilation_database);
        const auto* resolution_error =
            std::get_if<HeaderScanJobResolutionError>(
                &resolution_result);
        if (resolution_error != nullptr)
        {
            ReflectionGenerationError error = MakeError(
                ReflectionGenerationErrorCode::
                    HeaderScanJobResolutionFailed,
                ReflectionGenerationError::NoJobIndex,
                resolution_error->module_name,
                resolution_error->compile_command_source);
            error.resolution_error = *resolution_error;
            return error;
        }

        std::vector<ResolvedHeaderScanJob> resolved_jobs =
            std::get<std::vector<ResolvedHeaderScanJob>>(
                std::move(resolution_result));
        HeaderParseInvocationBuildResult invocation_result =
            BuildHeaderParseInvocations(resolved_jobs);
        const auto* invocation_error =
            std::get_if<HeaderParseInvocationBuildError>(
                &invocation_result);
        if (invocation_error != nullptr)
        {
            ReflectionGenerationError error = MakeError(
                ReflectionGenerationErrorCode::
                    HeaderParseInvocationBuildFailed,
                ReflectionGenerationError::NoJobIndex,
                invocation_error->module_name,
                invocation_error->compile_command_source);
            error.invocation_error = *invocation_error;
            return error;
        }

        std::vector<HeaderParseInvocation> invocations =
            std::get<std::vector<HeaderParseInvocation>>(
                std::move(invocation_result));
        std::vector<GeneratedCodeFile> generated_files;
        generated_files.reserve(
            invocations.size() * 2 + manifest_inputs.size() * 2);
        std::vector<ReflectionModuleRenderInput> module_render_inputs;
        module_render_inputs.reserve(manifest_inputs.size());
        std::vector<ClangDiagnostic> diagnostics;
        std::size_t reflected_type_count = 0;

        for (std::size_t job_index = 0;
             job_index < invocations.size();
             ++job_index)
        {
            const HeaderParseInvocation& invocation =
                invocations[job_index];
            const HeaderScanJob& job = invocation.job;

            GeneratedFilePlanResult file_plan_result =
                BuildGeneratedFilePlan(job);
            const auto* file_plan_error =
                std::get_if<GeneratedFilePlanError>(&file_plan_result);
            if (file_plan_error != nullptr)
            {
                ReflectionGenerationError error = MakeError(
                    ReflectionGenerationErrorCode::
                        GeneratedFilePlanningFailed,
                    job_index,
                    job.module_name,
                    file_plan_error->path);
                error.file_plan_error = *file_plan_error;
                return error;
            }
            const GeneratedFilePlan& file_plan =
                std::get<GeneratedFilePlan>(file_plan_result);

            ClangHeaderScanResult scan_result =
                ScanHeaderWithClang(invocation);
            const auto* scan_error =
                std::get_if<ClangHeaderScanError>(&scan_result);
            if (scan_error != nullptr)
            {
                ReflectionGenerationError error = MakeError(
                    ReflectionGenerationErrorCode::HeaderScanFailed,
                    job_index,
                    job.module_name,
                    job.header_path);
                error.scan_error = *scan_error;
                return error;
            }
            ClangHeaderScanOutput scan_output =
                std::get<ClangHeaderScanOutput>(std::move(scan_result));

            ReflectionCodeRenderResult render_result =
                RenderReflectionCode(job, scan_output, file_plan);
            const auto* render_error =
                std::get_if<ReflectionCodeRenderError>(&render_result);
            if (render_error != nullptr)
            {
                ReflectionGenerationError error = MakeError(
                    ReflectionGenerationErrorCode::CodeRenderFailed,
                    job_index,
                    job.module_name,
                    render_error->path.empty()
                        ? job.header_path
                        : render_error->path);
                error.render_error = *render_error;
                return error;
            }

            reflected_type_count += scan_output.records.size();
            diagnostics.insert(
                diagnostics.end(),
                std::make_move_iterator(scan_output.diagnostics.begin()),
                std::make_move_iterator(scan_output.diagnostics.end()));

            ReflectionCodeRenderOutput rendered =
                std::get<ReflectionCodeRenderOutput>(
                    std::move(render_result));

            auto module_input = std::find_if(
                module_render_inputs.begin(),
                module_render_inputs.end(),
                [&job](const ReflectionModuleRenderInput& candidate)
                {
                    return candidate.module_name == job.module_name;
                });
            if (module_input == module_render_inputs.end())
            {
                module_render_inputs.push_back(ReflectionModuleRenderInput{
                    job.module_name,
                    job.output_directory,
                    {}});
                module_input = std::prev(module_render_inputs.end());
            }
            module_input->header_registrations.push_back(
                ReflectionHeaderRegistration{
                    rendered.header_file.path,
                    rendered.registration_function_name});

            generated_files.push_back(std::move(rendered.header_file));
            generated_files.push_back(std::move(rendered.source_file));
        }

        for (const ReflectionModuleRenderInput& module_input :
             module_render_inputs)
        {
            ReflectionModuleRenderResult module_render_result =
                RenderReflectionModuleRegistration(module_input);
            const auto* module_render_error =
                std::get_if<ReflectionModuleRenderError>(
                    &module_render_result);
            if (module_render_error != nullptr)
            {
                ReflectionGenerationError error = MakeError(
                    ReflectionGenerationErrorCode::ModuleCodeRenderFailed,
                    ReflectionGenerationError::NoJobIndex,
                    module_input.module_name,
                    module_render_error->path.empty()
                        ? module_input.output_directory
                        : module_render_error->path);
                error.module_render_error = *module_render_error;
                return error;
            }

            ReflectionModuleRenderOutput module_rendered =
                std::get<ReflectionModuleRenderOutput>(
                    std::move(module_render_result));
            generated_files.push_back(
                std::move(module_rendered.header_file));
            generated_files.push_back(
                std::move(module_rendered.source_file));
        }

        GeneratedFilesWriteResult write_result =
            WriteGeneratedCodeFiles(generated_files);
        const auto* write_error =
            std::get_if<GeneratedFileWriteError>(&write_result);
        if (write_error != nullptr)
        {
            ReflectionGenerationError error = MakeError(
                ReflectionGenerationErrorCode::FileWriteFailed,
                ReflectionGenerationError::NoJobIndex,
                {},
                write_error->path);
            error.write_error = *write_error;
            return error;
        }

        GeneratedFilesWriteOutput write_output =
            std::get<GeneratedFilesWriteOutput>(std::move(write_result));
        return ReflectionGenerationOutput{
            manifest_inputs.size(),
            invocations.size(),
            reflected_type_count,
            std::move(diagnostics),
            std::move(write_output.files)};
    }
}
