#pragma once

#include "RHICapabilities.h"

#include <cstdint>
#include <string>

namespace GE::RHI
{
    struct AdapterId final
    {
        std::uint64_t value = 0;
    };

    struct AdapterCapabilities final
    {
        RHIFeatures features;
        RHILimits limits;
    };

    struct AdapterInfo final
    {
        AdapterId id;
        RHIBackend backend = RHIBackend::Unknown;
        AdapterType type = AdapterType::Unknown;
        std::uint32_t vendor_id = 0;
        std::uint32_t device_id = 0;
        std::string name;
        AdapterCapabilities capabilities;
    };
}
