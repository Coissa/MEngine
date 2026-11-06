#include <Editor/ClusterDebugColorizer.h>
#include "RenderSceneFixture.h"
#include <Platform/Windows/Win32Application.h>
#include <Renderer/Nanite/ClusterBuilder/Cluster.h>
#include <Renderer/RHI/D3D12/D3D12BackendFactory.h>
#include <Renderer/Renderer.h>
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
    void SaveImage(const RHI::Image& image, const std::filesystem::path& path)
    {
        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << image.width << ' ' << image.height << "\n255\n";
        for (size_t i = 0; i < image.rgba.size(); i += 4)
            file.write(reinterpret_cast<const char*>(image.rgba.data() + i), 3);
    }
} // namespace
int main()
{
    try
    {
        auto device = D3D12::CreateDevice({.useWarp = true, .enableDebug = true});
        auto& context = device->GetCommandContext();
        auto texture = device->CreateTexture({64, 64, RHI::RHIFormat::R8G8B8A8_UNorm, true});
        auto target = device->CreateView(*texture, RHI::ViewKind::RenderTarget);
        {
            auto canceled = context.BeginFrame();
            canceled.Cmd().Transition(*texture, RHI::State::RenderTarget);
        }
        std::array<bool, RHI::FrameSlotCount> slots{};
        for (uint32_t frame = 0; frame < 9; ++frame)
        {
            auto recording = context.BeginFrame();
            slots[recording.SlotIndex()] = true;
            recording.Cmd().Transition(*texture, RHI::State::RenderTarget);
            recording.Cmd().SetRenderTargets(*target);
            recording.Cmd().ClearColor(*target, {1, 0, 0, 1});
            context.Submit(std::move(recording));
        }
        const auto image = device->ReadbackTexture(*texture);
        Require(image.rgba[0] == 255 && image.rgba[1] == 0, "Canceled recording or frame state corrupted output");
        Require(slots[0] && slots[1] && slots[2], "Three frame slots were not exercised");
        auto window = Platform::Windows::Win32Application::Create({.title = "RHI test", .client_size = {64, 64}});
        Require(bool(window), "Test window creation failed");
        auto swapchain = D3D12::CreateSwapchain(
            *device, reinterpret_cast<void*>(window->GetNativeWindowHandle().window), {64, 64, 2});
        for (uint32_t frame = 0; frame < 9; ++frame)
        {
            if (frame == 3 || frame == 6)
            {
                context.WaitIdle();
                swapchain->Resize(frame == 3 ? 80 : 96, 64);
            }
            auto recording = context.BeginFrame();
            auto output = swapchain->GetCurrentTarget();
            recording.Cmd().Transition(output.color.Resource(), RHI::State::RenderTarget);
            recording.Cmd().SetRenderTargets(output.color);
            recording.Cmd().ClearColor(output.color, {0, 0.5f, 0.5f, 1});
            recording.Cmd().Transition(output.color.Resource(), RHI::State::Present);
            context.Submit(std::move(recording));
            swapchain->Present(false);
        }
        context.WaitIdle();
        swapchain.reset();
        auto shaderRoot = std::filesystem::path(std::getenv("MENGINE_SHADER_DIR"));
        Render::RHIRenderScene source;
        std::string error;
        Require(Tests::BuildTestScene(shaderRoot, source, error), error.c_str());
        const auto directSource = source;
        source.gpu_driven.reset();
        Render::Renderer direct(*device, std::move(source));
        Render::Renderer culled(*device, directSource);
        target.reset();
        texture = device->CreateTexture({128, 128, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        // Release the old view before the texture it borrowed.
        target.reset();
        target = device->CreateView(*texture, RHI::ViewKind::RenderTarget);
        direct.Resize(128, 128);
        culled.Resize(128, 128);
        RenderCore::RenderFrame frame{.output_extent = {128, 128}, .scene_extent = {128, 128}};
        Tests::BuildRotatingCubeRenderFrame(0, frame);
        auto draw = [&](Render::Renderer& renderer)
        {
            auto recording = context.BeginFrame();
            renderer.Render(recording.Cmd(), recording.SlotIndex(), frame, {*target, RHI::State::CopySource});
            context.Wait(context.Submit(std::move(recording)));
            renderer.CommitFrame();
            return device->ReadbackTexture(*texture);
        };
        auto a = draw(direct), b = draw(culled);
        Require(a.rgba == b.rgba, "Compute culling changed the visible scene");
        bool variation = false;
        for (size_t i = 4; i < a.rgba.size(); i += 4)
            if (a.rgba[i] != a.rgba[0] || a.rgba[i + 1] != a.rgba[1])
            {
                variation = true;
                break;
            }
        Require(variation, "Scene did not render any variation");
        SaveImage(a, shaderRoot.parent_path() / "render-smoke.ppm");

        AssetFormats::StaticMeshAsset grid;
        constexpr uint32_t divisions = 10;
        grid.nodes.push_back({.parent = -1, .local_transform = Math::Mat4{}, .first_submesh = 0, .submesh_count = 1});
        for (uint32_t y = 0; y <= divisions; ++y)
            for (uint32_t x = 0; x <= divisions; ++x)
                grid.vertices.push_back({.position = {2.0f * x / divisions - 1, 2.0f * y / divisions - 1, 0},
                                         .normal = {0, 0, -1},
                                         .texcoord = {float(x) / divisions, float(y) / divisions}});
        for (uint32_t y = 0; y < divisions; ++y)
            for (uint32_t x = 0; x < divisions; ++x)
            {
                const uint32_t first = y * (divisions + 1) + x;
                for (uint32_t index :
                     {first, first + divisions + 1, first + 1, first + 1, first + divisions + 1, first + divisions + 2})
                    grid.indices.push_back(index);
            }
        grid.submeshes.push_back({.first_index = 0,
                                  .index_count = uint32_t(grid.indices.size()),
                                  .material_slot = 0,
                                  .bounds_min = {-1, -1, 0},
                                  .bounds_max = {1, 1, 0}});
        const auto dags = Render::Nanite::ReduceMesh(grid);
        auto debug = Editor::BuildClusterDebugGeometry(grid, Render::Nanite::PackClusterDAG(dags), 2);
        Require(debug.cluster_count >= 2, "Cluster fixture did not exercise multiple leaf ranges");
        auto clusteredSource = directSource;
        clusteredSource.nanite_meshes.push_back(std::move(debug.mesh));
        Render::Renderer clustered(*device, std::move(clusteredSource));
        clustered.Resize(128, 128);
        frame.render_items = std::move(debug.render_items);
        for (auto& item : frame.render_items)
            item.color_tint = {0, 8, 0, -1};
        const auto clusters = draw(clustered);
        bool blue = false, green = false;
        for (size_t i = 0; i < clusters.rgba.size(); i += 4)
        {
            blue |= clusters.rgba[i] >= 60 && clusters.rgba[i] <= 70 && clusters.rgba[i + 2] > 250;
            green |= clusters.rgba[i] > 150 && clusters.rgba[i + 1] > 250 && clusters.rgba[i + 2] <= 70;
        }
        Require(blue && green, "Cluster index/base-vertex ranges did not draw both leaf colors");
        SaveImage(clusters, shaderRoot.parent_path() / "cluster-smoke.ppm");
        context.WaitIdle();
        device->CheckDebugErrors();
        std::cout << "Recording cancellation, 3 slots / 2 backbuffers, resize, "
                     "bytecode rendering and GPU culling passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
