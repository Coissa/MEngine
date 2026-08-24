#pragma once

namespace GE::Runtime
{
    class Engine
    {
    public:
        virtual ~Engine() noexcept;

        [[nodiscard]] virtual bool Initialize() = 0;
        virtual void Tick(float delta_seconds) = 0;
        virtual void Shutdown() noexcept = 0;
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;
    protected:
        Engine() noexcept;
    };
}
