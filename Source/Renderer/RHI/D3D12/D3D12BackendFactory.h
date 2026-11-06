#pragma once
#include "../RHIDevice.h"
#include "../RHISwapchain.h"

namespace GE::D3D12
{
    std::unique_ptr<RHI::Device> CreateDevice(const RHI::DeviceDesc& desc = {});
    // Platform handle is only interpreted inside this backend.
    std::unique_ptr<RHI::Swapchain> CreateSwapchain(RHI::Device& device, void* window, const RHI::SwapchainDesc& desc);
} // namespace GE::D3D12
