#pragma once

#include "EngineRunSettings.h"

#include <Engine/RenderCore/RenderFrame.h>

#include <memory>

namespace GE::Reflection
{
    class TypeRegistry;
}

namespace GE::Runtime
{
    class Engine;
}

namespace GE::Platform
{
    class PlatformApplication;
}

namespace GE::Render
{
    class Renderer;
}

namespace GE::Launch
{
    class EngineLoop final
    {
    public:
        // 由 composition root 注入纯值渲染快照的构建方式；Launch 不认识具体场景句柄。
        using RenderFrameBuilder =
            void (*)(float, GE::RenderCore::RenderFrame&) noexcept;

        EngineLoop() noexcept;
        explicit EngineLoop(
            std::unique_ptr<GE::Platform::PlatformApplication> platform_application
        ) noexcept;
        EngineLoop(
            std::unique_ptr<GE::Platform::PlatformApplication> platform_application,
            std::unique_ptr<GE::Render::Renderer> renderer,
            RenderFrameBuilder render_frame_builder = nullptr
        ) noexcept;
        ~EngineLoop() noexcept;

        EngineLoop(const EngineLoop&) = delete;
        EngineLoop& operator=(const EngineLoop&) = delete;
        EngineLoop(EngineLoop&&) = delete;
        EngineLoop& operator=(EngineLoop&&) = delete;

        [[nodiscard]] bool Initialize();
        void Shutdown() noexcept;
        [[nodiscard]] int Run(
            std::unique_ptr<GE::Runtime::Engine> engine,
            EngineRunSettings settings = {}
        );
    private:
        enum class State
        {
            Created,
            Initialized,
            Failed,
            Running,
            Ran,
            Shutdown
        };

        State m_state = State::Created;
        std::unique_ptr<GE::Reflection::TypeRegistry> m_type_registry;
        std::unique_ptr<GE::Runtime::Engine> m_engine;
        std::unique_ptr<GE::Render::Renderer> m_renderer;
        std::unique_ptr<GE::Platform::PlatformApplication> m_platform_application;
        RenderFrameBuilder m_render_frame_builder = nullptr;
    };
}
