#include "TestRuntimeRegistries.h"
#include <Editor/EditorSession.h>
#include <Runtime/World.h>
#include <Runtime/WorldContext.h>
#include <Runtime/WorldType.h>

#include <cstdint>
#include <exception>
#include <iostream>

namespace
{
    [[nodiscard]] int Fail(int exit_code, const char* message)
    {
        std::cerr << "[PIESmoke] Failure: " << message << '\n';
        return exit_code;
    }
} // namespace

int main()
{
    try
    {
        constexpr std::int32_t editor_scene_state = 137;
        std::cout << "[PIESmoke] Begin\n";

        GE::Editor::EditorSession editor_engine{editor_scene_state};
        GE::Tests::RuntimeRegistries registries;
        const GE::Runtime::EngineServices services{registries.reflection, registries.serialization,
                                                   registries.components, registries.assets};
        if (editor_engine.GetEditorWorldContext() != nullptr || editor_engine.GetPIEWorldContext() != nullptr ||
            editor_engine.IsPlaySessionRunning())
        {
            return Fail(1, "new EditorSession exposed a WorldContext");
        }

        if (!editor_engine.Initialize(services))
        {
            return Fail(2, "EditorSession initialization failed");
        }

        const GE::Runtime::WorldContext* editor_context = editor_engine.GetEditorWorldContext();
        if (editor_context == nullptr || !editor_context->IsInitialized() ||
            editor_context->GetWorldType() != GE::Runtime::WorldType::Editor)
        {
            return Fail(3, "Editor WorldContext is invalid");
        }

        const GE::Runtime::World* editor_world = editor_context->GetWorld();
        if (editor_world == nullptr || !editor_world->IsInitialized() ||
            editor_world->GetSceneState() != editor_scene_state || editor_world->GetTickCount() != 0)
        {
            return Fail(4, "Editor World initial state is invalid");
        }
        std::cout << "[PIESmoke] Editor initialized\n";

        editor_engine.Tick({0.0, 0.0});
        if (editor_world->GetTickCount() != 0)
        {
            return Fail(5, "Editor World executed runtime Tick");
        }
        std::cout << "[PIESmoke] Editor ticked\n";

        if (!editor_engine.StartPlaySession())
        {
            return Fail(6, "first PIE session failed to start");
        }

        const GE::Runtime::WorldContext* first_pie_context = editor_engine.GetPIEWorldContext();
        if (!editor_engine.IsPlaySessionRunning() || first_pie_context == nullptr ||
            !first_pie_context->IsInitialized() || first_pie_context->GetWorldType() != GE::Runtime::WorldType::PIE)
        {
            return Fail(7, "first PIE WorldContext is invalid");
        }

        const GE::Runtime::World* first_pie_world = first_pie_context->GetWorld();
        if (first_pie_context == editor_context || first_pie_world == nullptr || first_pie_world == editor_world ||
            !first_pie_world->IsInitialized() || first_pie_world->GetSceneState() != editor_scene_state ||
            first_pie_world->GetTickCount() != 0 || editor_world->GetTickCount() != 0)
        {
            return Fail(8, "first PIE session is not isolated");
        }
        std::cout << "[PIESmoke] First PIE started\n";

        if (editor_engine.StartPlaySession())
        {
            return Fail(9, "second active PIE session was accepted");
        }
        if (editor_engine.GetPIEWorldContext() != first_pie_context ||
            first_pie_context->GetWorld() != first_pie_world || first_pie_world->GetTickCount() != 0)
        {
            return Fail(10, "rejected PIE start changed the active session");
        }
        std::cout << "[PIESmoke] Duplicate PIE rejected\n";

        editor_engine.Tick({0.0, 0.0});
        if (editor_world->GetTickCount() != 0 || first_pie_world->GetTickCount() != 1)
        {
            return Fail(11, "Editor and PIE Tick counts are invalid");
        }
        std::cout << "[PIESmoke] First PIE ticked\n";

        const double game_elapsed_before_pause = first_pie_context->GetGameElapsedSeconds();
        editor_engine.SetPlaySessionPaused(true);
        editor_engine.Tick({1.0, 10.0});
        if (!first_pie_context->IsPaused() || first_pie_world->GetTickCount() != 1 ||
            first_pie_context->GetGameElapsedSeconds() != game_elapsed_before_pause ||
            first_pie_context->GetRealElapsedSeconds() != 10.0)
            return Fail(19, "Pause did not freeze gameplay time");
        if (!editor_engine.RequestPlaySessionSingleStep() || !editor_engine.RequestPlaySessionSingleStep())
            return Fail(20, "paused single-step requests were rejected");
        editor_engine.Tick({1.0, 11.0});
        if (first_pie_world->GetTickCount() != 2)
            return Fail(21, "first single step did not execute one core frame");
        editor_engine.Tick({1.0, 12.0});
        if (first_pie_world->GetTickCount() != 3)
            return Fail(22, "single-step requests were not consumed one per frame");
        editor_engine.SetPlaySessionPaused(false);
        if (editor_engine.RequestPlaySessionSingleStep())
            return Fail(23, "single step was accepted while playing");

        editor_engine.StopPlaySession();
        if (editor_engine.IsPlaySessionRunning() || editor_engine.GetPIEWorldContext() != nullptr ||
            editor_engine.GetEditorWorldContext() != editor_context || editor_context->GetWorld() != editor_world ||
            editor_world->GetSceneState() != editor_scene_state || editor_world->GetTickCount() != 0)
        {
            return Fail(12, "stopping PIE damaged the Editor World");
        }
        std::cout << "[PIESmoke] First PIE stopped\n";

        editor_engine.StopPlaySession();
        if (editor_engine.IsPlaySessionRunning() || editor_engine.GetPIEWorldContext() != nullptr)
        {
            return Fail(13, "repeated StopPlaySession changed state");
        }
        std::cout << "[PIESmoke] Repeated stop safe\n";

        if (!editor_engine.StartPlaySession())
        {
            return Fail(14, "second PIE session failed to start");
        }

        const GE::Runtime::WorldContext* second_pie_context = editor_engine.GetPIEWorldContext();
        if (!editor_engine.IsPlaySessionRunning() || second_pie_context == nullptr ||
            second_pie_context == editor_context || !second_pie_context->IsInitialized() ||
            second_pie_context->GetWorldType() != GE::Runtime::WorldType::PIE)
        {
            return Fail(15, "second PIE WorldContext is invalid");
        }

        const GE::Runtime::World* second_pie_world = second_pie_context->GetWorld();
        if (second_pie_world == nullptr || second_pie_world == editor_world || !second_pie_world->IsInitialized() ||
            second_pie_world->GetSceneState() != editor_scene_state || second_pie_world->GetTickCount() != 0 ||
            editor_world->GetTickCount() != 0)
        {
            return Fail(16, "second PIE session did not start fresh");
        }
        std::cout << "[PIESmoke] Second PIE started\n";

        editor_engine.Shutdown();
        if (editor_engine.IsPlaySessionRunning() || editor_engine.GetPIEWorldContext() != nullptr ||
            editor_engine.GetEditorWorldContext() != nullptr)
        {
            return Fail(17, "EditorSession Shutdown retained a WorldContext");
        }
        std::cout << "[PIESmoke] Shutdown complete\n";

        editor_engine.Shutdown();
        if (editor_engine.IsPlaySessionRunning() || editor_engine.GetPIEWorldContext() != nullptr ||
            editor_engine.GetEditorWorldContext() != nullptr)
        {
            return Fail(18, "repeated EditorSession Shutdown changed state");
        }

        std::cout << "[PIESmoke] Passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[PIESmoke] Unhandled exception: " << exception.what() << '\n';
        return 100;
    }
    catch (...)
    {
        std::cerr << "[PIESmoke] Unknown exception\n";
        return 101;
    }
}
