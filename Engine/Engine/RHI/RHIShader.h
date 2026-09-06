#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace GE::RHI
{
    enum class ShaderStage : std::uint8_t
    {
        Vertex,
        Pixel,
        Compute
    };

    enum class ShaderBytecodeFormat : std::uint8_t
    {
        // D3D12 接受 DXIL，也兼容旧式 DXBC；RHI 不负责产生这些字节。
        D3DBytecode,
        SpirV
    };

    // bytecode 只在 CreateShader 调用期间借用；Shader 对象必须复制所需字节。
    struct ShaderDescription final
    {
        ShaderStage stage = ShaderStage::Vertex;
        ShaderBytecodeFormat format = ShaderBytecodeFormat::D3DBytecode;
        std::span<const std::byte> bytecode;
        std::string_view debug_name;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return !bytecode.empty();
        }
    };

    class Shader
    {
    public:
        virtual ~Shader() noexcept = default;
        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&&) = delete;
        Shader& operator=(Shader&&) = delete;

        [[nodiscard]] virtual ShaderStage GetStage() const noexcept = 0;

    protected:
        Shader() noexcept = default;
    };
}
