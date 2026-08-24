#include "WorldContext.h"
#include "GameInstance.h"
#include "World.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace GE::Runtime
{
    namespace
    {
        [[nodiscard]] const char* GetWorldTypeName(WorldType world_type) noexcept
        {
            switch (world_type)
            {
            case WorldType::Game:
                return "Game";
            case WorldType::Editor:
                return "Editor";
            case WorldType::PIE:
                return "PIE";
            case WorldType::None:
            default:
                return "None";
            }
        }
    }

    WorldContext::WorldContext(
        WorldType world_type,
        std::unique_ptr<World> world,
        std::unique_ptr<GameInstance> game_instance
    ) noexcept
        : m_world_type(world_type)
        , m_world(std::move(world))
        , m_game_instance(std::move(game_instance))
    {
    }

    WorldContext::~WorldContext() noexcept
    {
        Shutdown();
    }

    [[nodiscard]] bool WorldContext::Initialize()
    {
        if (m_state != State::Created)
        {
            std::cerr << "[WorldContext " << GetWorldTypeName(m_world_type)
                      << "] Initialize called in invalid state\n";
            return false;
        }

        m_state = State::Failed;
        if (!HasValidObjectCombination())
        {
            std::cerr << "[WorldContext " << GetWorldTypeName(m_world_type)
                      << "] Initialize called with invalid object combination\n";
            return false;
        }

        std::cout << "[WorldContext " << GetWorldTypeName(m_world_type)
                  << "] Initialize\n";

        if (m_game_instance != nullptr && !m_game_instance->Initialize())
        {
            m_game_instance->Shutdown();
            return false;
        }

        if (!m_world->Initialize())
        {
            m_world->Shutdown();
            if (m_game_instance != nullptr)
            {
                m_game_instance->Shutdown();
            }
            return false;
        }

        m_state = State::Initialized;
        return true;
    }

    void WorldContext::Tick(float delta_seconds)
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[WorldContext " << GetWorldTypeName(m_world_type)
                      << "] Tick called in invalid state\n";
            throw std::logic_error{
                "WorldContext::Tick called before successful initialization"
            };
        }
        if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F)
        {
            throw std::invalid_argument{
                "delta_seconds must be finite and non-negative"
            };
        }

        std::cout << "[WorldContext " << GetWorldTypeName(m_world_type)
                  << "] Tick\n";
        if (m_game_instance != nullptr)
        {
            m_game_instance->Tick(delta_seconds);
        }
        m_world->Tick(delta_seconds);
    }

    void WorldContext::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
        try
        {
            std::cout << "[WorldContext " << GetWorldTypeName(m_world_type)
                      << "] Shutdown\n";
        }
        catch (...)
        {
        }

        if (m_world != nullptr)
        {
            m_world->Shutdown();
        }
        if (m_game_instance != nullptr)
        {
            m_game_instance->Shutdown();
        }
    }

    [[nodiscard]] bool WorldContext::IsInitialized() const noexcept
    {
        return m_state == State::Initialized;
    }

    [[nodiscard]] WorldType WorldContext::GetWorldType() const noexcept
    {
        return m_world_type;
    }

    [[nodiscard]] const World* WorldContext::GetWorld() const noexcept
    {
        return m_world.get();
    }

    [[nodiscard]] const GameInstance* WorldContext::GetGameInstance() const noexcept
    {
        return m_game_instance.get();
    }

    [[nodiscard]] bool WorldContext::HasValidObjectCombination() const noexcept
    {
        if (m_world == nullptr)
        {
            return false;
        }

        switch (m_world_type)
        {
        case WorldType::Game:
        case WorldType::PIE:
            return m_game_instance != nullptr;
        case WorldType::Editor:
            return m_game_instance == nullptr;
        case WorldType::None:
        default:
            return false;
        }
    }
}
