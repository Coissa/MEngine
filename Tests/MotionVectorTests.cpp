#include "RenderSceneFixture.h"
#include <Renderer/Renderer.h>
#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <Renderer/ShaderLibrary.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace GE;

namespace
{
    void ExpectMotion(const RHI::Image& image, uint32_t x, uint32_t y, float pixelsX, float pixelsY,
                      const char* message)
    {
        const size_t offset = (size_t(y) * image.width + x) * 4;
        const auto encode = [](float pixels)
        { return int(std::lround(std::clamp(0.5f + pixels / 32.0f, 0.0f, 1.0f) * 255)); };
        if (std::abs(int(image.rgba[offset]) - encode(pixelsX)) > 2 ||
            std::abs(int(image.rgba[offset + 1]) - encode(pixelsY)) > 2 ||
            std::abs(int(image.rgba[offset + 2]) - 128) > 1)
            throw std::runtime_error(message);
    }
} // namespace

int main()
{
    try
    {
        const auto shaderRoot = std::filesystem::path(std::getenv("MENGINE_SHADER_DIR"));
        Render::RHIRenderScene source;
        std::string error;
        if (!Tests::BuildTestScene(shaderRoot, source, error))
            throw std::runtime_error(error);

        Render::RHIMeshDescription plane;
        plane.handle = {1};
        for (const auto& position :
             std::array<Math::Vec3, 4>{Math::Vec3{-0.2f, -0.2f, 0.5f}, Math::Vec3{-0.2f, 0.2f, 0.5f},
                                       Math::Vec3{0.2f, 0.2f, 0.5f}, Math::Vec3{0.2f, -0.2f, 0.5f}})
            plane.vertices.push_back({.position = {position[0], position[1], position[2]}});
        plane.indices = {0, 1, 2, 0, 2, 3};
        source.meshes = {std::move(plane)};
        Render::RHIGizmoCompositionDescription gizmo;
        const auto gizmoShader = shaderRoot / "editor/editor_gizmo.hlsl";
        if (!Render::LoadShaderBytecode(gizmoShader, "GizmoVS", gizmo.vertex_shader, error) ||
            !Render::LoadShaderBytecode(gizmoShader, "GizmoVisiblePS", gizmo.visible_pixel_shader, error) ||
            !Render::LoadShaderBytecode(gizmoShader, "GizmoOccludedPS", gizmo.occluded_pixel_shader, error))
            throw std::runtime_error(error);
        source.gizmo_composition = std::move(gizmo);

        auto device = D3D12::CreateDevice({.useWarp = true, .enableDebug = true});
        auto& context = device->GetCommandContext();
        Render::Renderer renderer(*device, std::move(source));
        renderer.Resize(64, 64);
        auto output = device->CreateTexture({64, 64, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto target = device->CreateView(*output, RHI::ViewKind::RenderTarget);
        RenderCore::RenderFrame frame{.output_extent = {64, 64}, .scene_extent = {64, 64}};
        frame.visualize_motion_vectors = true;
        frame.render_items = {
            {.mesh = {1}, .material = {1}, .object_id = 101, .local_to_world = Math::TranslationMatrix({-0.45f, 0, 0})},
            {.mesh = {1}, .material = {1}, .object_id = 202, .local_to_world = Math::TranslationMatrix({0.45f, 0, 0})}};
        auto draw = [&]()
        {
            auto recording = context.BeginFrame();
            renderer.Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            const auto submission = context.Submit(std::move(recording));
            renderer.CommitFrame();
            context.Wait(submission);
            ++frame.frame_number;
            return device->ReadbackTexture(*output);
        };

        auto image = draw();
        ExpectMotion(image, 18, 32, 0, 0, "First-frame motion was not zero");
        frame.render_items[0].local_to_world = Math::TranslationMatrix({-0.325f, 0, 0});
        std::swap(frame.render_items[0], frame.render_items[1]);
        image = draw();
        ExpectMotion(image, 22, 32, -4, 0, "Object motion did not survive item reordering");
        ExpectMotion(image, 46, 32, 0, 0, "Moving one instance affected a static instance of the same mesh");
        for (int i = 0; i < 4; ++i)
            ExpectMotion(draw(), 22, 32, 0, 0, "History followed frame slots instead of the last submitted scene");

        frame.main_view.view = Math::TranslationMatrix({-0.125f, 0, 0});
        image = draw();
        ExpectMotion(image, 42, 32, 4, 0, "Camera translation produced incorrect surface motion");
        ExpectMotion(image, 2, 2, 0, 0, "Camera translation moved the sky");

        frame.main_view.view = {};
        frame.render_items = {{.mesh = {1}, .material = {1}, .object_id = 101}};
        renderer.ResetHistory();
        draw();
        frame.render_items[0].local_to_world = Math::TranslationMatrix({0, 0.125f, 0});
        ExpectMotion(draw(), 32, 28, 0, 4, "Vertical motion used the wrong UV convention");
        frame.render_items[0].object_id = 303;
        frame.render_items[0].local_to_world = Math::TranslationMatrix({0.125f, 0.125f, 0});
        ExpectMotion(draw(), 36, 28, 0, 0, "A new object inherited a different object's transform");
        frame.render_items.clear();
        draw();
        frame.render_items = {{.mesh = {1}, .material = {1}, .object_id = 303}};
        ExpectMotion(draw(), 32, 32, 0, 0, "A reappearing object reused stale transform history");

        {
            frame.render_items[0].local_to_world = Math::TranslationMatrix({0.125f, 0, 0});
            auto canceled = context.BeginFrame();
            renderer.Render(canceled.Cmd(), canceled.SlotIndex(), frame, {*target, RHI::State::CopySource});
        }
        frame.render_items[0].local_to_world = Math::TranslationMatrix({0.25f, 0, 0});
        ExpectMotion(draw(), 40, 32, -8, 0, "Canceled recording advanced motion history");

        ++frame.history_revision;
        frame.render_items[0].local_to_world = {};
        ExpectMotion(draw(), 32, 32, 0, 0, "Scene revision did not reset history");
        ++frame.main_view.camera_id;
        frame.main_view.view = Math::TranslationMatrix({-0.125f, 0, 0});
        ExpectMotion(draw(), 28, 32, 0, 0, "Camera switch did not reset history");
        frame.main_view.view = {};
        renderer.Resize(64, 64);
        ExpectMotion(draw(), 32, 32, 0, 0, "Resize did not reset history");
        frame.main_view.valid = false;
        frame.render_items.clear();
        draw();
        frame.main_view.valid = true;
        frame.render_items = {
            {.mesh = {1}, .material = {1}, .object_id = 303, .local_to_world = Math::TranslationMatrix({0.25f, 0, 0})}};
        ExpectMotion(draw(), 40, 32, 0, 0, "Invalid view did not invalidate motion history");

        frame.gizmo.vertices = {
            {{-0.2f, -0.2f, 0, 1}, 0xffffffff}, {{0, 0.2f, 0, 1}, 0xffffffff}, {{0.2f, -0.2f, 0, 1}, 0xffffffff}};
        frame.gizmo.indices = {0, 1, 2};
        image = draw();
        const size_t center = (32 * image.width + 32) * 4;
        if (image.rgba[center] != 255 || image.rgba[center + 1] != 255 || image.rgba[center + 2] != 255)
            throw std::runtime_error("Gizmo did not render after switching back to a single color target");
        frame.gizmo = {};

        frame.render_items.clear();
        frame.main_view.projection = Math::PerspectiveFovLH_ZO(Math::Radians{1.0f}, 1, 0.1f, 100);
        ExpectMotion(draw(), 32, 32, 0, 0, "Projection change did not reset history");
        frame.main_view.view = Math::TranslationMatrix({-1, 0, 0});
        ExpectMotion(draw(), 32, 32, 0, 0, "Perspective sky acquired camera-translation motion");
        frame.main_view.view = Math::RotationYMatrix(Math::Radians{0.05f});
        image = draw();
        if (std::abs(int(image.rgba[center]) - 128) < 8)
            throw std::runtime_error("Camera rotation did not produce sky motion");

        frame.main_view.view = {};
        renderer.ResetHistory();
        frame.render_items = {
            {.mesh = {1}, .material = {1}, .object_id = 404, .local_to_world = Math::TranslationMatrix({0, 0, -2})}};
        draw();
        frame.render_items[0].local_to_world = Math::TranslationMatrix({0, 0, 2});
        image = draw();
        if (image.rgba[center] != 255 || image.rgba[center + 1] != 255)
            throw std::runtime_error("A previous point behind the camera did not reject reprojection");

        device->CheckDebugErrors();
        std::cout << "Object/camera/sky motion, stable IDs, committed history, reset and Gizmo output passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
