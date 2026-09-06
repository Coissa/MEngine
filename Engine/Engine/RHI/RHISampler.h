#pragma once

#include <cstdint>

namespace GE::RHI
{
    // D3D12/Vulkan 都能自然表达的基础采样过滤方式。
    enum class SamplerFilter : std::uint8_t
    {
        Nearest,
        Linear
    };

    enum class SamplerAddressMode : std::uint8_t
    {
        Repeat,
        ClampToEdge
    };

    struct SamplerDescription final
    {
        SamplerFilter filter = SamplerFilter::Linear;
        SamplerAddressMode address_u = SamplerAddressMode::Repeat;
        SamplerAddressMode address_v = SamplerAddressMode::Repeat;
        SamplerAddressMode address_w = SamplerAddressMode::Repeat;
    };

    // Sampler 描述如何读取纹理，不拥有 Texture，也不保存任何材质数据。
    class Sampler
    {
    public:
        virtual ~Sampler() noexcept = default;
        Sampler(const Sampler&) = delete;
        Sampler& operator=(const Sampler&) = delete;
        Sampler(Sampler&&) = delete;
        Sampler& operator=(Sampler&&) = delete;

        [[nodiscard]] virtual const SamplerDescription&
        GetDescription() const noexcept = 0;

    protected:
        Sampler() noexcept = default;
    };
}
