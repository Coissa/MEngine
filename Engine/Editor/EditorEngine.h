#pragma once

#include <Engine/Runtime/Engine.h>

#include <cstdint>
#include <memory>

namespace GE::Runtime
{
    class WorldContext;
}

namespace GE::Editor
{
    class EditorEngine : public GE::Runtime::Engine
    {
    public:
        explicit EditorEngine(
            std::int32_t editor_scene_state = 0
        ) noexcept;
        ~EditorEngine() noexcept override;

        [[nodiscard]] bool Initialize() override;
        void Tick(float delta_seconds) override;
        void Shutdown() noexcept override;

        [[nodiscard]] bool StartPlaySession();
        void StopPlaySession() noexcept;

        [[nodiscard]] bool IsPlaySessionRunning() const noexcept;
        [[nodiscard]] const GE::Runtime::WorldContext*
        GetEditorWorldContext() const noexcept;
        [[nodiscard]] const GE::Runtime::WorldContext*
        GetPIEWorldContext() const noexcept;
    private:
        enum class State
        {
            Created,
            Initialized,
            Failed,
            Shutdown
        };

        std::int32_t m_editor_scene_state;
        std::unique_ptr<GE::Runtime::WorldContext> m_editor_world_context;
        std::unique_ptr<GE::Runtime::WorldContext> m_pie_world_context;
        State m_state = State::Created;
    };
}
