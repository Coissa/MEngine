#pragma once

#include "WorldType.h"

#include <memory>

namespace GE::Runtime
{
    class World;
    class GameInstance;

    class WorldContext final
    {
    public:
        WorldContext(
            WorldType world_type,
            std::unique_ptr<World> world,
            std::unique_ptr<GameInstance> game_instance = nullptr
        ) noexcept;
        ~WorldContext() noexcept;

        WorldContext(const WorldContext& worldcontext) = delete;
        WorldContext& operator=(const WorldContext& worldcontext) = delete;

        WorldContext(WorldContext&& worldcontext) = delete;
        WorldContext& operator=(WorldContext&& worldcontext) = delete;

        [[nodiscard]] bool Initialize();
        void Tick(float delta_seconds);
        void Shutdown() noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] WorldType GetWorldType() const noexcept;
        [[nodiscard]] const World* GetWorld() const noexcept;
        [[nodiscard]] const GameInstance* GetGameInstance() const noexcept;
    private:
        enum class State
        {
            Created,
            Initialized,
            Failed,
            Shutdown
        };

        [[nodiscard]] bool HasValidObjectCombination() const noexcept;

        State m_state = State::Created;
        WorldType m_world_type = WorldType::None;
        std::unique_ptr<World> m_world;
        std::unique_ptr<GameInstance> m_game_instance;
    };
}
