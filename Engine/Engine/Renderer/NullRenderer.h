#pragma once

#include "Renderer.h"

#include <cstdint>

namespace GE::Render
{
    class NullRenderer final : public Renderer
    {
    public:
        NullRenderer() noexcept;
        ~NullRenderer() noexcept override;

        NullRenderer(const NullRenderer&) = delete;
        NullRenderer& operator=(const NullRenderer&) = delete;
        NullRenderer(NullRenderer&&) = delete;
        NullRenderer& operator=(NullRenderer&&) = delete;

        [[nodiscard]] bool Initialize() override;

        [[nodiscard]] bool SubmitFrame(
            GE::RenderCore::RenderFrame frame) override;

        void Shutdown() noexcept override;

        [[nodiscard]] std::uint64_t GetSubmittedFrameCount() const noexcept;
    private:
        enum class State
        {
            Created,
            Initialized,
            Shutdown
        };

        State m_state = State::Created;
        std::uint64_t m_submitted_frame_count = 0;
    };
}