#include "D3D12Shader.h"

#include <utility>

namespace GE::D3D12RHI
{
    D3D12Shader::D3D12Shader(
        RHI::ShaderStage stage,
        std::vector<std::byte> bytecode) noexcept
        : stage_(stage), bytecode_(std::move(bytecode))
    {
    }

    RHI::ShaderStage D3D12Shader::GetStage() const noexcept
    {
        return stage_;
    }
}
