#include "EditorEngine.h"
#include "D3D12EditorUIResources.h"
#include "EditorUserInterface.h"

#include <Engine/Launch/EngineLoop.h>
#include <Engine/Platform/Windows/Win32Application.h>
#include <Engine/RHI/D3D12/D3D12BackendFactory.h>
#include <Engine/Renderer/Examples/D3D12RotatingCubeExample.h>
#include <Engine/Renderer/RHIRenderer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    [[nodiscard]] bool IsSmokeRun(int argument_count, char** arguments) noexcept
    {
        return argument_count > 1 && arguments != nullptr
            && std::string_view(arguments[1]) == "--smoke";
    }

    [[nodiscard]] std::optional<std::filesystem::path> FindScenePath(
        int argument_count,
        char** arguments)
    {
        for (int index = 1; index + 1 < argument_count; ++index)
        {
            if (std::string_view(arguments[index]) == "--scene")
            {
                return std::filesystem::path(arguments[index + 1]);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool BuildMeshBounds(
        const GE::Render::RHIMeshDescription& mesh,
        GE::Editor::EditorMeshBounds& out_bounds) noexcept
    {
        if (!mesh.handle.IsValid() || mesh.vertices.empty())
        {
            return false;
        }
        const std::array<float, 3>& first = mesh.vertices.front().position;
        if (!std::isfinite(first[0]) || !std::isfinite(first[1])
            || !std::isfinite(first[2]))
        {
            return false;
        }
        out_bounds.minimum = {first[0], first[1], first[2]};
        out_bounds.maximum = out_bounds.minimum;
        for (const GE::Render::PositionColorTextureVertex& vertex
            : mesh.vertices)
        {
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                const float value = vertex.position[axis];
                if (!std::isfinite(value))
                {
                    return false;
                }
                out_bounds.minimum[axis] =
                    (std::min)(out_bounds.minimum[axis], value);
                out_bounds.maximum[axis] =
                    (std::max)(out_bounds.maximum[axis], value);
            }
        }
        return out_bounds.IsValid();
    }
}

int main(int argument_count, char** arguments)
{
    try
    {
        constexpr GE::Platform::WindowSize InitialSize{
            .width = 1280, .height = 720};
        auto application = GE::Platform::Windows::Win32Application::Create({
            .title = "MEngine Editor - Rotating Cube",
            .client_size = InitialSize});
        if (application == nullptr)
        {
            std::cerr << "[MEngineEditor] Window creation failed\n";
            return 1;
        }

        const auto native_window = application->GetNativeWindowHandle();
        const std::optional<std::filesystem::path> scene_path =
            FindScenePath(argument_count, arguments);
        auto editor_engine = scene_path
            ? std::make_unique<GE::Editor::EditorEngine>(*scene_path)
            : std::make_unique<GE::Editor::EditorEngine>();
        GE::Editor::EditorFontAtlas font_atlas;
        std::string ui_error;
        if (!editor_engine->InitializeUserInterface(
                reinterpret_cast<std::uintptr_t>(native_window.window),
                font_atlas,
                ui_error))
        {
            std::cerr << "[MEngineEditor] UI initialization failed: "
                      << ui_error << '\n';
            return 1;
        }
        application->SetMessageHandler(
            &GE::Editor::EditorEngine::HandlePlatformMessage,
            editor_engine.get());

        GE::Render::RHIRenderScene scene;
        std::string scene_error;
        if (!GE::Render::Examples::BuildD3D12RotatingCubeScene(
                scene, scene_error))
        {
            std::cerr << "[MEngineEditor] Cube scene creation failed: "
                      << scene_error << '\n';
            return 1;
        }
        for (const GE::Render::RHIMeshDescription& mesh : scene.meshes)
        {
            GE::Editor::EditorMeshBounds bounds;
            if (!BuildMeshBounds(mesh, bounds)
                || !editor_engine->RegisterMeshBounds(mesh.handle, bounds))
            {
                std::cerr
                    << "[MEngineEditor] Invalid or duplicate mesh bounds\n";
                return 1;
            }
        }
        if (!GE::Editor::AddD3D12EditorUIResources(
                font_atlas, scene, scene_error))
        {
            std::cerr << "[MEngineEditor] UI render resources failed: "
                      << scene_error << '\n';
            return 1;
        }
        auto renderer = std::make_unique<GE::Render::RHIRenderer>(
            GE::Render::RHIRendererCreateInfo{
                .backend = GE::D3D12RHI::CreateD3D12Backend(),
                .surface = {
                    .system = GE::RHI::NativeWindowSystem::Win32,
                    .display_handle = reinterpret_cast<std::uintptr_t>(
                        native_window.instance),
                    .window_handle = reinterpret_cast<std::uintptr_t>(
                        native_window.window)},
                .initial_extent = {
                    .width = InitialSize.width,
                    .height = InitialSize.height},
                .scene = std::move(scene)});

        application->Show();
        // Editor/PIE 共用该进程级 Renderer；回调只生成当前帧的值快照。
        GE::Launch::EngineLoop engine_loop{
            std::move(application),
            std::move(renderer)
        };
        if (!engine_loop.Initialize())
        {
            return 1;
        }

        const GE::Launch::EngineRunSettings run_settings{
            // 三帧会覆盖两个帧槽，并验证第一个帧槽可被安全复用。
            .max_frames = IsSmokeRun(argument_count, arguments)
                ? std::optional<std::uint64_t>{3}
                : std::nullopt
        };

        const int exit_code = engine_loop.Run(
            std::move(editor_engine),
            run_settings
        );
        engine_loop.Shutdown();
        return exit_code;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[MEngineEditor] Unhandled exception: "
                  << exception.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "[MEngineEditor] Unknown exception\n";
        return 1;
    }
}
