#include <Engine/Launch/EngineLoop.h>
#include <Engine/Platform/Windows/Win32Application.h>
#include <Engine/RHI/D3D12/D3D12BackendFactory.h>
#include <Engine/Renderer/Examples/D3D12RotatingCubeExample.h>
#include <Engine/Renderer/RHIRenderer.h>
#include <Engine/Runtime/GameEngine.h>

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

    [[nodiscard]] std::filesystem::path FindShaderDirectory(
        int argument_count,
        char** arguments)
    {
        const std::filesystem::path executable =
            argument_count > 0 && arguments != nullptr
                && arguments[0] != nullptr && arguments[0][0] != '\0'
            ? std::filesystem::absolute(arguments[0])
            : std::filesystem::current_path();
        const std::filesystem::path directory = executable.has_parent_path()
            ? executable.parent_path() : std::filesystem::current_path();
        return directory / "Shaders";
    }

    [[nodiscard]] std::filesystem::path FindAssetDirectory(
        int argument_count,
        char** arguments)
    {
        const std::filesystem::path executable =
            argument_count > 0 && arguments != nullptr
                && arguments[0] != nullptr && arguments[0][0] != '\0'
            ? std::filesystem::absolute(arguments[0])
            : std::filesystem::current_path();
        const std::filesystem::path directory = executable.has_parent_path()
            ? executable.parent_path() : std::filesystem::current_path();
        return directory / "Assets";
    }
}

int main(int argument_count, char** arguments)
{
    try
    {
        constexpr GE::Platform::WindowSize InitialSize{
            .width = 1280, .height = 720};
        auto application = GE::Platform::Windows::Win32Application::Create({
            .title = "MEngine - Rotating Cube",
            .client_size = InitialSize});
        if (application == nullptr)
        {
            std::cerr << "[MEngineGame] Window creation failed\n";
            return 1;
        }

        const auto native_window = application->GetNativeWindowHandle();
        GE::Render::RHIRenderScene scene;
        std::string scene_error;
        if (!GE::Render::Examples::BuildD3D12RotatingCubeScene(
                FindShaderDirectory(argument_count, arguments),
                scene,
                scene_error))
        {
            std::cerr << "[MEngineGame] Cube scene creation failed: "
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
        // 示例函数只生成值快照；EngineLoop 和 Renderer 都不拥有它引用的场景对象。
        GE::Launch::EngineLoop engine_loop{
            FindAssetDirectory(argument_count, arguments),
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

        const std::optional<std::filesystem::path> scene_path =
            FindScenePath(argument_count, arguments);
        std::unique_ptr<GE::Runtime::Engine> game_engine = scene_path
            ? std::make_unique<GE::Runtime::GameEngine>(*scene_path)
            : std::make_unique<GE::Runtime::GameEngine>();
        const int exit_code = engine_loop.Run(
            std::move(game_engine),
            run_settings
        );
        engine_loop.Shutdown();
        return exit_code;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[MEngineGame] Unhandled exception: "
                  << exception.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "[MEngineGame] Unknown exception\n";
        return 1;
    }
}
