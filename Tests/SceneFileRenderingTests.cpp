#include "RenderSceneFixture.h"
#include <Editor/D3D12EditorRenderResources.h>
#include <Editor/EditorSession.h>
#include <Editor/EditorUserInterface.h>
#include <Renderer/Renderer.h>
#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <Runtime/GameObject.h>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace GE;

namespace
{
    void Require(bool value, const std::string& message)
    {
        if (!value)
            throw std::runtime_error(message);
    }

    double Difference(const RHI::Image& a, const RHI::Image& b)
    {
        double value = 0;
        for (std::size_t i = 0; i < a.rgba.size(); ++i)
            value += std::abs(int(a.rgba[i]) - int(b.rgba[i]));
        return value / a.rgba.size();
    }

    void TestSkullHighlights(RHI::Device& device, Render::Renderer& renderer, const Runtime::World& world)
    {
        constexpr uint32_t width = 640, height = 360;
        renderer.Resize(width, height);
        auto output = device.CreateTexture({width, height, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto target = device.CreateView(*output, RHI::ViewKind::RenderTarget);
        RenderCore::RenderFrame frame{.output_extent = {width, height}, .scene_extent = {width, height}};
        Require(Runtime::ExtractWorldRenderFrame(world, frame), "Could not extract skull regression view");
        const auto projection = frame.main_view.view * frame.main_view.projection;
        // Teeth/jaw bounds in the default scene; exclude the broad, smooth forehead.
        const auto a = Math::Transform({-1.4f, 2.4f, -2, 1}, projection);
        const auto b = Math::Transform({1.4f, 3.5f, -2, 1}, projection);
        const int x0 = int((a[0] / a[3] * 0.5f + 0.5f) * width);
        const int x1 = int((b[0] / b[3] * 0.5f + 0.5f) * width);
        const int y0 = int((0.5f - b[1] / b[3] * 0.5f) * height);
        const int y1 = int((0.5f - a[1] / a[3] * 0.5f) * height);
        auto& context = device.GetCommandContext();
        RHI::Image previous;
        double difference = 0;
        size_t brightJumps = 0;
        for (uint32_t i = 0; i < 48; ++i)
        {
            auto recording = context.BeginFrame();
            renderer.Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            context.Wait(context.Submit(std::move(recording)));
            renderer.CommitFrame();
            auto image = device.ReadbackTexture(*output);
            if (i >= 32)
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x)
                        for (int c = 0; c < 3; ++c)
                        {
                            const auto index = (size_t(y) * width + x) * 4 + c;
                            const auto delta = std::abs(int(previous.rgba[index]) - int(image.rgba[index]));
                            difference += delta;
                            brightJumps += delta > 32;
                        }
            previous = std::move(image);
        }
        const double samples = double(16 * (x1 - x0) * (y1 - y0) * 3);
        std::cout << "Skull teeth: temporal difference=" << difference / samples
                  << ", bright-jump ratio=" << brightJumps / samples << '\n';
        Require(difference / samples < 2.0 && brightJumps / samples < 0.005,
                "Subpixel metallic highlights flicker around the skull teeth");
        renderer.Resize(128, 128);
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const std::filesystem::path assets = MENGINE_TEST_ASSET_ROOT;
        const std::filesystem::path shaders = std::getenv("MENGINE_SHADER_DIR");
        Tests::RuntimeRegistries registries;
        auto project = Asset::ProjectDescriptorSerializer::LoadFromFile(assets / "MEngine.project");
        Require(bool(project), "Project could not be loaded");
        Require(project.Value().startup_scene == "Scenes/RendererLab.mscene", "Startup scene was lost");
        auto registry = Asset::AssetRegistrySerializer::LoadFromFile(assets / "AssetRegistry.mregistry",
                                                                     project.Value().namespace_id);
        Require(bool(registry), "Asset registry could not be loaded");
        const bool hardware = argc > 1 && std::string_view(argv[1]) == "--hardware";
        auto device = D3D12::CreateDevice({.useWarp = !hardware, .enableDebug = true});
        auto& context = device->GetCommandContext();
        std::unique_ptr<Render::Renderer> renderer;
        Editor::EditorSession editor{assets / "Scenes/RendererArchitectureDemo.mscene"};
        std::size_t loadedSkullVertices = 0;
        bool hasAlphaCutout = false, hasTiledTexture = false;
        editor.SetSceneResourceLoader(
            [&](Runtime::World& world, std::string& error)
            {
                Render::RHIRenderScene scene;
                if (!Editor::BuildEditorScene(shaders, scene, error) ||
                    !Editor::LoadSceneAssets(world, registry.Value(), assets, scene, error))
                    return false;
                loadedSkullVertices = 0;
                if (const auto* skull = world.FindGameObjectById({3}); skull && skull->GetMeshRenderer())
                    for (const auto& mesh : scene.meshes)
                        if (mesh.handle == skull->GetMeshRenderer()->mesh)
                            loadedSkullVertices = mesh.vertices.size();
                hasAlphaCutout = hasTiledTexture = false;
                for (const auto& material : scene.materials)
                {
                    hasAlphaCutout |= material.alpha_cutoff > 0;
                    hasTiledTexture |= material.uv_scale[0] > 1;
                }
                // The debug pass is supplied by the editor and remains independent of scene files.
                Editor::EditorFontAtlas font{1, 1, {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}};
                if (!Editor::AddD3D12EditorRenderResources(font, shaders, scene, error))
                    return false;
                scene.ui_composition.reset();
                context.WaitIdle();
                auto replacement = std::make_unique<Render::Renderer>(*device, std::move(scene));
                replacement->Resize(128, 128);
                renderer = std::move(replacement);
                return true;
            });
        const Runtime::EngineServices services{registries.reflection, registries.serialization, registries.components,
                                               registry.Value()};
        Require(editor.Initialize(services), "File-driven editor initialization failed");
        auto output = device->CreateTexture({128, 128, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto target = device->CreateView(*output, RHI::ViewKind::RenderTarget);
        auto draw = [&](bool gizmo = false)
        {
            RenderCore::RenderFrame frame{.output_extent = {128, 128}, .scene_extent = {128, 128}};
            Require(Runtime::ExtractWorldRenderFrame(*editor.GetEditorWorldContext()->GetWorld(), frame),
                    "Scene camera extraction failed");
            frame.enable_taa = false;
            if (gizmo)
            {
                frame.gizmo.vertices = {{{-0.3f, -0.3f, 0.0f, 1}, 0xff0000ff},
                                        {{0.3f, -0.3f, 0.0f, 1}, 0xff0000ff},
                                        {{0, 0.3f, 0.0f, 1}, 0xff0000ff}};
                frame.gizmo.indices = {0, 1, 2};
            }
            auto recording = context.BeginFrame();
            renderer->Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            auto submitted = context.Submit(std::move(recording));
            renderer->CommitFrame();
            context.Wait(submitted);
            return device->ReadbackTexture(*output);
        };

        const auto first = draw();
        const auto debug = draw(true);
        const std::size_t center = (64 * 128 + 64) * 4;
        Require(debug.rgba[center] == 255 && debug.rgba[center + 1] == 0,
                "Editor debug geometry was changed by tone mapping");
        auto* world = editor.GetEditorWorldContext()->GetWorld();
        world->GetSceneConfiguration().rendering.exposure = 0.05f;
        const auto dark = draw();
        Require(Difference(first, dark) > 5, "Scene exposure did not affect the lit mesh");
        Require(first.rgba[0] > dark.rgba[0] || first.rgba[1] > dark.rgba[1] || first.rgba[2] > dark.rgba[2],
                "Scene exposure did not affect the HDR sky");

        auto* cube = world->FindGameObjectById({3});
        const auto originalRotation = cube->GetTransform().rotation_degrees;
        Require(editor.StartPlaySession(), "File-based Rotator could not start");
        editor.Tick({0.1, 0.1});
        Require(editor.GetPIEWorldContext()->GetWorld()->FindGameObjectById({3})->GetTransform().rotation_degrees !=
                    originalRotation,
                "Serialized Rotator did not update the PIE object");
        editor.StopPlaySession();
        Require(cube->GetTransform().rotation_degrees == originalRotation, "PIE changed the editor scene");

        std::string error;
        const auto savedPath = std::filesystem::current_path() / "SceneFileRenderingTests.mscene";
        Require(editor.SaveScene(savedPath, error), error);
        Require(editor.LoadScene(assets / "Scenes/TwoCubes.mscene", error), error);
        Require(Difference(first, draw()) > 5, "Selecting another scene did not change the rendered content");
        Require(editor.LoadScene(savedPath, error), error);
        Require(Difference(dark, draw()) < 0.1, "Save/reload lost scene assets or render settings");

        // A resource failure must leave the displayed scene and renderer usable.
        auto invalid = Runtime::SceneSerializer::LoadFromFile(savedPath, registries.components,
                                                              registries.GetSerializationContext(), error);
        invalid->GetSceneConfiguration().meshes[0].asset = registries.CreateNamespace();
        const auto invalidPath = std::filesystem::current_path() / "SceneFileRenderingTests-invalid.mscene";
        Require(
            Runtime::SceneSerializer::SaveToFile(*invalid, invalidPath, registries.GetSerializationContext(), error),
            error);
        auto* retainedWorld = editor.GetEditorWorldContext()->GetWorld();
        Require(!editor.LoadScene(invalidPath, error) && editor.GetEditorWorldContext()->GetWorld() == retainedWorld,
                "Failed asset loading replaced the active world");
        Require(Difference(dark, draw()) < 0.1, "Failed asset loading damaged the active renderer");
        Require(editor.LoadScene(assets / "Scenes/PBRMaterialLab.mscene", error), error);
        Require(editor.GetEditorWorldContext()->GetWorld()->GetGameObjectCount() == 8, "PBR material lab did not load");
        const auto materialLab = draw();
        Require(Difference(first, materialLab) > 5, "PBR material lab did not render");
        Require(editor.LoadScene(assets / project.Value().startup_scene, error), error);
        const auto* richWorld = editor.GetEditorWorldContext()->GetWorld();
        Require(richWorld->GetGameObjectCount() == 50 && richWorld->GetSceneConfiguration().meshes.size() == 20 &&
                    loadedSkullVertices > 30000 && hasAlphaCutout && hasTiledTexture,
                "Rich default scene lost its skull, layout, cutout foliage, or tiled ground");
        const auto richImage = draw();
        Require(Difference(first, richImage) > 5, "Rich scene did not change the rendered content");
        TestSkullHighlights(*device, *renderer, *richWorld);
        Require(editor.SaveScene(savedPath, error) && editor.LoadScene(savedPath, error), error);
        Require(editor.GetEditorWorldContext()->GetWorld()->GetGameObjectCount() == 50 &&
                    Difference(richImage, draw()) < 0.1,
                "Rich scene did not survive save/reload");
        Require(editor.NewScene(error) && editor.GetEditorWorldContext()->GetWorld()->GetGameObjectCount() == 0,
                "New scene still contains fixed demo objects");
        std::filesystem::remove(savedPath);
        std::filesystem::remove(invalidPath);
        context.WaitIdle();
        renderer.reset();
        device->CheckDebugErrors();
        std::cout << "[SceneFileRenderingTests] Passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
