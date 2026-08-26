#include "Engine.h"

namespace GE::Runtime
{
    Engine::Engine() noexcept = default;
    Engine::~Engine() noexcept = default;

    void Engine::RequestExit() noexcept
    {
        m_exit_requested = true;
    }

    bool Engine::IsExitRequested() const noexcept
    {
        return m_exit_requested;
    }
}
