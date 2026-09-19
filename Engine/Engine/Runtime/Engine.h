#pragma once

#include <Engine/RenderCore/RenderFrame.h>
#include "EngineServices.h"

namespace GE::Runtime
{
    struct EngineFrameTime final
    {
        double real_delta_seconds = 0.0;
        double real_elapsed_seconds = 0.0;
    };

    class Engine
    {
    public:
        virtual ~Engine() noexcept;

        [[nodiscard]] virtual bool Initialize(const EngineServices& services) = 0;
        virtual void Tick(const EngineFrameTime& frame_time) = 0;
        virtual void Shutdown() noexcept = 0;
        [[nodiscard]] virtual bool BuildRenderFrame(
            GE::RenderCore::RenderFrame& out_frame) noexcept;
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        void RequestExit() noexcept;
        [[nodiscard]] bool IsExitRequested() const noexcept;
    protected:
        Engine() noexcept;

    private:
        bool m_exit_requested = false;
    };
}
