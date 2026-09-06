#pragma once

#include "RHIAdapterCapabilities.h"
#include "RHIFormat.h"
#include "RHIResult.h"
#include "RHIDevice.h"

#include <memory>
#include <vector>

namespace GE::RHI
{
    class Backend
    {
    public:
        virtual ~Backend() noexcept = default;

        Backend(const Backend&) = delete;
        Backend& operator=(const Backend&) = delete;
        Backend(Backend&&) = delete;
        Backend& operator=(Backend&&) = delete;

        [[nodiscard]] virtual RHIBackend GetBackendType()
            const noexcept = 0;

        [[nodiscard]] virtual RHIResult EnumerateAdapters(
            std::vector<AdapterInfo>& out_adapters) const = 0;

        [[nodiscard]] virtual RHIResult QueryFormatCapabilities(
            AdapterId adapter_id,
            RHIFormat format,
            FormatCapabilities& out_capabilities) const noexcept = 0;

        [[nodiscard]] virtual RHIResult CreateDevice(
            const DeviceCreateInfo& create_info,
            std::unique_ptr<Device>& out_device) = 0;

    protected:
        Backend() noexcept = default;
    };
}
