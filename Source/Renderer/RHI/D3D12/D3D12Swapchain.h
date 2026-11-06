#pragma once
#include "../RHISwapchain.h"
#include "D3D12View.h"
#include <memory>

namespace GE::D3D12
{
    class Device;

    class Swapchain final : public RHI::Swapchain
    {
    public:
        Swapchain(Device& device, HWND window, const RHI::SwapchainDesc& desc);
        RHI::FrameTarget GetCurrentTarget() override;
        void Resize(uint32_t width, uint32_t height) override;
        void Present(bool vsync) override;

    private:
        void CreateTargets();
        Device& device;
        RHI::SwapchainDesc desc;
        ComPtr<IDXGISwapChain3> native;
        std::vector<std::unique_ptr<Texture>> textures;
        std::vector<std::unique_ptr<RHI::TextureView>> views;
    };
} // namespace GE::D3D12
