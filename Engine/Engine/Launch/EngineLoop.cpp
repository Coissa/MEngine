#include "EngineLoop.h"

#include "BuiltinTypeRegistration.h"
#include "Engine/Core/Version.h"
#include "TypeRegistry.h"

#include <Engine/Runtime/Engine.h>

#include <iostream>
#include <memory>
#include <utility>

namespace GE::Launch
{
    EngineLoop::EngineLoop() = default;

    EngineLoop::~EngineLoop() noexcept
    {
        Shutdown();
    }

    void EngineLoop::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }
        if (m_engine != nullptr)
        {
            m_engine->Shutdown();
            m_engine.reset();
        }

        m_type_registry.reset();
        m_state = State::Shutdown;

        try
        {
            std::cout << "[MEngineLaunch] Shutdown\n";
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] bool EngineLoop::Initialize()
    {
        if (m_state != State::Created)
        {
            std::cerr << "[MEngineLaunch] Initialize called in invalid state\n";
            return false;
        }
        m_state = State::Failed;

        std::cout << "[MEngineLaunch] Initialize\n";
        std::cout << "MEngine " << GE::EngineVersion << '\n';

        auto registry = std::make_unique<GE::Reflection::TypeRegistry>();
        const auto registration_result =
            GE::Reflection::RegisterBuiltinTypes(*registry);
        if (registration_result != GE::Reflection::TypeRegistry::RegisterTypeResult::Success)
        {
            std::cerr
                << "[MEngineLaunch] RegisterBuiltinTypes failed: "
                << static_cast<int>(registration_result) << '\n';
            return false;
        }

        const auto freeze_result = registry->Freeze();
        if (freeze_result != GE::Reflection::TypeRegistry::FreezeResult::Success)
        {
            std::cerr
                << "[MEngineLaunch] Reflection freeze failed: "
                << static_cast<int>(freeze_result) << '\n';
            return false;
        }

        m_type_registry = std::move(registry);
        m_state = State::Initialized;
        std::cout << "[MEngineLaunch] Reflection ready\n";
        return true;
    }

    [[nodiscard]] int EngineLoop::Run(std::unique_ptr<GE::Runtime::Engine> engine)
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[MEngineLaunch] Run called in invalid state\n";
            return 1;
        }

        if (engine == nullptr)
        {
            std::cerr << "[MEngineLaunch] Run requires an Engine instance\n";
            m_state = State::Failed;
            return 1;
        }

        m_state = State::Running;
        m_engine = std::move(engine);

        const bool init_state = m_engine->Initialize();
        if (init_state == false)
        {
            std::cerr << "[MEngineLaunch] Engine initialization failed\n";
            m_engine->Shutdown();
            m_engine.reset();
            m_state = State::Failed;
            return 1;
        }

        std::cout << "[MEngineLaunch] Run\n";
        m_engine->Tick(0.0F);
        m_engine->Shutdown();
        m_engine.reset();
        m_state = State::Ran;
        return 0;
    }
}
