#pragma once

#include <Engine/RHI/RHIShader.h>

#include <cstddef>
#include <vector>

namespace GE::D3D12RHI
{
    class D3D12Device;

    // D3D12 Shader 没有独立 COM 对象；包装器拥有一份 DXIL 字节。
    class D3D12Shader final : public RHI::Shader
    {
    public:
        D3D12Shader(
            RHI::ShaderStage stage,
            std::vector<std::byte> bytecode) noexcept;
        ~D3D12Shader() noexcept override = default;

        [[nodiscard]] RHI::ShaderStage GetStage() const noexcept override;

    private:
        friend class D3D12Device;
        RHI::ShaderStage stage_;
        std::vector<std::byte> bytecode_;
    };
}
