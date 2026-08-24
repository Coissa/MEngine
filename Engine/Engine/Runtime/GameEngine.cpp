#include "GameEngine.h"
#include "GameInstance.h"
#include "World.h"
#include "WorldContext.h"
#include "WorldType.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace GE::Runtime
{
    GameEngine::GameEngine(std::int32_t initial_scene_state) noexcept
        : m_initial_scene_state(initial_scene_state)
    {
    }

    GameEngine::~GameEngine() noexcept
    {
        Shutdown();
    }

    bool GameEngine::Initialize()
    {
        if (m_state != State::Created)
        {
            std::cerr << "[GameEngine] Initialize called in invalid state\n";
            return false;
        }

        m_state = State::Failed;
        std::cout << "[GameEngine] Initialize\n";

        auto world = std::make_unique<World>(m_initial_scene_state);
        auto game_instance = std::make_unique<GameInstance>();

        auto world_context = std::make_unique<WorldContext>(
            WorldType::Game,
            std::move(world),
            std::move(game_instance)
        );
        if (!world_context->Initialize())
        {
            return false;
        }

        m_world_context = std::move(world_context);
        m_state = State::Initialized;
        return true;
    }

    void GameEngine::Tick(float delta_seconds)
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[GameEngine] Engine not Initialized\n";
            throw std::logic_error{
                "GameEngine::Tick called before successful initialization"
            };
        }
        if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F)
        {
            throw std::invalid_argument{
                "delta_seconds must be finite and non-negative"
            };
        }

        std::cout << "[GameEngine] Tick\n";

        m_world_context->Tick(delta_seconds);
    }

    void GameEngine::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
        try
        {
            std::cout << "[GameEngine] Shutdown\n";
        }
        catch (...)
        {
        }

        if (m_world_context != nullptr)
        {
            m_world_context->Shutdown();
            m_world_context.reset();
        }
    }

    [[nodiscard]] const WorldContext* GameEngine::GetWorldContext() const noexcept
    {
        return m_world_context.get();
    }
}
