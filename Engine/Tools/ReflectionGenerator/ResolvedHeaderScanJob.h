#pragma once

#include "CompileCommand.h"
#include "HeaderScanJob.h"

#include <functional>

namespace GE::Reflection::Generator
{
    struct ResolvedHeaderScanJob
    {
        HeaderScanJob job;
        std::reference_wrapper<const CompileCommand> compile_command;
    };
}