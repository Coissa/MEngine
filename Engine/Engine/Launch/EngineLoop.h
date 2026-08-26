#pragma once

#include "EngineRunSettings.h"

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

namespace GE::Launch
{
    class EngineLoop final
    {
    public:
        EngineLoop() noexcept;
        explicit EngineLoop(
            std::unique_ptr<GE::Platform::PlatformApplication> platform_application
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
        std::unique_ptr<GE::Platform::PlatformApplication> m_platform_application;
    };
}
