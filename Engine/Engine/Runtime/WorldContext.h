#pragma once

#include "FixedStepScheduler.h"
#include "WorldType.h"
#include <cstdint>
#include <memory>

namespace GE::Runtime
{
    class World;
    class GameInstance;

    class WorldContext final
    {
    public:
        WorldContext(WorldType world_type, std::unique_ptr<World> world,
            std::unique_ptr<GameInstance> game_instance = nullptr,
            FixedStepSettings fixed_step_settings = {});
        ~WorldContext() noexcept;
        WorldContext(const WorldContext&) = delete;
        WorldContext& operator=(const WorldContext&) = delete;

        [[nodiscard]] bool Initialize();
        void Tick(double real_delta_seconds, double real_elapsed_seconds);
        void Shutdown() noexcept;
        void SetPaused(bool paused) noexcept;
        [[nodiscard]] bool IsPaused() const noexcept;
        [[nodiscard]] bool RequestSingleStep() noexcept;
        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] WorldType GetWorldType() const noexcept;
        [[nodiscard]] const World* GetWorld() const noexcept;
        [[nodiscard]] World* GetWorld() noexcept;
        [[nodiscard]] const GameInstance* GetGameInstance() const noexcept;
        [[nodiscard]] double GetGameDeltaSeconds() const noexcept;
        [[nodiscard]] double GetRealDeltaSeconds() const noexcept;
        [[nodiscard]] double GetRealElapsedSeconds() const noexcept;
        [[nodiscard]] double GetGameElapsedSeconds() const noexcept;
        [[nodiscard]] double GetSimulationElapsedSeconds() const noexcept;
        [[nodiscard]] double GetInterpolationAlpha() const noexcept;
        [[nodiscard]] std::uint32_t GetExecutedSubstepCount() const noexcept;
        [[nodiscard]] double GetDroppedTimeSeconds() const noexcept;

    private:
        enum class State { Created, Initialized, Failed, Shutdown };
        [[nodiscard]] bool HasValidObjectCombination() const noexcept;
        State m_state = State::Created;
        WorldType m_world_type = WorldType::None;
        std::unique_ptr<World> m_world;
        std::unique_ptr<GameInstance> m_game_instance;
        std::unique_ptr<FixedStepScheduler> m_scheduler;
        double m_game_delta_seconds = 0.0;
        double m_real_delta_seconds = 0.0;
        double m_real_elapsed_seconds = 0.0;
        double m_game_elapsed_seconds = 0.0;
        double m_simulation_elapsed_seconds = 0.0;
        double m_interpolation_alpha = 0.0;
        std::uint32_t m_executed_substep_count = 0;
        double m_dropped_time_seconds = 0.0;
    };
}
