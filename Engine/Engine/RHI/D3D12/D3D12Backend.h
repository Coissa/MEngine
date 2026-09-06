
#pragma once

#include <Engine/RHI/RHIBackend.h>

namespace GE::D3D12RHI
{
    class D3D12Backend final : public RHI::Backend
    {
    public:
        D3D12Backend() noexcept = default;
        ~D3D12Backend() noexcept override = default;

        [[nodiscard]] RHI::RHIBackend GetBackendType()
            const noexcept override;

        [[nodiscard]] RHI::RHIResult EnumerateAdapters(
            std::vector<RHI::AdapterInfo>& out_adapters) const override;

        [[nodiscard]] RHI::RHIResult QueryFormatCapabilities(
            RHI::AdapterId adapter_id,
            RHI::RHIFormat format,
            RHI::FormatCapabilities& out_capabilities)
            const noexcept override;

        [[nodiscard]] RHI::RHIResult CreateDevice(
            const RHI::DeviceCreateInfo& create_info,
            std::unique_ptr<RHI::Device>& out_device) override;
    };
}