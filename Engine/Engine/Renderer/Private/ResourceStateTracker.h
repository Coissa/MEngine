#pragma once

#include <Engine/RHI/RHIBuffer.h>
#include <Engine/RHI/RHICommand.h>
#include <Engine/RHI/RHITexture.h>

#include <vector>

namespace GE::Render::Private
{
    // Renderer 私有状态账本；只借用资源身份，不拥有或延长资源生命周期。
    class ResourceStateTracker final
    {
    public:
        [[nodiscard]] bool TrackBuffer(
            const RHI::Buffer& buffer,
            RHI::BufferState initial_state);
        [[nodiscard]] bool TrackTexture(
            const RHI::Texture& texture,
            RHI::TextureState initial_state);

        [[nodiscard]] bool GetBufferState(
            const RHI::Buffer& buffer,
            RHI::BufferState& out_state) const noexcept;
        [[nodiscard]] bool GetTextureState(
            const RHI::Texture& texture,
            RHI::TextureState& out_state) const noexcept;

        // 只有账本中的 before 完全匹配且确实发生状态变化时才提交更新。
        [[nodiscard]] bool CommitBufferTransition(
            const RHI::Buffer& buffer,
            RHI::BufferState before,
            RHI::BufferState after) noexcept;
        [[nodiscard]] bool CommitTextureTransition(
            const RHI::Texture& texture,
            RHI::TextureState before,
            RHI::TextureState after) noexcept;

        void ForgetBuffer(const RHI::Buffer& buffer) noexcept;
        void ForgetTexture(const RHI::Texture& texture) noexcept;
        void ClearTextures() noexcept;
        void Reset() noexcept;

    private:
        struct BufferEntry final
        {
            const RHI::Buffer* buffer = nullptr;
            RHI::BufferState state = RHI::BufferState::Common;
        };

        struct TextureEntry final
        {
            const RHI::Texture* texture = nullptr;
            RHI::TextureState state = RHI::TextureState::Undefined;
        };

        std::vector<BufferEntry> buffers_;
        std::vector<TextureEntry> textures_;
    };
}
