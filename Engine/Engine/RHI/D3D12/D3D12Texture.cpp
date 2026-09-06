#include "D3D12Texture.h"

#include <utility>

namespace GE::D3D12RHI
{
    D3D12Texture::D3D12Texture(
        Microsoft::WRL::ComPtr<ID3D12Resource> native_resource,
        RHI::TextureDescription description) noexcept
        : native_resource_(std::move(native_resource)),
          description_(description)
    {
    }

    const RHI::TextureDescription&
    D3D12Texture::GetDescription() const noexcept
    {
        return description_;
    }
}
