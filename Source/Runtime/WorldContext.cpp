#include "WorldContext.h"
#include "World.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace GE::Runtime
{
    namespace
    {
        const char* WorldTypeName(WorldType type) noexcept
        {
            switch (type)
            {
            case WorldType::Editor:
                return "Editor";
            case WorldType::PIE:
                return "PIE";
            default:
                return "None";
            }
        }
    } // namespace

    WorldContext::WorldContext(WorldType world_type, std::unique_ptr<World> world, FixedStepSettings settings)
        : m_world_type(world_type), m_world(std::move(world))
    {
        if (world_type == WorldType::PIE)
            m_scheduler = std::make_unique<FixedStepScheduler>(settings);
    }

    WorldContext::~WorldContext() noexcept
    {
        Shutdown();
    }

    bool WorldContext::Initialize()
    {
        if (m_state != State::Created || !HasValidObjectCombination())
            return false;
        m_state = State::Failed;
        std::cout << "[WorldContext " << WorldTypeName(m_world_type) << "] Initialize\n";
        if (!m_world->Initialize())
        {
            return false;
        }
        if (m_world_type != WorldType::Editor && !m_world->StartGameplay())
        {
            m_world->Shutdown();
            return false;
        }
        m_state = State::Initialized;
        return true;
    }

    void WorldContext::Tick(double real_delta_seconds, double real_elapsed_seconds)
    {
        assert(m_state == State::Initialized);
        m_real_delta_seconds = real_delta_seconds;
        m_real_elapsed_seconds = real_elapsed_seconds;
        if (m_world_type == WorldType::Editor)
        {
            m_game_delta_seconds = 0.0;
            m_executed_substep_count = 0;
            return;
        }

        const FixedStepFrame frame = m_scheduler->Advance(real_delta_seconds);
        m_game_delta_seconds = frame.game_delta_seconds;
        m_game_elapsed_seconds += frame.game_delta_seconds;
        m_executed_substep_count = frame.substep_count;
        m_simulation_elapsed_seconds += frame.fixed_delta_seconds * frame.substep_count;
        m_interpolation_alpha = frame.interpolation_alpha;
        m_dropped_time_seconds += frame.dropped_seconds;

        for (std::uint32_t step = 0; step < frame.substep_count; ++step)
            m_world->FixedTick(static_cast<float>(frame.fixed_delta_seconds));

        if (!m_scheduler->IsPaused() || frame.single_step)
        {
            const float delta =
                static_cast<float>(frame.single_step ? frame.fixed_delta_seconds : frame.game_delta_seconds);
            m_world->Tick(delta);
        }
    }

    void WorldContext::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
            return;
        m_state = State::Shutdown;
        try
        {
            std::cout << "[WorldContext " << WorldTypeName(m_world_type) << "] Shutdown\n";
        }
        catch (...)
        {
        }
        if (m_world != nullptr)
            m_world->Shutdown();
    }
    void WorldContext::SetPaused(bool value) noexcept
    {
        if (m_scheduler != nullptr)
            m_scheduler->SetPaused(value);
    }
    bool WorldContext::IsPaused() const noexcept
    {
        return m_scheduler != nullptr && m_scheduler->IsPaused();
    }
    bool WorldContext::RequestSingleStep() noexcept
    {
        return m_scheduler != nullptr && m_scheduler->RequestSingleStep();
    }
    bool WorldContext::IsInitialized() const noexcept
    {
        return m_state == State::Initialized;
    }
    WorldType WorldContext::GetWorldType() const noexcept
    {
        return m_world_type;
    }
    const World* WorldContext::GetWorld() const noexcept
    {
        return m_world.get();
    }
    World* WorldContext::GetWorld() noexcept
    {
        return m_world.get();
    }
    double WorldContext::GetGameDeltaSeconds() const noexcept
    {
        return m_game_delta_seconds;
    }
    double WorldContext::GetRealDeltaSeconds() const noexcept
    {
        return m_real_delta_seconds;
    }
    double WorldContext::GetRealElapsedSeconds() const noexcept
    {
        return m_real_elapsed_seconds;
    }
    double WorldContext::GetGameElapsedSeconds() const noexcept
    {
        return m_game_elapsed_seconds;
    }
    double WorldContext::GetSimulationElapsedSeconds() const noexcept
    {
        return m_simulation_elapsed_seconds;
    }
    double WorldContext::GetInterpolationAlpha() const noexcept
    {
        return m_interpolation_alpha;
    }
    std::uint32_t WorldContext::GetExecutedSubstepCount() const noexcept
    {
        return m_executed_substep_count;
    }
    double WorldContext::GetDroppedTimeSeconds() const noexcept
    {
        return m_dropped_time_seconds;
    }

    bool WorldContext::HasValidObjectCombination() const noexcept
    {
        return m_world &&
               ((m_world_type == WorldType::Editor && !m_scheduler) || (m_world_type == WorldType::PIE && m_scheduler));
    }
} // namespace GE::Runtime
