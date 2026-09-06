#include "GameEngine.h"
#include "BuiltinScene.h"
#include "GameInstance.h"
#include "SceneSerializer.h"
#include "World.h"
#include "WorldContext.h"
#include "WorldType.h"
#include "WorldRenderExtractor.h"

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

    GameEngine::GameEngine(std::filesystem::path scene_path)
        : m_initial_scene_state(0),
          m_initial_scene_path(std::move(scene_path))
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

        std::unique_ptr<World> world;
        if (!m_initial_scene_path.empty())
        {
            std::string error;
            world = SceneSerializer::LoadFromFile(m_initial_scene_path, error);
            if (world == nullptr)
            {
                std::cerr << "[GameEngine] Scene load failed: " << error << '\n';
                return false;
            }
        }
        else
        {
            world = std::make_unique<World>(m_initial_scene_state);
        }
        if (m_initial_scene_path.empty() && !PopulateBuiltinScene(*world))
        {
            return false;
        }
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

    bool GameEngine::BuildRenderFrame(
        GE::RenderCore::RenderFrame& out_frame) noexcept
    {
        const World* world = m_world_context == nullptr
            ? nullptr : m_world_context->GetWorld();
        return world != nullptr
            && WorldRenderExtractor::Extract(*world, out_frame);
    }
}
