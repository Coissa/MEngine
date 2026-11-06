#include <Editor/SceneResources.h>
#include <Renderer/Renderer.h>
#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

using namespace GE;
namespace
{
    void Require(bool value, const char* message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    int Display(float linear)
    {
        const float mapped = 1 - std::exp(-linear);
        return int(
            std::round(255 * (mapped <= 0.0031308f ? mapped * 12.92f : 1.055f * std::pow(mapped, 1 / 2.4f) - 0.055f)));
    }
    std::array<int, 3> Center(const RHI::Image& image)
    {
        const size_t offset = (image.height / 2 * image.width + image.width / 2) * 4;
        return {image.rgba[offset], image.rgba[offset + 1], image.rgba[offset + 2]};
    }
    Render::RHIRenderScene Scene(bool constantEnvironment)
    {
        Render::RHIRenderScene scene;
        std::string error;
        Require(Editor::BuildEditorScene(std::getenv("MENGINE_SHADER_DIR"), scene, error), error.c_str());
        scene.gpu_driven.reset();
        scene.ui_composition.reset();
        scene.gizmo_composition.reset();
        auto& sky = scene.textures.front();
        sky.width = sky.height = 1;
        const float colors[6][4] = {{1, 0.02f, 0.02f, 1},     {0.02f, 1, 0.02f, 1}, {1, 1, 1, 1},
                                    {0.02f, 0.02f, 0.02f, 1}, {0.02f, 1, 0.02f, 1}, {0.02f, 0.02f, 1, 1}};
        const float constant[4] = {1, 0.5f, 0.25f, 1};
        sky.pixels.resize(sizeof(colors));
        for (size_t face = 0; face < 6; ++face)
            std::memcpy(sky.pixels.data() + face * sizeof(constant), constantEnvironment ? constant : colors[face],
                        sizeof(constant));
        Render::RHIMeshDescription quad;
        quad.handle = {1};
        quad.vertices = {{{-3, -3, 0}, {0, 0, -1}, {1, 1, 1}, {0, 1}},
                         {{3, -3, 0}, {0, 0, -1}, {1, 1, 1}, {1, 1}},
                         {{3, 3, 0}, {0, 0, -1}, {1, 1, 1}, {1, 0}},
                         {{-3, 3, 0}, {0, 0, -1}, {1, 1, 1}, {0, 0}}};
        quad.indices = {0, 1, 2, 0, 2, 3};
        scene.meshes = {quad};
        Render::RHITextureDescription transparent;
        transparent.handle = {1};
        transparent.width = transparent.height = 1;
        transparent.format = RHI::RHIFormat::R8G8B8A8_UNorm;
        transparent.pixels = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{0}};
        scene.textures.push_back(std::move(transparent));
        Render::RHIMaterialDescription material;
        material.handle = {1};
        material.roughness = 0.6f;
        scene.materials = {material};
        material.handle = {2};
        material.metallic = 1;
        material.roughness = 0.1f;
        scene.materials.push_back(material);
        material.handle = {3};
        material.metallic = 0;
        material.albedo_texture = {1};
        material.alpha_cutoff = 0.5f;
        scene.materials.push_back(material);
        material.handle = {4};
        material.albedo_texture = {};
        material.alpha_cutoff = 0;
        material.metallic = 1;
        material.roughness = 1;
        scene.materials.push_back(material);
        return scene;
    }
} // namespace
int main(int argc, char** argv)
{
    try
    {
        const bool hardware = argc > 1 && std::string_view(argv[1]) == "--hardware";
        auto device = D3D12::CreateDevice({.useWarp = !hardware, .enableDebug = true});
        auto& context = device->GetCommandContext();
        Render::Renderer renderer(*device, Scene(true));
        renderer.Resize(128, 128);
        auto output = device->CreateTexture({128, 128, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto target = device->CreateView(*output, RHI::ViewKind::RenderTarget);
        RenderCore::RenderFrame frame{.output_extent = {128, 128}, .scene_extent = {128, 128}};
        frame.enable_taa = false;
        frame.main_view.view = Math::TranslationMatrix({0, 0, 6});
        frame.main_view.projection = Math::PerspectiveFovLH_ZO(Math::Radians{Math::Pi / 3}, 1, 0.1f, 100);
        frame.main_view.camera_world_position = {0, 0, -6, 1};
        frame.rendering = {.light_direction = {-0.4f, 0.8f, -0.7f},
                           .light_intensity = 0,
                           .ambient = 0,
                           .exposure = 1,
                           .normal_debug = false};
        frame.render_items = {{.mesh = {1}, .material = {2}, .object_id = 1}};
        auto draw = [&](Render::Renderer& current)
        {
            auto recording = context.BeginFrame();
            current.Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            context.Wait(context.Submit(std::move(recording)));
            current.CommitFrame();
            return device->ReadbackTexture(*output);
        };
        Require(Center(draw(renderer))[0] == 0, "Metal receives light with every source disabled");
        frame.rendering.environment_intensity = 1;
        auto env = Center(draw(renderer));
        Require(std::abs(env[0] - Display(1)) < 5 && std::abs(env[1] - Display(0.5f)) < 5 &&
                    std::abs(env[2] - Display(0.25f)) < 5,
                "Constant HDR environment was not conserved by split-sum IBL");
        frame.rendering.environment_intensity = 0.5f;
        Require(std::abs(Center(draw(renderer))[0] - Display(0.5f)) < 5, "Environment intensity is not linear");
        frame.rendering.environment_intensity = 0;
        frame.render_items[0].material = {1};
        frame.rendering.lights = {{.position = {0, 0, -3}, .range = 10, .intensity = 20, .color = {1, 0, 0}}};
        auto point = Center(draw(renderer));
        Require(point[0] > 100 && point[1] == 0, "Point light color or inverse-square attenuation is incorrect");
        frame.rendering.lights[0].range = 1;
        Require(Center(draw(renderer))[0] == 0, "Point light leaked outside its range");
        frame.rendering.lights[0].range = 10;
        frame.rendering.lights[0].spot = true;
        frame.rendering.lights[0].direction = {0, 0, 1};
        Require(Center(draw(renderer))[0] > 100, "Spot light did not illuminate its cone");
        frame.rendering.lights[0].direction = {1, 0, 0};
        Require(Center(draw(renderer))[0] == 0, "Spot light leaked outside its cone");
        frame.rendering.lights = {{.position = {0, 0, -3}, .range = 10, .intensity = 10, .color = {1, 0, 0}},
                                  {.position = {0, 0, -3}, .range = 10, .intensity = 10, .color = {0, 0, 1}}};
        auto multiple = Center(draw(renderer));
        Require(multiple[0] > 70 && std::abs(multiple[0] - multiple[2]) < 2 && multiple[1] == 0,
                "Local lights did not accumulate independently");

        frame.rendering.lights.clear();
        frame.rendering.light_intensity = 3;
        frame.rendering.shadow_distance = 40;
        frame.render_items.push_back(
            {.mesh = {1},
             .material = {1},
             .object_id = 2,
             .local_to_world = Math::ScaleMatrix({0.18f, 0.18f, 1}) * Math::TranslationMatrix({-0.5f, 0.7f, -1})});
        auto unshadowed = draw(renderer);
        frame.rendering.shadows = true;
        auto shadowed = draw(renderer);
        frame.render_items[1].material = {3};
        auto cutout = draw(renderer);
        int shadowPixels = 0, restoredPixels = 0;
        for (size_t y = 65; y < 100; ++y)
            for (size_t x = 60; x < 95; ++x)
            {
                const size_t index = (y * 128 + x) * 4;
                if (int(unshadowed.rgba[index]) - int(shadowed.rgba[index]) > 30)
                    ++shadowPixels;
                if (std::abs(int(unshadowed.rgba[index]) - int(cutout.rgba[index])) < 3)
                    ++restoredPixels;
            }
        Require(shadowPixels > 60, "Cascaded directional shadow did not occlude the receiver");
        Require(restoredPixels > 1100, "Alpha-clipped caster still wrote opaque shadow depth");
        Require(renderer.GetGpuTimings().valid, "Completed frame timestamps were not exposed");
        double total = 0;
        for (double ms : renderer.GetGpuTimings().milliseconds)
        {
            Require(std::isfinite(ms) && ms >= 0, "GPU timestamp read before its frame completed");
            total += ms;
        }
        Require(total > 0, "GPU timestamp intervals are empty");

        Render::Renderer directional(*device, Scene(false));
        directional.Resize(128, 128);
        frame.render_items.resize(1);
        frame.render_items[0].material = {2};
        frame.rendering.shadows = false;
        frame.rendering.light_intensity = 0;
        frame.rendering.environment_intensity = 1;
        auto blue = Center(draw(directional));
        frame.rendering.environment_rotation = 90;
        auto red = Center(draw(directional));
        Require(blue[2] > blue[0] + 40 && red[0] > red[2] + 40, "IBL rotation or cube face orientation is incorrect");
        frame.rendering.environment_rotation = 0;
        frame.render_items[0].material = {4};
        auto rough = Center(draw(directional));
        Require(rough[0] > blue[0] + 15 && rough[2] < blue[2] - 15,
                "Roughness did not select a filtered environment mip");

        // Aborted per-subresource transitions must not change the next recording's state assumptions.
        auto array = device->CreateTexture({8, 8, RHI::RHIFormat::R32G32B32A32_Float, true, false, 2, 2, false, true});
        {
            auto canceled = context.BeginFrame();
            canceled.Cmd().Transition(*array, RHI::State::Storage, 1);
            canceled.Cmd().Transition(*array, RHI::State::RenderTarget, 2);
        }
        context.ExecuteImmediate(
            [&](RHI::CommandList& cmd)
            {
                cmd.Transition(*array, RHI::State::Storage, 1);
                cmd.UAVBarrier(*array);
                cmd.Transition(*array, RHI::State::ShaderResource);
            });
        device->CheckDebugErrors();
        std::cout << device->GetCapabilities().adapterName
                  << ": IBL, CSM/cutouts, local lights, subresources and timestamps passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
