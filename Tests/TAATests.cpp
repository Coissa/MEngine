#include "RenderSceneFixture.h"
#include <Renderer/PostProcess.h>
#include <Renderer/Renderer.h>
#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <Renderer/ShaderLibrary.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace GE;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    double Linear(double color)
    {
        color /= 255.0;
        return color <= 0.04045 ? color / 12.92 : std::pow((color + 0.055) / 1.055, 2.4);
    }

    double ReferenceError(const RHI::Image& image, const RHI::Image& reference)
    {
        const auto scale = reference.width / image.width;
        double error = 0;
        for (uint32_t y = 0; y < image.height; ++y)
            for (uint32_t x = 0; x < image.width; ++x)
                for (uint32_t channel = 0; channel < 3; ++channel)
                {
                    double expected = 0;
                    for (uint32_t j = 0; j < scale; ++j)
                        for (uint32_t i = 0; i < scale; ++i)
                            expected += Linear(
                                reference.rgba[((y * scale + j) * reference.width + x * scale + i) * 4 + channel]);
                    expected /= scale * scale;
                    const auto delta = Linear(image.rgba[(y * image.width + x) * 4 + channel]) - expected;
                    error += delta * delta;
                }
        return error / (image.width * image.height * 3);
    }

    double Difference(const RHI::Image& a, const RHI::Image& b)
    {
        double sum = 0;
        for (size_t i = 0; i < a.rgba.size(); ++i)
            sum += std::abs(int(a.rgba[i]) - int(b.rgba[i]));
        return sum / a.rgba.size();
    }

    void SaveImage(const RHI::Image& image, const std::filesystem::path& path)
    {
        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << image.width << ' ' << image.height << "\n255\n";
        for (size_t i = 0; i < image.rgba.size(); i += 4)
            file.write(reinterpret_cast<const char*>(image.rgba.data() + i), 3);
    }

    void TestResolve(RHI::Device& device, const Render::RHIRenderScene& source)
    {
        Render::PipelineLibrary pipelines(device, source);
        Render::SizeResources size(device, 8, 8);
        auto constants = device.CreateBuffer({256, RHI::Memory::Upload});
        auto input = device.CreateTexture({8, 8, RHI::RHIFormat::R32G32B32A32_Float});
        auto inputView = device.CreateView(*input, RHI::ViewKind::ShaderResource);
        std::array<float, 8 * 8 * 4> pixels{};
        for (uint32_t y = 0; y < 8; ++y)
            for (uint32_t x = 0; x < 8; ++x)
            {
                const auto offset = (y * 8 + x) * 4;
                pixels[offset] = pixels[offset + 1] = pixels[offset + 2] = float(Linear(x < 4 ? 32 : 224));
                pixels[offset + 3] = 1;
            }
        device.UploadTexture(*input, pixels.data(), 8 * 4 * sizeof(float));
        auto& context = device.GetCommandContext();
        auto resolve = [&](bool valid, float motion)
        {
            const Render::TAAConstants data{{1.0f / 8, 1.0f / 8}, {0, 0}, 0.1f, uint32_t(valid)};
            constants->Write(0, &data, sizeof(data));
            auto recording = context.BeginFrame();
            auto& cmd = recording.Cmd();
            cmd.Transition(*size.velocity, RHI::State::RenderTarget);
            cmd.ClearColor(*size.velocityRTV, {motion, 0, 0, 0});
            cmd.Transition(*size.depth, RHI::State::DepthWrite);
            cmd.ClearDepth(*size.depthDSV, 0.5f);
            cmd.Transition(*size.history[0], RHI::State::RenderTarget);
            cmd.ClearColor(*size.historyRTV[0], {0.2f, 0.2f, 0.2f, 8});
            Render::ResolveTAA(cmd, {*inputView, *size.velocitySRV, *size.depthSRV, *size.historySRV[0],
                                     *size.historyRTV[1], *pipelines.taa, *pipelines.taaLayout, *constants});
            Render::DrawSceneOutput(
                cmd, {*size.historySRV[1], *size.colorRTV, *pipelines.display, *pipelines.displayLayout});
            context.Wait(context.Submit(std::move(recording)));
            const auto image = device.ReadbackTexture(*size.color);
            // Display reused the input texture, as in the editor; restore the test input.
            device.UploadTexture(*input, pixels.data(), 8 * 4 * sizeof(float));
            return image;
        };
        const auto fresh = resolve(false, 0);
        const auto accumulated = resolve(true, 0);
        const auto rejected = resolve(true, 2);
        const size_t edge = (4 * 8 + 3) * 4;
        Require(accumulated.rgba[edge] > fresh.rgba[edge] + 4, "TAA did not accumulate valid history");
        Require(fresh.rgba == rejected.rgba, "Out-of-viewport history was not rejected");
        pixels.fill(float(Linear(224)));
        device.UploadTexture(*input, pixels.data(), 8 * 4 * sizeof(float));
        const auto clipped = resolve(true, 0);
        Require(std::abs(int(clipped.rgba[edge]) - 224) <= 1,
                "Uniform colors retained incompatible history or changed gamma");
    }
} // namespace

int main()
{
    try
    {
        const auto shaderRoot = std::filesystem::path(std::getenv("MENGINE_SHADER_DIR"));
        Render::RHIRenderScene source;
        std::string error;
        Require(Tests::BuildTestScene(shaderRoot, source, error), error.c_str());
        auto device = D3D12::CreateDevice({.useWarp = true, .enableDebug = true});
        TestResolve(*device, source);

        Render::RHIMeshDescription triangle;
        triangle.handle = {1};
        triangle.vertices = {{{-0.82f, -0.73f, 0.5f}}, {{0.77f, -0.57f, 0.5f}}, {{-0.15f, 0.84f, 0.5f}}};
        triangle.indices = {0, 1, 2};
        source.meshes = {std::move(triangle)};
        source.skybox.reset();
        Render::RHIGizmoCompositionDescription gizmo;
        const auto gizmoShader = shaderRoot / "editor/editor_gizmo.hlsl";
        Require(Render::LoadShaderBytecode(gizmoShader, "GizmoVS", gizmo.vertex_shader, error) &&
                    Render::LoadShaderBytecode(gizmoShader, "GizmoVisiblePS", gizmo.visible_pixel_shader, error) &&
                    Render::LoadShaderBytecode(gizmoShader, "GizmoOccludedPS", gizmo.occluded_pixel_shader, error),
                error.c_str());
        source.gizmo_composition = std::move(gizmo);
        Render::Renderer renderer(*device, source), referenceRenderer(*device, source);
        renderer.Resize(64, 64);
        referenceRenderer.Resize(256, 256);
        auto output = device->CreateTexture({64, 64, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto target = device->CreateView(*output, RHI::ViewKind::RenderTarget);
        auto referenceOutput = device->CreateTexture({256, 256, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        auto referenceTarget = device->CreateView(*referenceOutput, RHI::ViewKind::RenderTarget);
        auto& context = device->GetCommandContext();
        RenderCore::RenderFrame frame{.output_extent = {64, 64}, .scene_extent = {64, 64}};
        frame.render_items = {{.mesh = {1}, .material = {1}, .object_id = 1}};
        auto draw = [&]()
        {
            auto recording = context.BeginFrame();
            renderer.Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            const auto id = context.Submit(std::move(recording));
            renderer.CommitFrame();
            context.Wait(id);
            ++frame.frame_number;
            return device->ReadbackTexture(*output);
        };

        auto referenceFrame = frame;
        referenceFrame.output_extent = referenceFrame.scene_extent = {256, 256};
        referenceFrame.enable_taa = false;
        auto recording = context.BeginFrame();
        referenceRenderer.Render(recording.Cmd(), recording.SlotIndex(), referenceFrame,
                                 {*referenceTarget, RHI::State::CopySource});
        context.Wait(context.Submit(std::move(recording)));
        referenceRenderer.CommitFrame();
        const auto reference = device->ReadbackTexture(*referenceOutput);
        frame.enable_taa = false;
        const auto raw = draw();
        frame.enable_taa = true;
        const auto first = draw();
        RHI::Image ninth, seventeenth, final;
        for (int i = 1; i <= 24; ++i)
        {
            final = draw();
            if (i == 8)
                ninth = final;
            if (i == 16)
                seventeenth = final;
        }
        const auto rawError = ReferenceError(raw, reference), taaError = ReferenceError(final, reference);
        std::cout << "Supersampled-reference MSE: no AA=" << rawError << ", TAA=" << taaError << '\n';
        Require(taaError < rawError * 0.9, "TAA did not improve the diagonal edge against a supersampled reference");
        Require(Difference(seventeenth, final) <= Difference(first, ninth) + 0.01, "Static TAA did not converge");
        SaveImage(raw, shaderRoot.parent_path() / "taa-off.ppm");
        SaveImage(final, shaderRoot.parent_path() / "taa-on.ppm");
        SaveImage(reference, shaderRoot.parent_path() / "taa-reference.ppm");

        renderer.ResetHistory();
        Require(draw().rgba == first.rgba, "Reset history did not restart the jitter sequence");
        {
            auto canceled = context.BeginFrame();
            renderer.Render(canceled.Cmd(), canceled.SlotIndex(), frame, {*target, RHI::State::CopySource});
        }
        const auto afterCancel = draw();
        renderer.ResetHistory();
        draw();
        Require(draw().rgba == afterCancel.rgba, "Canceled recording advanced TAA history or jitter");

        ++frame.history_revision;
        Require(draw().rgba == first.rgba, "Scene revision retained old TAA history");
        ++frame.main_view.camera_id;
        Require(draw().rgba == first.rgba, "Camera switch retained old TAA history");
        renderer.Resize(64, 64);
        Require(draw().rgba == first.rgba, "Resize retained old TAA history");
        frame.enable_taa = false;
        Require(draw().rgba == raw.rgba, "Disabling TAA did not restore the original scene");
        frame.enable_taa = true;
        Require(draw().rgba == first.rgba, "Re-enabling TAA did not reset history");
        frame.visualize_motion_vectors = true;
        const auto motion = draw();
        const size_t center = (32 * 64 + 32) * 4;
        Require(std::abs(int(motion.rgba[center]) - 128) <= 1 && std::abs(int(motion.rgba[center + 1]) - 128) <= 1,
                "Static motion visualization included projection jitter");
        frame.visualize_motion_vectors = false;
        Require(draw().rgba == first.rgba, "Leaving velocity visualization did not reset history");

        frame.gizmo.vertices = {
            {{-0.3f, -0.3f, 0, 1}, 0xffffffff}, {{0, 0.3f, 0, 1}, 0xffffffff}, {{0.3f, -0.3f, 0, 1}, 0xffffffff}};
        frame.gizmo.indices = {0, 1, 2};
        const auto overlay = draw();
        Require(overlay.rgba[center] == 255 && overlay.rgba[center + 2] == 255, "Gizmo was filtered by TAA");
        frame.gizmo = {};
        const auto removedOverlay = draw();
        renderer.ResetHistory();
        draw();
        draw();
        Require(draw().rgba == removedOverlay.rgba, "Gizmo contaminated the temporal history");

        frame.render_items[0].local_to_world = Math::TranslationMatrix({0.65f, 0, 0});
        const auto moved = draw();
        frame.render_items.clear();
        frame.enable_taa = false;
        const auto background = draw();
        Require(std::abs(int(moved.rgba[center]) - int(background.rgba[center])) <= 1 &&
                    std::abs(int(moved.rgba[center + 1]) - int(background.rgba[center + 1])) <= 1,
                "Moving geometry left a history trail in the exposed background");
        device->CheckDebugErrors();
        std::cout << "TAA accumulation, clipping, gamma, convergence, reset, cancellation and Gizmo isolation passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
