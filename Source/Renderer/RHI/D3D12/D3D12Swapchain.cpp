#include "D3D12Swapchain.h"
#include "D3D12Device.h"

namespace GE::D3D12
{
    Swapchain::Swapchain(Device& owner, HWND window, const RHI::SwapchainDesc& description)
        : device(owner), desc(description)
    {
        DXGI_SWAP_CHAIN_DESC1 nativeDesc{};
        nativeDesc.Width = desc.width;
        nativeDesc.Height = desc.height;
        nativeDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        nativeDesc.SampleDesc.Count = 1;
        nativeDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        nativeDesc.BufferCount = desc.bufferCount;
        nativeDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> swapchain;
        Check(device.Factory()->CreateSwapChainForHwnd(device.Queue().Native(), window, &nativeDesc, nullptr, nullptr,
                                                       &swapchain),
              "CreateSwapChainForHwnd");
        Check(swapchain.As(&native), "Query SwapChain3");
        Check(device.Factory()->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "MakeWindowAssociation");
        CreateTargets();
    }

    void Swapchain::CreateTargets()
    {
        const RHI::TextureDesc textureDesc{desc.width, desc.height, RHI::RHIFormat::B8G8R8A8_UNorm, true, false};
        for (uint32_t i = 0; i < desc.bufferCount; ++i)
        {
            ComPtr<ID3D12Resource> resource;
            Check(native->GetBuffer(i, IID_PPV_ARGS(&resource)), "Swapchain GetBuffer");
            textures.push_back(std::make_unique<Texture>(std::move(resource), textureDesc));
            views.push_back(device.CreateView(*textures.back(), RHI::ViewKind::RenderTarget));
        }
    }

    RHI::FrameTarget Swapchain::GetCurrentTarget()
    {
        return {*views[native->GetCurrentBackBufferIndex()]};
    }

    void Swapchain::Resize(uint32_t width, uint32_t height)
    {
        // Host has stopped recording and waited. No FrameTarget borrowing crosses
        // this call.
        views.clear();
        textures.clear();
        Check(native->ResizeBuffers(desc.bufferCount, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0), "ResizeBuffers");
        desc.width = width;
        desc.height = height;
        CreateTargets();
    }

    void Swapchain::Present(bool vsync)
    {
        Check(native->Present(vsync ? 1 : 0, 0), "Present");
    }
} // namespace GE::D3D12
