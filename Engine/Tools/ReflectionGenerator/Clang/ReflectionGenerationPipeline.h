#pragma once

#include "ClangHeaderScanner.h"
#include "CompilationDatabase.h"
#include "GeneratedFilePlan.h"
#include "GeneratedFileWriter.h"
#include "GenerationPlan.h"
#include "HeaderParseInvocationBuilder.h"
#include "HeaderScanJobResolver.h"
#include "ModuleManifestLoader.h"
#include "ReflectionCodeRenderer.h"
#include "ReflectionModuleRenderer.h"

#include <cstddef>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace GE::Reflection::Generator
{
    /// Complete user request for one generator run.
    struct ReflectionGenerationRequest
    {
        /// Absolute path to the CMake-produced compile_commands.json file.
        std::filesystem::path compilation_database_path;

        /// Absolute paths to explicit module manifests, in processing order.
        std::vector<std::filesystem::path> manifest_paths;
    };

    /// Summary returned after every generated file has been written or skipped.
    struct ReflectionGenerationOutput
    {
        std::size_t module_count;
        std::size_t header_count;
        std::size_t reflected_type_count;
        std::vector<ClangDiagnostic> diagnostics;
        std::vector<GeneratedFileWriteRecord> files;
    };

    /// The orchestration stage that stopped a generator run.
    enum class ReflectionGenerationErrorCode
    {
        EmptyManifestPaths,
        CompilationDatabasePathNotAbsolute,
        ManifestPathNotAbsolute,
        ManifestLoadFailed,
        GenerationPlanBuildFailed,
        CompilationDatabaseLoadFailed,
        HeaderScanJobResolutionFailed,
        HeaderParseInvocationBuildFailed,
        GeneratedFilePlanningFailed,
        HeaderScanFailed,
        CodeRenderFailed,
        ModuleCodeRenderFailed,
        FileWriteFailed
    };

    /// Preserves the original typed error from the stage that failed.
    struct ReflectionGenerationError
    {
        static constexpr std::size_t NoJobIndex =
            std::numeric_limits<std::size_t>::max();

        ReflectionGenerationErrorCode code;
        std::size_t job_index;
        std::string module_name;
        std::filesystem::path path;

        std::optional<ModuleManifestLoadError> manifest_load_error;
        std::optional<GenerationPlanBuildError> plan_build_error;
        std::optional<CompilationDatabaseLoadError>
            compilation_database_error;
        std::optional<HeaderScanJobResolutionError> resolution_error;
        std::optional<HeaderParseInvocationBuildError> invocation_error;
        std::optional<GeneratedFilePlanError> file_plan_error;
        std::optional<ClangHeaderScanError> scan_error;
        std::optional<ReflectionCodeRenderError> render_error;
        std::optional<ReflectionModuleRenderError> module_render_error;
        std::optional<GeneratedFileWriteError> write_error;
    };

    using ReflectionGenerationResult =
        std::variant<ReflectionGenerationOutput, ReflectionGenerationError>;

    /// Executes the complete generator pipeline.
    ///
    /// All scanning and rendering finishes before filesystem output begins.
    /// The function therefore performs no generated-file writes when a
    /// manifest, planning, compile-command, Clang, or rendering stage fails.
    [[nodiscard]] ReflectionGenerationResult RunReflectionGeneration(
        const ReflectionGenerationRequest& request);
}
