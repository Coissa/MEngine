#include <Editor/SceneResources.h>
#include <Core/Math/angle.h>
#include <Renderer/Renderer.h>
#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <cmath>
#include <cstdlib>
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

    double Linear(double srgb)
    {
        return srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
    }

    int Display(double radiance)
    {
        const double mapped = 1 - std::exp(-radiance);
        return int(
            std::lround(255 * (mapped <= 0.0031308 ? mapped * 12.92 : 1.055 * std::pow(mapped, 1 / 2.4) - 0.055)));
    }
} // namespace

int main(int count, char** arguments)
{
    try
    {
        const bool hardware = count > 1 && std::string_view(arguments[1]) == "--hardware";
        auto device = D3D12::CreateDevice({.useWarp = !hardware, .enableDebug = true});
        Render::RHIRenderScene scene;
        std::string error;
        Require(Editor::BuildEditorScene(std::getenv("MENGINE_SHADER_DIR"), scene, error), error.c_str());
        scene.skybox.reset();
        scene.ui_composition.reset();
        scene.gizmo_composition.reset();
        scene.gpu_driven.reset();
        scene.textures.clear();
        Render::RHIMeshDescription quad;
        quad.handle = {1};
        quad.vertices = {{{-1, -1, 0.5f}, {0, 0, 1}, {1, 1, 1}, {0, 1}},
                         {{1, -1, 0.5f}, {0, 0, 1}, {1, 1, 1}, {1, 1}},
                         {{1, 1, 0.5f}, {0, 0, 1}, {1, 1, 1}, {1, 0}},
                         {{-1, 1, 0.5f}, {0, 0, 1}, {1, 1, 1}, {0, 0}}};
        quad.indices = {0, 1, 2, 0, 2, 3};
        scene.meshes = {quad};
        const auto texture = [&](uint32_t id, std::initializer_list<std::byte> pixels, uint32_t width = 1)
        {
            Render::RHITextureDescription value;
            value.handle = {id};
            value.width = width;
            value.height = 1;
            value.format = RHI::RHIFormat::R8G8B8A8_UNorm;
            value.pixels = pixels;
            scene.textures.push_back(std::move(value));
        };
        texture(1, {std::byte{128}, std::byte{128}, std::byte{128}, std::byte{255}});
        texture(2, {std::byte{255}, std::byte{128}, std::byte{128}, std::byte{255}});
        texture(3,
                {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{255}, std::byte{255}, std::byte{255},
                 std::byte{255}, std::byte{255}},
                2);
        texture(4, {std::byte{128}, std::byte{128}, std::byte{128}, std::byte{64}});
        scene.materials.clear();
        Render::RHIMaterialDescription material;
        material.handle = {1};
        material.albedo_texture = {1};
        scene.materials.push_back(material); // Color texture, decoded by the sRGB view.
        material.handle = {2};
        material.albedo_texture = {0};
        material.base_color = {float(Linear(128.0 / 255)), float(Linear(128.0 / 255)), float(Linear(128.0 / 255)), 1};
        scene.materials.push_back(material); // Equivalent linear factor, without a texture.
        material.handle = {3};
        material.base_color = {1, 1, 1, 1};
        material.roughness = 0.5f;
        scene.materials.push_back(material); // Dielectric, normal incidence.
        material.handle = {4};
        material.metallic = 1;
        scene.materials.push_back(material); // Metal, same roughness.
        material.handle = {5};
        material.metallic = 0;
        material.normal_texture = {2};
        scene.materials.push_back(material);
        material.handle = {6};
        material.normal_texture = {0};
        material.metallic = material.roughness = 1;
        material.metallic_roughness_texture = {1};
        scene.materials.push_back(material);
        material.handle = {7};
        material.metallic_roughness_texture = {0};
        material.metallic = material.roughness = 128.0f / 255;
        scene.materials.push_back(material);
        material.handle = {8};
        material.metallic = 0;
        material.emissive = {4, 4, 4};
        scene.materials.push_back(material);
        material.handle = {9};
        material.emissive = {2, 2, 2};
        material.emissive_texture = {1};
        scene.materials.push_back(material);
        material.handle = {10};
        material.emissive = {};
        material.emissive_texture = {0};
        material.albedo_texture = {3};
        scene.materials.push_back(material);

        material.handle = {11};
        material.albedo_texture = {0};
        material.occlusion_texture = {1};
        scene.materials.push_back(material);
        material.handle = {12};
        material.albedo_texture = {4};
        material.occlusion_texture = {0};
        material.alpha_cutoff = 0.5f;
        scene.materials.push_back(material);

        auto displayOnlyScene = scene;
        displayOnlyScene.post_process->tone_map_pixel_shader.clear();
        Render::Renderer renderer(*device, std::move(scene));
        renderer.Resize(64, 64);
        auto output = device->CreateTexture({64, 64, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto target = device->CreateView(*output, RHI::ViewKind::RenderTarget);
        RenderCore::RenderFrame frame{.output_extent = {64, 64}, .scene_extent = {64, 64}};
        frame.main_view.camera_world_position = {0, 0, 3, 1};
        frame.rendering.normal_debug = false;
        frame.rendering.light_direction = {0, 0, 1};
        frame.rendering.exposure = 1;
        frame.enable_taa = false;
        frame.render_items = {{.mesh = {1}, .material = {1}, .object_id = 1}};
        auto& context = device->GetCommandContext();
        const auto draw = [&](uint32_t id, float ambient, float intensity)
        {
            frame.render_items[0].material = {id};
            frame.rendering.ambient = ambient;
            frame.rendering.light_intensity = intensity;
            renderer.ResetHistory();
            auto recording = context.BeginFrame();
            renderer.Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            context.Wait(context.Submit(std::move(recording)));
            renderer.CommitFrame();
            return device->ReadbackTexture(*output).rgba[(32 * 64 + 32) * 4];
        };
        const int gray = draw(1, 1, 0);
        Require(std::abs(gray - Display(Linear(128.0 / 255))) <= 2,
                "Base color was not decoded from sRGB exactly once");
        Require(std::abs(gray - draw(2, 1, 0)) <= 1, "Linear material factors and color textures disagree");
        const int diffuse = draw(3, 0, 1);
        Require(std::abs(diffuse - Display(0.96 / Math::Pi + 0.04 / (4 * Math::Pi * 0.0625))) <= 2,
                "Dielectric GGX/Fresnel response differs from the normal-incidence reference");
        Require(std::abs(draw(4, 0, 1) - Display(1 / (4 * Math::Pi * 0.0625))) <= 2,
                "Metallic GGX response is incorrect");
        Require(draw(5, 0, 1) + 30 < diffuse, "Tangent-space normal texture did not change illumination");
        Require(std::abs(int(draw(6, 0, 1)) - int(draw(7, 0, 1))) <= 1,
                "Metallic-roughness texture was treated as sRGB");
        Require(std::abs(draw(9, 0, 0) - Display(2 * Linear(128.0 / 255))) <= 2, "Emissive texture is not sRGB");
        Require(std::abs(draw(10, 1, 0) - Display(0.515625)) <= 2, "Color filtering did not occur in linear space");
        Require(std::abs(draw(11, 1, 0) - Display(128.0 / 255)) <= 2,
                "Occlusion texture is not linear or affected the wrong lighting term");
        Require(std::abs(draw(12, 1, 0) - Display(0.025)) <= 2,
                "Alpha mask did not discard the surface before lighting");
        frame.enable_taa = true;
        Require(std::abs(draw(8, 0, 0) - Display(4)) <= 2, "TAA clipped HDR emissive radiance before tone mapping");
        Render::Renderer displayOnly(*device, std::move(displayOnlyScene));
        displayOnly.Resize(64, 64);
        frame.render_items[0].material = {1};
        frame.rendering.ambient = 1;
        frame.rendering.light_intensity = 0;
        for (bool temporal : {false, true})
        {
            frame.enable_taa = temporal;
            auto recording = context.BeginFrame();
            displayOnly.Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            context.Wait(context.Submit(std::move(recording)));
            displayOnly.CommitFrame();
            const auto display = device->ReadbackTexture(*output);
            Require(std::abs(int(display.rgba[(32 * 64 + 32) * 4]) - 128) <= 1,
                    "Disabling tone mapping applied sRGB encoding twice");
        }
        device->CheckDebugErrors();
        std::cout << device->GetCapabilities().adapterName
                  << ": PBR, material texture roles, linear filtering and HDR TAA passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
