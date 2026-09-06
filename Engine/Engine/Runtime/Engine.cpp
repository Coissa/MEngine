#include "Engine.h"

namespace GE::Runtime
{
    Engine::Engine() noexcept = default;
    Engine::~Engine() noexcept = default;

    bool Engine::BuildRenderFrame(
        GE::RenderCore::RenderFrame&) noexcept
    {
        return true;
    }

    void Engine::RequestExit() noexcept
    {
        m_exit_requested = true;
    }

    bool Engine::IsExitRequested() const noexcept
    {
        return m_exit_requested;
    }
}
