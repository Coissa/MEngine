#include <Editor/SceneResources.h>
#include <Renderer/Renderer.h>
#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace GE;
namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void Save(const RHI::Image& image, const char* path)
    {
        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << image.width << ' ' << image.height << "\n255\n";
        for (size_t i = 0; i < image.rgba.size(); i += 4)
            file.write(reinterpret_cast<const char*>(image.rgba.data() + i), 3);
    }
    std::array<double, 3> CenterAverage(const RHI::Image& image)
    {
        std::array<double, 3> result{};
        for (uint32_t y = image.height / 2 - 16; y < image.height / 2 + 16; ++y)
            for (uint32_t x = image.width / 2 - 16; x < image.width / 2 + 16; ++x)
                for (uint32_t c = 0; c < 3; ++c)
                    result[c] += image.rgba[(y * image.width + x) * 4 + c] / 1024.0;
        return result;
    }
    Render::RHIRenderScene MakeScene()
    {
        Render::RHIRenderScene scene;
        std::string error;
        Require(Editor::BuildEditorScene(std::getenv("MENGINE_SHADER_DIR"), scene, error), error.c_str());
        scene.ui_composition.reset();
        scene.gizmo_composition.reset();
        scene.gpu_driven.reset();
        scene.skybox.reset();
        Render::RHIMeshDescription receiver;
        receiver.handle = {1};
        receiver.vertices = {{{-3, -3, 0}, {0, 0, -1}, {1, 1, 1}, {0, 0}},
                             {{3, -3, 0}, {0, 0, -1}, {1, 1, 1}, {1, 0}},
                             {{3, 3, 0}, {0, 0, -1}, {1, 1, 1}, {1, 1}},
                             {{-3, 3, 0}, {0, 0, -1}, {1, 1, 1}, {0, 1}}};
        receiver.indices = {0, 1, 2, 0, 2, 3};
        Render::RHIMeshDescription emitter;
        emitter.handle = {2};
        emitter.vertices = {{{-1.5f, -3, -2}, {1, 0, 0}, {1, 1, 1}, {0, 0}},
                            {{-1.5f, -3, 0}, {1, 0, 0}, {1, 1, 1}, {1, 0}},
                            {{-1.5f, 3, 0}, {1, 0, 0}, {1, 1, 1}, {1, 1}},
                            {{-1.5f, 3, -2}, {1, 0, 0}, {1, 1, 1}, {0, 1}}};
        emitter.indices = {0, 1, 2, 0, 2, 3};
        scene.meshes = {receiver, emitter};
        Render::RHIMaterialDescription white, red;
        white.handle = {1};
        white.base_color = {0.8f, 0.8f, 0.8f, 1};
        red.handle = {2};
        red.emissive = {4, 0.04f, 0.02f};
        scene.materials = {white, red};
        return scene;
    }
} // namespace
int main()
{
    try
    {
        auto device = D3D12::CreateDevice({.enableDebug = true});
        Require(device->GetCapabilities().inlineRayTracing, "GI hardware test requires DXR 1.1");
        auto& context = device->GetCommandContext();
        Render::Renderer renderer(*device, MakeScene());
        constexpr uint32_t width = 192, height = 144;
        renderer.Resize(width, height);
        auto output = device->CreateTexture({width, height, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto target = device->CreateView(*output, RHI::ViewKind::RenderTarget);
        RenderCore::RenderFrame frame{.output_extent = {width, height}, .scene_extent = {width, height}};
        frame.enable_taa = false;
        frame.main_view.view = Math::TranslationMatrix({0, 0, 6});
        frame.main_view.projection =
            Math::PerspectiveFovLH_ZO(Math::Radians{Math::Pi / 3}, float(width) / height, 0.1f, 100);
        frame.main_view.camera_world_position = {0, 0, -6, 1};
        frame.rendering.light_intensity = frame.rendering.ambient = frame.rendering.environment_intensity = 0;
        frame.rendering.exposure = 1;
        frame.rendering.normal_debug = false;
        frame.render_items = {{.mesh = {1}, .material = {1}, .object_id = 1},
                              {.mesh = {2}, .material = {2}, .object_id = 2}};
        auto draw = [&]
        {
            auto recording = context.BeginFrame();
            renderer.Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            auto submitted = context.Submit(std::move(recording));
            renderer.CommitFrame();
            context.Wait(submitted);
            return device->ReadbackTexture(*output);
        };
        auto disabled = draw();
        Save(disabled, "gi-disabled.ppm");
        Require(CenterAverage(disabled)[0] < 1, "Receiver is lit with GI and all direct sources disabled");
        frame.enable_gi = true;
        RHI::Image enabled;
        for (int i = 0; i < 32; ++i)
            enabled = draw();
        Save(enabled, "gi-enabled.ppm");
        const auto average = CenterAverage(enabled);
        Require(average[0] > 20 && average[0] > average[1] * 3,
                "Emissive secondary surface did not produce red indirect light");
        // A discarded recording must not advance the GI/TAA history.
        {
            auto cancelled = context.BeginFrame();
            renderer.Render(cancelled.Cmd(), cancelled.SlotIndex(), frame, {*target, RHI::State::CopySource});
        }
        Require(CenterAverage(draw())[0] > 20, "GI was lost after cancelled recording");
        renderer.ResetHistory();
        Require(CenterAverage(draw())[0] > 10, "GI did not regenerate after history reset");
        context.WaitIdle();
        renderer.ReleaseSizeResources();
        renderer.Resize(width, height);
        Require(CenterAverage(draw())[0] > 10, "GI did not regenerate after resize");
        frame.enable_gi = false;
        Require(CenterAverage(draw())[0] < 1, "GI toggle left stale lighting in the frame");
        context.WaitIdle();
        device->CheckDebugErrors();
        std::cout << "GI indirect-light, toggle, resize and cancelled-recording checks passed; RGB = " << average[0]
                  << ", " << average[1] << ", " << average[2] << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
