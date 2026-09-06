#pragma once

#include <Engine/Runtime/Engine.h>
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
            std::int32_t initial_scene_state = 0
        ) noexcept;
        explicit GameEngine(std::filesystem::path scene_path);
        ~GameEngine() noexcept override;

        [[nodiscard]] bool Initialize() override;
        void Tick(float delta_seconds) override;
        void Shutdown() noexcept override;
        [[nodiscard]] bool BuildRenderFrame(
            GE::RenderCore::RenderFrame& out_frame) noexcept override;
        [[nodiscard]] const WorldContext* GetWorldContext() const noexcept;
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
        State m_state = State::Created;
    };
}
