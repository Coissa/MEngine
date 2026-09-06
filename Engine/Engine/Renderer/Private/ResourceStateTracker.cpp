#include "ResourceStateTracker.h"

#include <algorithm>
#include <new>

namespace GE::Render::Private
{
    bool ResourceStateTracker::TrackBuffer(
        const RHI::Buffer& buffer,
        RHI::BufferState initial_state)
    {
        RHI::BufferState existing_state;
        if (GetBufferState(buffer, existing_state))
        {
            return false;
        }

        try
        {
            buffers_.push_back({.buffer = &buffer, .state = initial_state});
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
        return true;
    }

    bool ResourceStateTracker::TrackTexture(
        const RHI::Texture& texture,
        RHI::TextureState initial_state)
    {
        RHI::TextureState existing_state;
        if (GetTextureState(texture, existing_state))
        {
            return false;
        }

        try
        {
            textures_.push_back({.texture = &texture, .state = initial_state});
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
        return true;
    }

    bool ResourceStateTracker::GetBufferState(
        const RHI::Buffer& buffer,
        RHI::BufferState& out_state) const noexcept
    {
        const auto entry = std::find_if(
            buffers_.begin(),
            buffers_.end(),
            [&buffer](const BufferEntry& candidate)
            {
                return candidate.buffer == &buffer;
            });
        if (entry == buffers_.end())
        {
            return false;
        }
        out_state = entry->state;
        return true;
    }

    bool ResourceStateTracker::GetTextureState(
        const RHI::Texture& texture,
        RHI::TextureState& out_state) const noexcept
    {
        const auto entry = std::find_if(
            textures_.begin(),
            textures_.end(),
            [&texture](const TextureEntry& candidate)
            {
                return candidate.texture == &texture;
            });
        if (entry == textures_.end())
        {
            return false;
        }
        out_state = entry->state;
        return true;
    }

    bool ResourceStateTracker::CommitBufferTransition(
        const RHI::Buffer& buffer,
        RHI::BufferState before,
        RHI::BufferState after) noexcept
    {
        if (before == after)
        {
            return false;
        }
        const auto entry = std::find_if(
            buffers_.begin(),
            buffers_.end(),
            [&buffer](const BufferEntry& candidate)
            {
                return candidate.buffer == &buffer;
            });
        if (entry == buffers_.end() || entry->state != before)
        {
            return false;
        }
        entry->state = after;
        return true;
    }

    bool ResourceStateTracker::CommitTextureTransition(
        const RHI::Texture& texture,
        RHI::TextureState before,
        RHI::TextureState after) noexcept
    {
        if (before == after)
        {
            return false;
        }
        const auto entry = std::find_if(
            textures_.begin(),
            textures_.end(),
            [&texture](const TextureEntry& candidate)
            {
                return candidate.texture == &texture;
            });
        if (entry == textures_.end() || entry->state != before)
        {
            return false;
        }
        entry->state = after;
        return true;
    }

    void ResourceStateTracker::ForgetBuffer(
        const RHI::Buffer& buffer) noexcept
    {
        std::erase_if(
            buffers_,
            [&buffer](const BufferEntry& candidate)
            {
                return candidate.buffer == &buffer;
            });
    }

    void ResourceStateTracker::ForgetTexture(
        const RHI::Texture& texture) noexcept
    {
        std::erase_if(
            textures_,
            [&texture](const TextureEntry& candidate)
            {
                return candidate.texture == &texture;
            });
    }

    void ResourceStateTracker::ClearTextures() noexcept
    {
        textures_.clear();
    }

    void ResourceStateTracker::Reset() noexcept
    {
        buffers_.clear();
        textures_.clear();
    }
}
