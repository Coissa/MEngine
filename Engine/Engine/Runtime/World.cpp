#include "World.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace GE::Runtime
{
    World::World(std::int32_t scene_state) noexcept
        : m_scene_state(scene_state)
    {
    }

    World::~World() noexcept
    {
        Shutdown();
    }

    bool World::Initialize()
    {
        if (m_state != State::Created)
        {
            std::cerr << "[World] Initialize called in invalid state\n";
            return false;
        }

        m_state = State::Failed;
        std::cout << "[World] Initialize\n";
        m_tick_count = 0;
        m_state = State::Initialized;
        return true;
    }

    [[nodiscard]] bool World::IsInitialized() const noexcept
    {
        return m_state == State::Initialized;
    }

    [[nodiscard]] std::int32_t World::GetSceneState() const noexcept
    {
        return m_scene_state;
    }

    [[nodiscard]] std::uint64_t World::GetTickCount() const noexcept
    {
        return m_tick_count;
    }

    void World::Tick(float delta_seconds)
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[World] Tick called in invalid state\n";
            throw std::logic_error{
                "World::Tick called before successful initialization"
            };
        }
        if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F)
        {
            throw std::invalid_argument{
                "delta_seconds must be finite and non-negative"
            };
        }

        std::cout << "[World] Tick\n";
        ++m_tick_count;
    }

    void World::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
        try
        {
            std::cout << "[World] Shutdown\n";
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] std::unique_ptr<World> World::CreatePIECopy() const
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[World] CreatePIECopy called in invalid state\n";
            return nullptr;
        }

        std::cout << "[World] Create PIE copy\n";
        return std::make_unique<World>(m_scene_state);
    }
}
