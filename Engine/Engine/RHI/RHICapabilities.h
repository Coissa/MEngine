#pragma once

#include <cstdint>

namespace GE::RHI
{
    enum class RHIBackend : std::uint8_t
    {
        Unknown,
        D3D12,
        Vulkan
    };

    enum class AdapterType : std::uint8_t
    {
        Unknown,
        Discrete,
        Integrated,
        Virtual,
        Software
    };

    enum class FeatureSupport : std::uint8_t
    {
        Unsupported,
        Native,
        Emulated
    };

    struct FeatureTier final
    {
        FeatureSupport support = FeatureSupport::Unsupported;
        std::uint32_t tier = 0;
    };

    struct RHILimits final
    {
        std::uint32_t max_texture_2d_dimension = 0;
        std::uint32_t max_color_attachments = 0;
        std::uint64_t constant_buffer_offset_alignment = 0;
    };

    // 只报告已经能通过公开 RHI 契约使用的能力，不用硬件潜力冒充已实现功能。
    struct RHIFeatures final
    {
        FeatureTier graphics_pipeline;
        FeatureTier storage_buffer;
        FeatureTier compute_pipeline;
        FeatureTier draw_indirect;
    };
}
