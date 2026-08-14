#pragma once

#include "HeaderScanJob.h"
#include "HeaderScanJobValidation.h"
#include "ModuleManifest.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace GE::Reflection::Generator
{
    enum class GenerationPlanBuildErrorCode
    {
        EmptyManifestInputs,
        ManifestPathNotAbsolute,
        DuplicateModuleName,
        DuplicateHeaderPath,
        InvalidHeaderScanJob
    };

    struct ModuleManifestInput
    {
        std::filesystem::path manifest_path;
        ModuleManifest manifest;
    };

    struct GenerationPlan
    {
        std::vector<HeaderScanJob> jobs;
    };

    struct GenerationPlanBuildError
    {
        GenerationPlanBuildErrorCode code;
        std::string module_name;
        std::filesystem::path path;
        std::optional<HeaderScanJobValidationError> scan_job_error;
    };

    using GenerationPlanBuildResult =
        std::variant<GenerationPlan, GenerationPlanBuildError>;

    [[nodiscard]] GenerationPlanBuildResult BuildGenerationPlan(
        std::span<const ModuleManifestInput> manifest_inputs);
}
