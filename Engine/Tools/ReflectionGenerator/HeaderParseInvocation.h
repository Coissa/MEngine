#pragma once

#include "HeaderScanJob.h"

#include <filesystem>
#include <string>
#include <vector>

namespace GE::Reflection::Generator
{
    struct HeaderParseInvocation
    {
        HeaderScanJob job;
        std::filesystem::path working_directory;
        std::vector<std::string> arguments;
    };
}