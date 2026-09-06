#pragma once

#include "RHIAdapterCapabilities.h"

namespace GE::RHI
{
    struct DeviceCreateInfo final
    {
        AdapterId adapter_id;
    };

    struct DeviceCapabilities final
    {
        AdapterId adapter_id;
        RHIFeatures features;
        RHILimits limits;
    };
}
