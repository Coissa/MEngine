#include "ReSTIRGIPass.h"
#include <algorithm>

namespace GE::Render
{
    namespace
    {
        struct GIInstance
        {
            Math::Mat4 world;
            std::array<float, 4> albedo, emissive;
            std::array<uint32_t, 4> geometry;
        };
        static_assert(sizeof(GIInstance) == 112);
        struct GIConstants
        {
            Math::Mat4 cameraWorld;
            std::array<float, 4> projection, sun, sizeJitter, previousJitter;
            std::array<uint32_t, 4> frame;
        };
    } // namespace

    void ReSTIRGIPass::Resize(uint32_t width, uint32_t height)
    {
        using namespace RHI;
        normal = device.CreateTexture({width, height, RHIFormat::R16G16B16A16_Float, true});
        diffuse = device.CreateTexture({width, height, RHIFormat::R16G16B16A16_Float, true});
        output = device.CreateTexture({width, height, RHIFormat::R16G16B16A16_Float, false, false, 1, 1, false, true});
        normalRTV = device.CreateView(*normal, ViewKind::RenderTarget);
        normalSRV = device.CreateView(*normal, ViewKind::ShaderResource);
        diffuseRTV = device.CreateView(*diffuse, ViewKind::RenderTarget);
        diffuseSRV = device.CreateView(*diffuse, ViewKind::ShaderResource);
        outputUAV = device.CreateView(*output, ViewKind::Storage);
        outputSRV = device.CreateView(*output, ViewKind::ShaderResource);
        for (auto& reservoir : reservoirs)
            reservoir = device.CreateBuffer({size_t(width) * height * 96, Memory::GPU, true});
        ResetHistory();
    }

    void ReSTIRGIPass::ResetHistory() noexcept
    {
        previous.valid = pending.valid = false;
    }

    void ReSTIRGIPass::CommitFrame() noexcept
    {
        if (pending.valid)
        {
            std::swap(previous, pending);
            pending.valid = false;
        }
    }

    RHI::TextureView& ReSTIRGIPass::Execute(RHI::CommandList& cmd, uint32_t slot, const RenderCore::RenderFrame& frame,
                                            SceneAssets& assets, PipelineLibrary& pipelines, RHI::Buffer& lighting,
                                            RHI::TextureView& depth, RHI::TextureView& color,
                                            RHI::TextureView& velocity, std::array<float, 2> jitter,
                                            uint32_t frameIndex)
    {
        using namespace RHI;
        std::vector<RayTracingInstance> instances;
        const bool reuseHistory = previous.valid && previous.cameraId == frame.main_view.camera_id &&
                                  previous.revision == frame.history_revision &&
                                  previous.projection == frame.main_view.projection &&
                                  previous.items == frame.render_items && previous.lighting == frame.rendering &&
                                  previous.mode == frame.gi_mode;
        pending.items = frame.render_items;
        pending.lighting = frame.rendering;
        pending.projection = frame.main_view.projection;
        pending.cameraId = frame.main_view.camera_id;
        pending.revision = frame.history_revision;
        pending.jitter = jitter;
        pending.index = previous.valid ? 1 - previous.index : 0;
        pending.mode = frame.gi_mode;
        pending.valid = true;
        std::vector<GIInstance> materials;
        for (const auto& item : frame.render_items)
        {
            const auto found = assets.meshes.find(item.mesh.value);
            if (found == assets.meshes.end() || !found->second.rayTracing || item.color_tint[3] < 0)
                continue;
            const auto& mesh = found->second;
            const auto& material = assets.materials.at(item.material.value);
            if (material.alphaCutoff > 0)
                continue; // First version traces opaque geometry only.
            instances.push_back({mesh.rayTracing.get(), item.local_to_world, uint32_t(materials.size())});
            materials.push_back(
                {item.local_to_world,
                 {material.baseColor[0] * item.color_tint[0], material.baseColor[1] * item.color_tint[1],
                  material.baseColor[2] * item.color_tint[2], material.metallic},
                 {material.emissive[0], material.emissive[1], material.emissive[2], 0},
                 {mesh.triangleOffset, 0, 0, 0}});
        }
        auto& resources = frames[slot];
        const uint32_t capacity = (std::max)(1u, uint32_t(instances.size()));
        if (resources.capacity < capacity)
        {
            resources.capacity = capacity;
            resources.scene = device.CreateRayTracingScene(capacity);
            resources.instances = device.CreateBuffer({capacity * sizeof(GIInstance), Memory::Upload});
        }
        if (!resources.constants)
            resources.constants = device.CreateBuffer({256, Memory::Upload});
        if (!materials.empty())
            resources.instances->Write(0, materials.data(), materials.size() * sizeof(GIInstance));
        const auto& projection = frame.main_view.projection;
        const auto& size = output->Description();
        GIConstants constants;
        Math::TryInverseAffine(frame.main_view.view, constants.cameraWorld);
        constants.projection = {projection.At(0, 0), projection.At(1, 1), projection.At(2, 2), projection.At(3, 2)};
        constants.sun = {frame.rendering.light_direction[0], frame.rendering.light_direction[1],
                         frame.rendering.light_direction[2], frame.rendering.light_intensity};
        constants.sizeJitter = {float(size.width), float(size.height), jitter[0], jitter[1]};
        constants.previousJitter = {previous.jitter[0], previous.jitter[1], 0, 0};
        constants.frame = {frameIndex, uint32_t(reuseHistory), frame.gi_mode, frame.gi_mode >= 3 ? 4u : 0u};
        resources.constants->Write(0, &constants, sizeof(constants));
        cmd.BuildRayTracingScene(*resources.scene, instances);
        cmd.Transition(*assets.giTriangles, State::ShaderResource);
        for (auto* view : {&depth, &color, normalSRV.get(), diffuseSRV.get()})
            cmd.Transition(view->Resource(), State::ShaderResource);
        auto& environment =
            assets.hasEnvironment ? *assets.textures.at(assets.skyTexture).view : *assets.irradiance.view;
        cmd.Transition(environment.Resource(), State::ShaderResource);
        cmd.Transition(*output, State::Storage);
        const uint32_t write = pending.index;
        cmd.Transition(*reservoirs[1 - write], State::ShaderResource);
        cmd.Transition(*reservoirs[write], State::Storage);
        cmd.Transition(velocity.Resource(), State::ShaderResource);
        cmd.SetComputePipeline(*pipelines.gi);
        cmd.SetBindingLayout(*pipelines.giLayout);
        cmd.BindConstantBuffer(0, *resources.constants);
        cmd.BindConstantBuffer(1, lighting);
        cmd.BindRayTracingScene(2, *resources.scene);
        cmd.BindBuffer(3, *assets.giTriangles);
        cmd.BindBuffer(4, *resources.instances);
        cmd.BindTexture(5, depth);
        cmd.BindTexture(6, *normalSRV);
        cmd.BindTexture(7, *diffuseSRV);
        cmd.BindTexture(8, color);
        cmd.BindTexture(9, *outputUAV);
        cmd.BindTexture(10, environment);
        cmd.BindSampler(11, *assets.environmentSampler);
        cmd.BindBuffer(12, *reservoirs[1 - write]);
        cmd.BindBuffer(13, *reservoirs[write], true);
        cmd.BindTexture(14, velocity);
        cmd.Dispatch((size.width + 7) / 8, (size.height + 7) / 8);
        cmd.Transition(*output, State::ShaderResource);
        cmd.Transition(*reservoirs[write], State::ShaderResource);
        return *outputSRV;
    }
} // namespace GE::Render
