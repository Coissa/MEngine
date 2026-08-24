#include "GameInstance.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace GE::Runtime
{
    GameInstance::GameInstance() noexcept = default;

    GameInstance::~GameInstance() noexcept
    {
        Shutdown();
    }

    [[nodiscard]] bool GameInstance::Initialize()
    {
        if (m_state != State::Created)
        {
            std::cerr << "[GameInstance] Initialize called in invalid state\n";
            return false;
        }

        m_state = State::Failed;
        std::cout << "[GameInstance] Initialize\n";
        m_tick_count = 0;
        m_state = State::Initialized;

        return true;
    }

    void GameInstance::Tick(float delta_seconds)
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[GameInstance] Tick called in invalid state\n";
            throw std::logic_error{
                "GameInstance::Tick called before successful initialization"
            };
        }
        if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F)
        {
            throw std::invalid_argument{
                "delta_seconds must be finite and non-negative"
            };
        }

        std::cout << "[GameInstance] Tick\n";
        ++m_tick_count;
    }

    void GameInstance::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
        try
        {
            std::cout << "[GameInstance] Shutdown\n";
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] bool GameInstance::IsInitialized() const noexcept
    {
        return m_state == State::Initialized;
    }

    [[nodiscard]] std::uint64_t GameInstance::GetTickCount() const noexcept
    {
        return m_tick_count;
    }
}
