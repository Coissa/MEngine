#pragma once

#include <Engine/RHI/RHITexture.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>

namespace GE::D3D12RHI
{
    class D3D12CommandList;
    class D3D12Device;
    // 后端私有纹理包装；持有 ID3D12Resource COM 引用，不暴露给通用 Renderer。
    class D3D12Texture final : public RHI::Texture
    {
    public:
        D3D12Texture(
            Microsoft::WRL::ComPtr<ID3D12Resource> native_resource,
            RHI::TextureDescription description) noexcept;

        ~D3D12Texture() noexcept override = default;

        [[nodiscard]] const RHI::TextureDescription&
        GetDescription() const noexcept override;

    private:
        friend class D3D12CommandList;
        friend class D3D12Device;
        Microsoft::WRL::ComPtr<ID3D12Resource> native_resource_;
        RHI::TextureDescription description_;
    };
}
