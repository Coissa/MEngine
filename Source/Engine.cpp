#include "Engine.h"
#include <Assets/Formats/GltfImporter.h>
#include <Assets/Registry/AssetRegistry.h>
#include <Assets/Registry/AssetRegistrySerializer.h>
#include <Assets/Registry/AssetSerialization.h>
#include <Assets/Registry/ProjectDescriptor.h>
#include <Core/FileSystem/FileMount.h>
#include <Core/Reflection/BuiltinTypeRegistration.h>
#include <Core/Reflection/TypeRegistry.h>
#include <Core/Serialization/BuiltinSerialization.h>
#include <Core/Serialization/SerializationRegistry.h>
#include <Core/Time/FrameClock.h>
#include <Editor/ClusterDebugColorizer.h>
#include <Editor/D3D12EditorRenderResources.h>
#include <Editor/EditorSession.h>
#include <Editor/EditorUserInterface.h>
#include <Editor/SceneResources.h>
#include <Platform/Windows/Win32Application.h>
#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <Renderer/Renderer.h>
#include <Runtime/ComponentRegistry.h>
#include <Runtime/World.h>
#include <Runtime/SceneSerializer.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
    bool HasOption(int count, char** arguments, std::string_view option)
    {
        for (int i = 1; i < count; ++i)
            if (arguments[i] == option)
                return true;
        return false;
    }
    std::optional<std::filesystem::path> PathOption(int count, char** arguments, std::string_view option)
    {
        for (int i = 1; i + 1 < count; ++i)
            if (arguments[i] == option)
                return std::filesystem::absolute(arguments[i + 1]);
        return std::nullopt;
    }
    std::filesystem::path ExecutableDirectory(char** arguments)
    {
        return std::filesystem::absolute(arguments[0]).parent_path();
    }
    void SaveSmokeImage(const GE::RHI::Image& image, const std::filesystem::path& path)
    {
        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << image.width << ' ' << image.height << "\n255\n";
        for (size_t i = 0; i < image.rgba.size(); i += 4)
            file.write(reinterpret_cast<const char*>(image.rgba.data() + i), 3);
    }

    [[nodiscard]] bool BuildMeshBounds(const GE::Render::RHIMeshDescription& mesh,
                                       GE::Editor::EditorMeshBounds& out_bounds) noexcept
    {
        if (!mesh.handle.IsValid() || mesh.vertices.empty())
        {
            return false;
        }
        const std::array<float, 3>& first = mesh.vertices.front().position;
        if (!std::isfinite(first[0]) || !std::isfinite(first[1]) || !std::isfinite(first[2]))
        {
            return false;
        }
        out_bounds.minimum = {first[0], first[1], first[2]};
        out_bounds.maximum = out_bounds.minimum;
        for (const GE::Render::PositionNormalColorTextureVertex& vertex : mesh.vertices)
        {
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                const float value = vertex.position[axis];
                if (!std::isfinite(value))
                {
                    return false;
                }
                out_bounds.minimum[axis] = (std::min)(out_bounds.minimum[axis], value);
                out_bounds.maximum[axis] = (std::max)(out_bounds.maximum[axis], value);
            }
        }
        return out_bounds.IsValid();
    }
} // namespace

namespace GE
{
    struct Engine::State
    {
        std::filesystem::path startupScene;
        std::unique_ptr<Reflection::TypeRegistry> types;
        std::unique_ptr<Serialization::SerializationRegistry> serialization;
        std::unique_ptr<Runtime::ComponentRegistry> components;
        std::unique_ptr<Asset::AssetRegistry> assets;
        std::unique_ptr<Platform::Windows::Win32Application> application;
        std::unique_ptr<RHI::Device> device;
        std::unique_ptr<RHI::Swapchain> swapchain;
        std::unique_ptr<Render::Renderer> renderer;
        std::unique_ptr<GE::Editor::EditorSession> editor;
        void InitializeServices(const std::filesystem::path& root)
        {
            auto project = Asset::ProjectDescriptorSerializer::LoadFromFile(root / "MEngine.project");
            if (!project)
                throw std::runtime_error(project.Error().message);
            if (!project.Value().startup_scene.empty())
                startupScene = root / project.Value().startup_scene;
            auto registry = Asset::AssetRegistrySerializer::LoadFromFile(root / "AssetRegistry.mregistry",
                                                                         project.Value().namespace_id);
            if (!registry)
                throw std::runtime_error(registry.Error().message);
            assets = std::make_unique<Asset::AssetRegistry>(std::move(registry).Value());
            types = std::make_unique<Reflection::TypeRegistry>();
            serialization = std::make_unique<Serialization::SerializationRegistry>();
            components = std::make_unique<Runtime::ComponentRegistry>();
            auto builtin = Reflection::RegisterBuiltinTypes(*types);
            if (!builtin)
                throw std::runtime_error(builtin.Error().message);
            auto ser = Serialization::RegisterBuiltinSerialization(*types, *serialization);
            if (!ser)
                throw std::runtime_error(ser.Error().message);
            auto asset = Asset::RegisterAssetSerialization(*types, *serialization);
            if (!asset)
                throw std::runtime_error(asset.Error().message);
            auto runtime = Runtime::RegisterBuiltinComponentReflection(*types);
            if (!runtime)
                throw std::runtime_error(runtime.Error().message);
            std::string error;
            if (!Runtime::RegisterBuiltinComponents(*types, *components, error))
                throw std::runtime_error(error);
            if (components->Freeze() != Runtime::ComponentRegistry::FreezeResult::Success)
                throw std::runtime_error("Component registration failed");
            auto freeze = types->Freeze();
            if (!freeze)
                throw std::runtime_error(freeze.Error().message);
            auto sf = serialization->Freeze(*types);
            if (!sf)
                throw std::runtime_error(sf.Error().message);
        }
    };
    Engine& Engine::Get()
    {
        static Engine instance;
        return instance;
    }
    Engine::~Engine()
    {
        Shutdown();
    }
    Editor::EditorSession& Engine::Editor()
    {
        return *state->editor;
    }
    Asset::AssetRegistry& Engine::Assets()
    {
        return *state->assets;
    }
    Reflection::TypeRegistry& Engine::Reflection()
    {
        return *state->types;
    }
    int Engine::Run(int argument_count, char** arguments)
    {
        state = std::make_unique<State>();
        state->InitializeServices((ExecutableDirectory(arguments) / "Assets"));
        constexpr GE::Platform::WindowSize InitialSize{.width = 1280, .height = 720};
        auto& application = state->application;
        application =
            GE::Platform::Windows::Win32Application::Create({.title = "MEngine Editor", .client_size = InitialSize});
        if (application == nullptr)
        {
            std::cerr << "[MEngineEditor] Window creation failed\n";
            return 1;
        }

        const auto native_window = application->GetNativeWindowHandle();
        const std::filesystem::path scene_path =
            PathOption(argument_count, arguments, "--scene").value_or(state->startupScene);
        auto& editor_engine = state->editor;
        editor_engine = !scene_path.empty() ? std::make_unique<GE::Editor::EditorSession>(scene_path)
                                            : std::make_unique<GE::Editor::EditorSession>();
        GE::Editor::EditorFontAtlas font_atlas;
        std::string ui_error;
        if (!editor_engine->InitializeUserInterface(reinterpret_cast<std::uintptr_t>(native_window.window), font_atlas,
                                                    ui_error))
        {
            std::cerr << "[MEngineEditor] UI initialization failed: " << ui_error << '\n';
            return 1;
        }
        application->SetMessageHandler(&GE::Editor::EditorSession::HandlePlatformMessage, editor_engine.get());

        const std::filesystem::path shader_directory =
            PathOption(argument_count, arguments, "--shader-dir").value_or(ExecutableDirectory(arguments) / "Shaders");
        const auto asset_root = shader_directory.parent_path() / "Assets";
        const bool smoke = HasOption(argument_count, arguments, "--smoke");
        const bool warp = HasOption(argument_count, arguments, "--warp");
        state->device = D3D12::CreateDevice({.useWarp = warp,
                                             .enableDebug =
#ifndef NDEBUG
                                                 true
#else
                                                 smoke
#endif
        });
        state->swapchain = D3D12::CreateSwapchain(*state->device, reinterpret_cast<void*>(native_window.window),
                                                  {InitialSize.width, InitialSize.height, 2});
        uint32_t renderWidth = 1, renderHeight = 1;
        editor_engine->SetSceneResourceLoader(
            [&](Runtime::World& world, std::string& error)
            {
                try
                {
                    // Old files used the original numeric cube/material IDs without an asset table.
                    if (world.GetSceneConfiguration().legacy_resource_ids)
                    {
                        auto legacy = Runtime::SceneSerializer::LoadFromFile(
                            asset_root / "Scenes/Default.mscene", *state->components,
                            Serialization::Context{*state->types, *state->serialization}, error);
                        if (!legacy)
                            return false;
                        world.GetSceneConfiguration().meshes = legacy->GetSceneConfiguration().meshes;
                        world.GetSceneConfiguration().materials = legacy->GetSceneConfiguration().materials;
                        world.GetSceneConfiguration().legacy_resource_ids = false;
                    }
                    Render::RHIRenderScene scene;
                    if (!Editor::BuildEditorScene(shader_directory, scene, error) ||
                        !Editor::LoadSceneAssets(world, *state->assets, asset_root, scene, error) ||
                        !Editor::AddD3D12EditorRenderResources(font_atlas, shader_directory, scene, error))
                        return false;
                    Editor::ClusterDebugGeometry debug_geometry;
                    if (const auto mesh_path = PathOption(argument_count, arguments, "--cluster-mesh");
                        mesh_path && !scene.materials.empty())
                    {
                        auto mount = GE::FileSystem::LooseFileMount::Open(mesh_path->parent_path());
                        auto logical_path =
                            GE::FileSystem::NormalizeLogicalPath(mesh_path->filename().generic_string());
                        if (!mount || !logical_path)
                        {
                            error = "Could not open cluster mesh";
                            return false;
                        }
                        auto imported = GE::AssetFormats::ImportGltf(mount.Value(), logical_path.Value());
                        if (!imported)
                        {
                            error = imported.Error().message;
                            return false;
                        }
                        const auto dags = GE::Render::Nanite::ReduceMesh(imported.Value().mesh);
                        std::uint32_t firstClusterHandle = 1;
                        for (const auto& mesh : scene.meshes)
                            firstClusterHandle = (std::max)(firstClusterHandle, mesh.handle.value + 1);
                        debug_geometry = GE::Editor::BuildClusterDebugGeometry(
                            imported.Value().mesh, GE::Render::Nanite::PackClusterDAG(dags), firstClusterHandle);
                        for (auto& item : debug_geometry.render_items)
                            item.material = scene.materials.front().handle;
                        std::cout << "[MEngineEditor] Nanite mesh: " << debug_geometry.cluster_count
                                  << " leaf clusters, " << debug_geometry.mesh.data.clusters.size()
                                  << " total clusters, " << debug_geometry.mesh.data.groups.size() << " groups, "
                                  << debug_geometry.render_items.size() << " nodes\n";
                        scene.nanite_meshes.push_back(std::move(debug_geometry.mesh));
                    }

                    Editor::EditorScenePicker picker;
                    for (const auto& mesh : scene.meshes)
                    {
                        Editor::EditorMeshBounds bounds;
                        if (!BuildMeshBounds(mesh, bounds) || !picker.RegisterMeshBounds(mesh.handle, bounds))
                            throw std::runtime_error("Invalid scene mesh bounds");
                    }
                    state->device->GetCommandContext().WaitIdle();
                    auto renderer = std::make_unique<Render::Renderer>(*state->device, std::move(scene));
                    renderer->Resize(renderWidth, renderHeight);
                    state->renderer = std::move(renderer);
                    editor_engine->SetScenePicker(std::move(picker));
                    editor_engine->SetClusterDebugGeometry(std::move(debug_geometry.render_items),
                                                           debug_geometry.bounds_min, debug_geometry.bounds_max,
                                                           debug_geometry.cluster_count);
                    return true;
                }
                catch (const std::exception& exception)
                {
                    error = exception.what();
                    return false;
                }
            });
        const Runtime::EngineServices services{*state->types, *state->serialization, *state->components,
                                               *state->assets};
        if (!editor_engine->Initialize(services))
            throw std::runtime_error("Editor initialization failed");
        application->Show();
        auto& context = state->device->GetCommandContext();
        Core::FrameClock clock;
        double elapsed = 0;
        uint64_t frames = 0;
        uint32_t windowWidth = InitialSize.width, windowHeight = InitialSize.height, sceneWidth = 0, sceneHeight = 0;
        while (application->PumpMessages())
        {
            const float delta = clock.Sample();
            elapsed += delta;
            editor_engine->Tick({delta, elapsed});
            const auto size = application->GetClientSize();
            if (size.width == 0 || size.height == 0)
            {
                state->renderer->ResetHistory();
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }
            RenderCore::RenderFrame frame{
                .frame_number = frames, .output_extent = {size.width, size.height}, .real_elapsed_seconds = elapsed};
            frame.gpu_timings = state->renderer->GetGpuTimings();
            if (!editor_engine->BuildRenderFrame(frame))
                throw std::runtime_error("Render frame extraction failed");
            const uint32_t desiredWidth = (std::max)(1u, frame.scene_extent.width),
                           desiredHeight = (std::max)(1u, frame.scene_extent.height);
            if (windowWidth != size.width || windowHeight != size.height || sceneWidth != desiredWidth ||
                sceneHeight != desiredHeight)
            {
                context.WaitIdle();
                state->renderer->ReleaseSizeResources();
                if (windowWidth != size.width || windowHeight != size.height)
                    state->swapchain->Resize(size.width, size.height);
                state->renderer->Resize(desiredWidth, desiredHeight);
                renderWidth = desiredWidth;
                renderHeight = desiredHeight;
                windowWidth = size.width;
                windowHeight = size.height;
                sceneWidth = desiredWidth;
                sceneHeight = desiredHeight;
            }
            auto recording = context.BeginFrame();
            auto target = state->swapchain->GetCurrentTarget();
            state->renderer->Render(recording.Cmd(), recording.SlotIndex(), frame, {target.color, RHI::State::Present});
            const auto submitted = context.Submit(std::move(recording));
            state->renderer->CommitFrame();
            if (smoke && frames == 11)
            {
                context.Wait(submitted);
                SaveSmokeImage(state->device->ReadbackTexture(target.color.Resource()),
                               ExecutableDirectory(arguments) / "editor-smoke.ppm");
            }
            state->swapchain->Present();
            if (++frames >= 12 && smoke)
                break;
        }
        if (smoke)
        {
            context.WaitIdle();
            state->device->CheckDebugErrors();
        }
        Shutdown();
        return 0;
    }
    void Engine::Shutdown() noexcept
    {
        if (!state)
            return;
        if (state->application)
            state->application->SetMessageHandler(nullptr, nullptr);
        try
        {
            if (state->device)
                state->device->GetCommandContext().WaitIdle();
        }
        catch (...)
        {
        }
        state.reset();
    }
} // namespace GE
