#pragma once

#include <memory>

namespace GE::Reflection
{
    class TypeRegistry;
}
namespace GE::Runtime
{
    class Engine;
}

namespace GE::Launch
{
    class EngineLoop final
    {
    public:
        EngineLoop();
        ~EngineLoop() noexcept;

        EngineLoop(const EngineLoop&) = delete;
        EngineLoop& operator=(const EngineLoop&) = delete;
        EngineLoop(EngineLoop&&) = delete;
        EngineLoop& operator=(EngineLoop&&) = delete;

        [[nodiscard]] bool Initialize();
        void Shutdown() noexcept;
        [[nodiscard]] int Run(
            std::unique_ptr<GE::Runtime::Engine> engine
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
    };
}
