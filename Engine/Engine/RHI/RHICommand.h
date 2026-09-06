#pragma once

#include "RHIResult.h"
#include "RHIBuffer.h"
#include "RHIPipeline.h"

#include <cstdint>

namespace GE::RHI
{
    class ColorAttachmentView;
    class DepthStencilView;
    class Texture;
    struct TextureUploadLayout;

    enum class TextureState : std::uint8_t
    {
        Undefined,
        Present,
        ColorAttachment,
        DepthWrite,
        ShaderRead,
        Storage,
        CopySource,
        CopyDestination
    };

    struct ClearColor final
    {
        float red = 0.0f;
        float green = 0.0f;
        float blue = 0.0f;
        float alpha = 1.0f;
    };

    struct Viewport final
    {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float min_depth = 0.0f;
        float max_depth = 1.0f;
    };

    struct ScissorRect final
    {
        std::int32_t left = 0;
        std::int32_t top = 0;
        std::int32_t right = 0;
        std::int32_t bottom = 0;
    };

    class CommandAllocator
    {
    public:
        virtual ~CommandAllocator() noexcept = default;
        CommandAllocator(const CommandAllocator&) = delete;
        CommandAllocator& operator=(const CommandAllocator&) = delete;
        CommandAllocator(CommandAllocator&&) = delete;
        CommandAllocator& operator=(CommandAllocator&&) = delete;

        // 调用方必须先用 Fence 证明关联命令已经执行完毕。
        [[nodiscard]] virtual RHIResult Reset() noexcept = 0;

    protected:
        CommandAllocator() noexcept = default;
    };

    class CommandList
    {
    public:
        virtual ~CommandList() noexcept = default;
        CommandList(const CommandList&) = delete;
        CommandList& operator=(const CommandList&) = delete;
        CommandList(CommandList&&) = delete;
        CommandList& operator=(CommandList&&) = delete;

        [[nodiscard]] virtual RHIResult Reset(
            CommandAllocator& allocator) noexcept = 0;
        [[nodiscard]] virtual RHIResult TransitionTexture(
            Texture& texture,
            TextureState before,
            TextureState after) noexcept = 0;
        [[nodiscard]] virtual RHIResult TransitionBuffer(
            Buffer& buffer,
            BufferState before,
            BufferState after) noexcept = 0;
        [[nodiscard]] virtual RHIResult CopyBuffer(
            Buffer& source,
            std::uint64_t source_offset,
            Buffer& destination,
            std::uint64_t destination_offset,
            std::uint64_t size) noexcept = 0;
        // 只记录 Buffer 到 Texture 的复制；状态转换和生命周期由调用方负责。
        [[nodiscard]] virtual RHIResult CopyBufferToTexture(
            Buffer& source,
            Texture& destination,
            const TextureUploadLayout& layout) noexcept = 0;
        [[nodiscard]] virtual RHIResult SetColorAttachment(
            ColorAttachmentView& view) noexcept = 0;
        // 切换到无深度附件的颜色 pass；默认实现允许旧后端显式报告不支持。
        [[nodiscard]] virtual RHIResult SetColorAttachmentOnly(
            ColorAttachmentView&) noexcept
        {
            return RHIResult::Unsupported;
        }
        [[nodiscard]] virtual RHIResult SetDepthStencilAttachment(
            DepthStencilView& view) noexcept = 0;
        [[nodiscard]] virtual RHIResult ClearColorAttachment(
            ColorAttachmentView& view,
            const ClearColor& color) noexcept = 0;
        [[nodiscard]] virtual RHIResult ClearDepthStencilAttachment(
            DepthStencilView& view,
            float depth) noexcept = 0;
        [[nodiscard]] virtual RHIResult SetViewportAndScissor(
            const Viewport& viewport,
            const ScissorRect& scissor) noexcept = 0;
        [[nodiscard]] virtual RHIResult SetGraphicsPipeline(
            GraphicsPipeline& pipeline) noexcept = 0;
        [[nodiscard]] virtual RHIResult SetResourceSet(
            ResourceSet& resource_set) noexcept = 0;
        [[nodiscard]] virtual RHIResult SetVertexBuffer(
            Buffer& buffer,
            std::uint64_t offset,
            std::uint32_t stride) noexcept = 0;
        [[nodiscard]] virtual RHIResult SetIndexBuffer(
            Buffer& buffer,
            std::uint64_t offset,
            RHIFormat index_format) noexcept = 0;
        [[nodiscard]] virtual RHIResult DrawIndexed(
            std::uint32_t index_count,
            std::uint32_t instance_count = 1,
            std::uint32_t first_index = 0,
            std::int32_t vertex_offset = 0,
            std::uint32_t first_instance = 0) noexcept = 0;
        [[nodiscard]] virtual RHIResult Close() noexcept = 0;

    protected:
        CommandList() noexcept = default;
    };
}
