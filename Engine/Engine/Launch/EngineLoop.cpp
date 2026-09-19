#include "EngineLoop.h"

#include "BuiltinTypeRegistration.h"
#include "Engine/Core/Version.h"
#include "TypeRegistry.h"

#include <Engine/Asset/AssetRegistry.h>
#include <Engine/Asset/AssetRegistrySerializer.h>
#include <Engine/Asset/AssetSerialization.h>
#include <Engine/Asset/ProjectDescriptor.h>
#include <Engine/Core/Diagnostics/EngineError.h>
#include <Engine/Core/Time/FrameClock.h>
#include <Engine/Platform/PlatformApplication.h>
#include <Engine/RenderCore/RenderFrame.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Runtime/Engine.h>
#include <Engine/Runtime/ComponentRegistry.h>
#include <Engine/Runtime/EngineServices.h>
#include <Engine/Serialization/BuiltinSerialization.h>
#include <Engine/Serialization/SerializationRegistry.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

namespace GE::Launch
{
    namespace
    {
        [[nodiscard]] bool IsValidRunSettings(
            const EngineRunSettings& settings) noexcept
        {
            if (!settings.frame_delta_override_seconds.has_value())
            {
                return true;
            }

            const float frame_delta_override_seconds =
                *settings.frame_delta_override_seconds;

            return std::isfinite(frame_delta_override_seconds)
                && frame_delta_override_seconds >= 0.0F;
        }
    }

    EngineLoop::EngineLoop(std::filesystem::path asset_root) noexcept
        : m_asset_root(std::move(asset_root))
    {
    }

    EngineLoop::EngineLoop(
        std::filesystem::path asset_root,
        std::unique_ptr<GE::Platform::PlatformApplication> platform_application
    ) noexcept
        : EngineLoop(
            std::move(asset_root), std::move(platform_application), nullptr)
    {
    }

    EngineLoop::EngineLoop(
        std::filesystem::path asset_root,
        std::unique_ptr<GE::Platform::PlatformApplication> platform_application,
        std::unique_ptr<GE::Render::Renderer> renderer,
        RenderFrameBuilder render_frame_builder
    ) noexcept
        // 函数指针不拥有场景，也不能捕获 World/Object；仅在当前主线程同步调用。
        : m_asset_root(std::move(asset_root)),
          m_renderer(std::move(renderer)),
          m_platform_application(std::move(platform_application)),
          m_render_frame_builder(render_frame_builder)
    {
    }

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
            if (m_platform_application != nullptr)
            {
                m_platform_application->SetMessageHandler(nullptr, nullptr);
            }
            m_engine->Shutdown();
            m_engine.reset();
        }

        if (m_renderer != nullptr)
        {
            m_renderer->Shutdown();
            m_renderer.reset();
        }

        m_serialization_registry.reset();
        m_type_registry.reset();
        m_component_registry.reset();
        m_asset_registry.reset();
        m_platform_application.reset();

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

        auto project_result = GE::Asset::ProjectDescriptorSerializer::LoadFromFile(
            m_asset_root / "MEngine.project");
        if (!project_result)
        {
            std::cerr << "[MEngineLaunch] "
                      << GE::Core::FormatEngineError(project_result.Error())
                      << '\n';
            return false;
        }
        auto asset_result = GE::Asset::AssetRegistrySerializer::LoadFromFile(
            m_asset_root / "AssetRegistry.mregistry",
            project_result.Value().namespace_id);
        if (!asset_result)
        {
            std::cerr << "[MEngineLaunch] "
                      << GE::Core::FormatEngineError(asset_result.Error())
                      << '\n';
            return false;
        }
        auto asset_registry = std::make_unique<GE::Asset::AssetRegistry>(
            std::move(asset_result).Value());

        auto registry = std::make_unique<GE::Reflection::TypeRegistry>();
        auto serialization_registry =
            std::make_unique<GE::Serialization::SerializationRegistry>();
        auto component_registry =
            std::make_unique<GE::Runtime::ComponentRegistry>();
        const auto registration_result =
            GE::Reflection::RegisterBuiltinTypes(*registry);
        if (!registration_result)
        {
            std::cerr
                << "[MEngineLaunch] RegisterBuiltinTypes failed: "
                << GE::Core::FormatEngineError(registration_result.Error()) << '\n';
            return false;
        }

        auto serialization_result =
            GE::Serialization::RegisterBuiltinSerialization(
                *registry, *serialization_registry);
        if (!serialization_result)
        {
            std::cerr << "[MEngineLaunch] Builtin serialization registration failed: "
                      << GE::Core::FormatEngineError(serialization_result.Error()) << '\n';
            return false;
        }
        serialization_result = GE::Asset::RegisterAssetSerialization(
            *registry, *serialization_registry);
        if (!serialization_result)
        {
            std::cerr << "[MEngineLaunch] Asset serialization registration failed: "
                      << GE::Core::FormatEngineError(serialization_result.Error()) << '\n';
            return false;
        }
        auto runtime_reflection_result =
            GE::Runtime::RegisterBuiltinComponentReflection(*registry);
        if (!runtime_reflection_result)
        {
            std::cerr << "[MEngineLaunch] Runtime reflection registration failed: "
                      << GE::Core::FormatEngineError(runtime_reflection_result.Error()) << '\n';
            return false;
        }

        std::string component_error;
        if (!GE::Runtime::RegisterBuiltinComponents(
                *registry, *component_registry, component_error))
        {
            std::cerr << "[MEngineLaunch] Component registration failed: "
                      << component_error << '\n';
            return false;
        }

        const auto freeze_result = registry->Freeze();
        if (!freeze_result)
        {
            std::cerr
                << "[MEngineLaunch] Reflection freeze failed: "
                << GE::Core::FormatEngineError(freeze_result.Error()) << '\n';
            return false;
        }
        const auto serialization_freeze_result =
            serialization_registry->Freeze(*registry);
        if (!serialization_freeze_result)
        {
            std::cerr << "[MEngineLaunch] Serialization freeze failed: "
                      << GE::Core::FormatEngineError(
                             serialization_freeze_result.Error()) << '\n';
            return false;
        }
        if (component_registry->Freeze()
            != GE::Runtime::ComponentRegistry::FreezeResult::Success)
        {
            std::cerr << "[MEngineLaunch] Component registry freeze failed\n";
            return false;
        }

        if (m_renderer != nullptr && !m_renderer->Initialize())
        {
            std::cerr << "[MEngineLaunch] Renderer initialization failed\n";
            m_renderer->Shutdown();
            return false;
        }

        m_type_registry = std::move(registry);
        m_serialization_registry = std::move(serialization_registry);
        m_component_registry = std::move(component_registry);
        m_asset_registry = std::move(asset_registry);
        m_state = State::Initialized;
        std::cout << "[MEngineLaunch] Reflection ready\n";
        return true;
    }

    [[nodiscard]] int EngineLoop::Run(
        std::unique_ptr<GE::Runtime::Engine> engine,
        EngineRunSettings settings)
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

        if (!IsValidRunSettings(settings))
        {
            std::cerr << "[MEngineLaunch] Invalid run settings\n";
            m_state = State::Failed;
            return 1;
        }

        m_state = State::Running;
        m_engine = std::move(engine);

        try
        {
            const GE::Runtime::EngineServices services{
                *m_type_registry, *m_serialization_registry,
                *m_component_registry, *m_asset_registry};
            const bool init_state = m_engine->Initialize(services);
            if (init_state == false)
            {
                std::cerr << "[MEngineLaunch] Engine initialization failed\n";
                if (m_platform_application != nullptr)
                {
                    m_platform_application->SetMessageHandler(nullptr, nullptr);
                }
                m_engine->Shutdown();
                m_engine.reset();
                m_state = State::Failed;
                return 1;
            }

            std::cout << "[MEngineLaunch] Run\n";
            GE::Core::FrameClock frame_clock;
            std::uint64_t completed_frame_count = 0;
            double elapsed_seconds = 0.0;

            while (!m_engine->IsExitRequested()
                && (!settings.max_frames.has_value()
                || completed_frame_count < *settings.max_frames))
            {
                if (m_platform_application != nullptr
                    && !m_platform_application->PumpMessages())
                {
                    break;
                }
                const float delta_seconds =
                    settings.frame_delta_override_seconds.has_value()
                    ? *settings.frame_delta_override_seconds
                    : frame_clock.Sample();
                elapsed_seconds += delta_seconds;

                m_engine->Tick({delta_seconds, elapsed_seconds});

                if (m_renderer != nullptr)
                {
                    GE::Platform::WindowSize window_size;
                    if (m_platform_application != nullptr)
                    {
                        window_size =
                            m_platform_application->GetClientSize();
                    }

                    GE::RenderCore::RenderFrame render_frame{
                        .frame_number = completed_frame_count,
                        .output_extent = {
                            .width = window_size.width,
                            .height = window_size.height
                        },
                        .real_elapsed_seconds = elapsed_seconds
                    };
                    if (!m_engine->BuildRenderFrame(render_frame))
                    {
                        std::cerr
                            << "[MEngineLaunch] Render frame extraction failed\n";
                        if (m_platform_application != nullptr)
                        {
                            m_platform_application->SetMessageHandler(
                                nullptr, nullptr);
                        }
                        m_engine->Shutdown();
                        m_engine.reset();
                        m_state = State::Failed;
                        return 1;
                    }
                    if (m_render_frame_builder)
                    {
                        m_render_frame_builder(
                            static_cast<float>(elapsed_seconds), render_frame);
                    }

                    if (!m_renderer->SubmitFrame(std::move(render_frame)))
                    {
                        std::cerr
                            << "[MEngineLaunch] Renderer frame submission failed\n";
                        if (m_platform_application != nullptr)
                        {
                            m_platform_application->SetMessageHandler(
                                nullptr, nullptr);
                        }
                        m_engine->Shutdown();
                        m_engine.reset();
                        m_state = State::Failed;
                        return 1;
                    }
                }

                ++completed_frame_count;
            }

            if (m_platform_application != nullptr)
            {
                m_platform_application->SetMessageHandler(nullptr, nullptr);
            }
            m_engine->Shutdown();
            m_engine.reset();
            m_state = State::Ran;
            return 0;
        }
        catch (...)
        {
            Shutdown();
            throw;
        }
    }
}
