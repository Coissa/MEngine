#include "EditorEngine.h"

#include <Engine/Runtime/GameInstance.h>
#include <Engine/Runtime/World.h>
#include <Engine/Runtime/WorldContext.h>
#include <Engine/Runtime/WorldType.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace GE::Editor
{
    EditorEngine::EditorEngine(std::int32_t editor_scene_state) noexcept
        : m_editor_scene_state(editor_scene_state)
    {
    }

    EditorEngine::~EditorEngine() noexcept
    {
        Shutdown();
    }

    bool EditorEngine::Initialize()
    {
        if (m_state != State::Created)
        {
            std::cerr << "[EditorEngine] Initialize called in invalid state\n";
            return false;
        }

        m_state = State::Failed;
        std::cout << "[EditorEngine] Initialize\n";

        auto editor_world =
            std::make_unique<GE::Runtime::World>(m_editor_scene_state);
        auto editor_world_context =
            std::make_unique<GE::Runtime::WorldContext>(
                GE::Runtime::WorldType::Editor,
                std::move(editor_world)
            );
        if (!editor_world_context->Initialize())
        {
            return false;
        }

        m_editor_world_context = std::move(editor_world_context);
        m_state = State::Initialized;
        return true;
    }

    void EditorEngine::Tick(float delta_seconds)
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[EditorEngine] Engine not Initialized\n";
            throw std::logic_error{
                "EditorEngine::Tick called before successful initialization"
            };
        }
        if (!std::isfinite(delta_seconds) || delta_seconds < 0.0F)
        {
            throw std::invalid_argument{
                "delta_seconds must be finite and non-negative"
            };
        }

        std::cout << "[EditorEngine] Tick\n";

        m_editor_world_context->Tick(delta_seconds);
        if (m_pie_world_context != nullptr)
        {
            m_pie_world_context->Tick(delta_seconds);
        }
    }

    void EditorEngine::Shutdown() noexcept
    {
        if (m_state == State::Shutdown)
        {
            return;
        }

        m_state = State::Shutdown;
        try
        {
            std::cout << "[EditorEngine] Shutdown\n";
        }
        catch (...)
        {
        }

        StopPlaySession();
        if (m_editor_world_context != nullptr)
        {
            m_editor_world_context->Shutdown();
            m_editor_world_context.reset();
        }
    }

    [[nodiscard]] bool EditorEngine::StartPlaySession()
    {
        if (m_state != State::Initialized)
        {
            std::cerr << "[EditorEngine] StartPlaySession called in invalid state\n";
            return false;
        }
        if (m_pie_world_context != nullptr)
        {
            std::cerr << "[EditorEngine] Play session is already running\n";
            return false;
        }

        const GE::Runtime::World* editor_world =
            m_editor_world_context->GetWorld();
        if (editor_world == nullptr)
        {
            std::cerr << "[EditorEngine] Editor World is unavailable\n";
            return false;
        }

        std::cout << "[EditorEngine] StartPlaySession\n";
        auto pie_world = editor_world->CreatePIECopy();
        if (pie_world == nullptr)
        {
            return false;
        }

        auto game_instance = std::make_unique<GE::Runtime::GameInstance>();
        auto pie_world_context =
            std::make_unique<GE::Runtime::WorldContext>(
                GE::Runtime::WorldType::PIE,
                std::move(pie_world),
                std::move(game_instance)
            );
        if (!pie_world_context->Initialize())
        {
            return false;
        }

        m_pie_world_context = std::move(pie_world_context);
        return true;
    }

    void EditorEngine::StopPlaySession() noexcept
    {
        if (m_pie_world_context == nullptr)
        {
            return;
        }

        try
        {
            std::cout << "[EditorEngine] StopPlaySession\n";
        }
        catch (...)
        {
        }

        m_pie_world_context->Shutdown();
        m_pie_world_context.reset();
    }

    [[nodiscard]] bool EditorEngine::IsPlaySessionRunning() const noexcept
    {
        return m_pie_world_context != nullptr
            && m_pie_world_context->IsInitialized();
    }

    [[nodiscard]] const GE::Runtime::WorldContext*
    EditorEngine::GetEditorWorldContext() const noexcept
    {
        return m_editor_world_context.get();
    }

    [[nodiscard]] const GE::Runtime::WorldContext*
    EditorEngine::GetPIEWorldContext() const noexcept
    {
        return m_pie_world_context.get();
    }
}
