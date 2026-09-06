#pragma once

namespace GE::RHI
{
    class Texture;

    // Shader 只读访问纹理的视图；拥有描述符，但只借用被观察的 Texture。
    class SampledTextureView
    {
    public:
        virtual ~SampledTextureView() noexcept = default;
        SampledTextureView(const SampledTextureView&) = delete;
        SampledTextureView& operator=(const SampledTextureView&) = delete;
        SampledTextureView(SampledTextureView&&) = delete;
        SampledTextureView& operator=(SampledTextureView&&) = delete;

        [[nodiscard]] virtual Texture& GetTexture() noexcept = 0;

    protected:
        SampledTextureView() noexcept = default;
    };

    // Texture 的颜色附件访问视图；拥有描述符，不拥有被观察的 Texture。
    class ColorAttachmentView
    {
    public:
        virtual ~ColorAttachmentView() noexcept = default;
        ColorAttachmentView(const ColorAttachmentView&) = delete;
        ColorAttachmentView& operator=(const ColorAttachmentView&) = delete;
        ColorAttachmentView(ColorAttachmentView&&) = delete;
        ColorAttachmentView& operator=(ColorAttachmentView&&) = delete;

        [[nodiscard]] virtual Texture& GetTexture() noexcept = 0;

    protected:
        ColorAttachmentView() noexcept = default;
    };

    // Texture 的深度附件访问视图；与颜色视图一样只借用 Texture。
    class DepthStencilView
    {
    public:
        virtual ~DepthStencilView() noexcept = default;
        DepthStencilView(const DepthStencilView&) = delete;
        DepthStencilView& operator=(const DepthStencilView&) = delete;
        DepthStencilView(DepthStencilView&&) = delete;
        DepthStencilView& operator=(DepthStencilView&&) = delete;

        [[nodiscard]] virtual Texture& GetTexture() noexcept = 0;

    protected:
        DepthStencilView() noexcept = default;
    };
}
