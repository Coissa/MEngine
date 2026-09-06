#pragma once

#include "RHIFormat.h"

#include <cstdint>

namespace GE::RHI
{
    // D3D12/Vulkan 都能自然表达的纹理用途；可组合标志为未来 GPU-driven 留出空间。
    enum class TextureUsage : std::uint16_t
    {
        None = 0,
        Sampled = 1u << 0,
        Storage = 1u << 1,
        ColorAttachment = 1u << 2,
        DepthStencilAttachment = 1u << 3,
        CopySource = 1u << 4,
        CopyDestination = 1u << 5,
        Present = 1u << 6
    };

    [[nodiscard]] constexpr TextureUsage operator|(
        TextureUsage left,
        TextureUsage right) noexcept
    {
        return static_cast<TextureUsage>(
            static_cast<std::uint16_t>(left)
            | static_cast<std::uint16_t>(right));
    }

    [[nodiscard]] constexpr bool HasTextureUsage(
        TextureUsage value,
        TextureUsage required) noexcept
    {
        return (static_cast<std::uint16_t>(value)
                & static_cast<std::uint16_t>(required))
            == static_cast<std::uint16_t>(required);
    }

    // 纹理的后端无关只读描述；不包含原生句柄或可变资源状态。
    struct TextureDescription final
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t array_layers = 1;
        std::uint32_t mip_levels = 1;
        std::uint32_t sample_count = 1;
        RHIFormat format = RHIFormat::Unknown;
        TextureUsage usage = TextureUsage::None;

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return width != 0
                && height != 0
                && array_layers != 0
                && mip_levels != 0
                && sample_count != 0
                && format != RHIFormat::Unknown
                && usage != TextureUsage::None;
        }
    };

    // 通用纹理身份；普通资源由创建者拥有，交换链图片由 Swapchain 拥有。
    class Texture
    {
    public:
        virtual ~Texture() noexcept = default;

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&&) = delete;
        Texture& operator=(Texture&&) = delete;

        [[nodiscard]] virtual const TextureDescription&
        GetDescription() const noexcept = 0;

    protected:
        Texture() noexcept = default;
    };

    struct TextureSubresource final
    {
        std::uint32_t mip_level = 0;
        std::uint32_t array_layer = 0;
    };

    struct TextureUploadLayout final
    {
        TextureSubresource subresource;

        // 本次纹理数据在 Upload Buffer 中的起始位置。
        std::uint64_t buffer_offset = 0;

        // 从 Buffer 第 0 字节开始计算，Buffer 至少需要这么大。
        std::uint64_t required_buffer_size = 0;

        // 一行真正的像素数据大小，不包含对齐填充。
        std::uint64_t row_size = 0;

        // Upload Buffer 中相邻两行的距离，可能大于 row_size。
        std::uint32_t row_pitch = 0;

        // 需要复制多少行。
        std::uint32_t row_count = 0;
    };
}
