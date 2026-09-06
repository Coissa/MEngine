#include "NullRenderer.h"

#include <Engine/Core/Diagnostics/Log.h>

namespace GE::Render
{
    NullRenderer::NullRenderer() noexcept = default;

    NullRenderer::~NullRenderer() noexcept
    {
        Shutdown();
    }

    bool NullRenderer::Initialize()
    {
        if (m_state != State::Created)
        {
            GE::Core::LogMessage(
                GE::Core::LogLevel::Error,
                "Renderer",
                "NullRenderer Initialize called in invalid state");
            return false;
        }

        m_state = State::Initialized;
        return true;
    }

    bool NullRenderer::SubmitFrame(GE::RenderCore::RenderFrame frame)
    {
        if (m_state != State::Initialized)
        {
            GE::Core::LogMessage(
                GE::Core::LogLevel::Error,
                "Renderer",
                "NullRenderer SubmitFrame called in invalid state");
            return false;
        }

        static_cast<void>(frame);
        ++m_submitted_frame_count;
        return true;
    }

    void NullRenderer::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
    }

    std::uint64_t NullRenderer::GetSubmittedFrameCount() const noexcept
    {
        return m_submitted_frame_count;
    }
}
