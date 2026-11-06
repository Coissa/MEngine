#include "D3D12BackendFactory.h"
#include "D3D12Device.h"
#include "D3D12Swapchain.h"

namespace GE::D3D12
{
    std::unique_ptr<RHI::Device> CreateDevice(const RHI::DeviceDesc& desc)
    {
        return std::make_unique<Device>(desc);
    }

    std::unique_ptr<RHI::Swapchain> CreateSwapchain(RHI::Device& device, void* window, const RHI::SwapchainDesc& desc)
    {
        return std::make_unique<Swapchain>(static_cast<Device&>(device), static_cast<HWND>(window), desc);
    }
} // namespace GE::D3D12
