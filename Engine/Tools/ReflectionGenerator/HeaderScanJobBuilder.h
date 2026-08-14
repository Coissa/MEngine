#pragma once

#include "ModuleManifest.h"
#include "HeaderScanJob.h"

#include <filesystem>
#include <vector>

namespace GE::Reflection::Generator
{
    [[nodiscard]] std::vector<HeaderScanJob> BuildHeaderScanJobs(
        const ModuleManifest& manifest,
        const std::filesystem::path& manifest_directory);
}
