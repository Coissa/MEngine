#pragma once

#include "RHICapabilities.h"

#include <cstdint>

namespace GE::RHI
{
    enum class RHIFormat : std::uint16_t
    {
        Unknown,

        R16_UInt,
        R32_UInt,

        R32G32_Float,
        R32G32B32_Float,
        R32G32B32A32_Float,

        R8G8B8A8_UNorm,
        B8G8R8A8_UNorm,

        D32_Float,
        D24_UNorm_S8_UInt
    };

    struct FormatCapabilities final
    {
        FeatureSupport vertex_buffer = FeatureSupport::Unsupported;
        FeatureSupport sampled_texture = FeatureSupport::Unsupported;
        FeatureSupport storage_texture = FeatureSupport::Unsupported;
        FeatureSupport render_target = FeatureSupport::Unsupported;
        FeatureSupport depth_stencil = FeatureSupport::Unsupported;
        FeatureSupport blendable = FeatureSupport::Unsupported;
    };
}
