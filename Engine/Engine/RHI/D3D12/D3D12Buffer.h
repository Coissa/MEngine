#pragma once

#include <Engine/RHI/RHIBuffer.h>

#include <d3d12.h>
#include <wrl/client.h>

namespace GE::D3D12RHI
{
    class D3D12CommandList;
    class D3D12Device;
    class D3D12ResourceSet;

    class D3D12Buffer final : public RHI::Buffer
    {
    public:
        D3D12Buffer(
            Microsoft::WRL::ComPtr<ID3D12Resource> native_resource,
            RHI::BufferDescription description) noexcept;
        ~D3D12Buffer() noexcept override = default;

        [[nodiscard]] const RHI::BufferDescription&
        GetDescription() const noexcept override;
        [[nodiscard]] RHI::RHIResult Map(
            std::uint64_t offset,
            std::uint64_t size,
            void*& out_data) noexcept override;
        void Unmap(
            std::uint64_t offset,
            std::uint64_t size) noexcept override;

    private:
        friend class D3D12CommandList;
        friend class D3D12Device;
        friend class D3D12ResourceSet;
        Microsoft::WRL::ComPtr<ID3D12Resource> native_resource_;
        RHI::BufferDescription description_;
        bool mapped_ = false;
    };
}
