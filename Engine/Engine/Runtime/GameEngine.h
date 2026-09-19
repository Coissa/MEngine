#pragma once

#include <Engine/Runtime/Engine.h>
#include <Engine/Runtime/FixedStepScheduler.h>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace GE::Runtime
{
    class WorldContext;

    class GameEngine : public Engine
    {
    public:
        explicit GameEngine(
            std::int32_t initial_scene_state = 0,
            FixedStepSettings fixed_step_settings = {}
        ) noexcept;
        explicit GameEngine(std::filesystem::path scene_path,
            FixedStepSettings fixed_step_settings = {});
        ~GameEngine() noexcept override;

        [[nodiscard]] bool Initialize(const EngineServices& services) override;
        void Tick(const EngineFrameTime& frame_time) override;
        void Shutdown() noexcept override;
        [[nodiscard]] bool BuildRenderFrame(
            GE::RenderCore::RenderFrame& out_frame) noexcept override;
        [[nodiscard]] const WorldContext* GetWorldContext() const noexcept;
        void SetPaused(bool paused) noexcept;
        [[nodiscard]] bool RequestSingleStep() noexcept;
    private:
        enum class State
        {
            Created,
            Initialized,
            Failed,
            Shutdown
        };

        std::int32_t m_initial_scene_state;
        std::filesystem::path m_initial_scene_path;
        std::unique_ptr<WorldContext> m_world_context;
        FixedStepSettings m_fixed_step_settings;
        State m_state = State::Created;
    };
}
