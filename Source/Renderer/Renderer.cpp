#include "Renderer.h"
#include "PostProcess.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
namespace GE::Render
{
    namespace
    {
        struct FrameConstants
        {
            Math::Mat4 viewProjection, previousViewProjection, rasterViewProjection;
            std::array<float, 4> camera;
            std::array<float, 4> lightDirectionIntensity, ambientExposureDebug;
        };
        struct ObjectConstants
        {
            Math::Mat4 world, normal, previousWorld;
            std::array<float, 4> tint;
            std::array<float, 4> material, pbr, emissive;
        };
        static_assert(sizeof(ObjectConstants) == 256);
        struct UIConstants
        {
            std::array<float, 2> scale, translate;
        };
        struct SkyConstants
        {
            Math::Mat4 viewProjection, previousViewProjection, rasterViewProjection;
            float intensity, exposure;
            float encodeOutput = 0, rotation = 0;
        };

        LightingConstants MakeLighting(const RenderCore::RenderFrame& frame, const SceneAssets& assets, bool shadows)
        {
            LightingConstants data;
            const auto& settings = frame.rendering;
            data.view = frame.main_view.view;
            data.environment = {
                assets.hasEnvironment ? settings.environment_intensity * assets.skyIntensity * assets.skyExposure : 0,
                settings.environment_rotation * Math::Pi / 180.0f,
                float(assets.prefilter.texture->Description().mipLevels - 1), 0};
            data.sunColor = {settings.light_color[0], settings.light_color[1], settings.light_color[2], 0};
            const auto count = (std::min)(settings.lights.size(), size_t(RenderCore::MaxLocalLights));
            data.counts[0] = float(count);
            for (size_t i = 0; i < count; ++i)
            {
                const auto& source = settings.lights[i];
                Math::Vec3 direction{source.direction[0], source.direction[1], source.direction[2]};
                Math::Normalize(direction);
                data.lights[i] = {
                    {source.position[0], source.position[1], source.position[2], source.range},
                    {direction[0], direction[1], direction[2], std::cos(source.outer_angle * Math::Pi / 180)},
                    {source.color[0], source.color[1], source.color[2], source.intensity},
                    {source.spot ? 1.0f : 0.0f, std::cos(source.inner_angle * Math::Pi / 180), 0, 0}};
            }
            if (!shadows)
                return data;
            data.shadowParameters = {1, 1.0f / ShadowResolution, 0.0003f, settings.shadow_distance};
            Math::Mat4 cameraWorld;
            Math::TryInverseAffine(frame.main_view.view, cameraWorld);
            const auto& projection = frame.main_view.projection;
            const float nearPlane = -projection.At(3, 2) / projection.At(2, 2);
            const float farPlane =
                (std::max)(nearPlane * 1.01f,
                           (std::min)(settings.shadow_distance, -projection.At(3, 2) / (projection.At(2, 2) - 1)));
            Math::Vec3 forward{-settings.light_direction[0], -settings.light_direction[1],
                               -settings.light_direction[2]};
            if (!Math::Normalize(forward))
                forward = {0, -1, 0};
            Math::Vec3 right =
                Math::Cross(std::abs(forward[1]) < 0.99f ? Math::Vec3{0, 1, 0} : Math::Vec3{0, 0, 1}, forward);
            Math::Normalize(right);
            const auto up = Math::Cross(forward, right);
            float begin = nearPlane;
            for (uint32_t cascade = 0; cascade < ShadowCascades; ++cascade)
            {
                const float t = float(cascade + 1) / ShadowCascades;
                const float end = 0.6f * nearPlane * std::pow(farPlane / nearPlane, t) +
                                  0.4f * (nearPlane + (farPlane - nearPlane) * t);
                data.splits[cascade] = end;
                std::array<Math::Vec3, 8> corners;
                Math::Vec3 center{};
                size_t index = 0;
                for (float depth : {cascade ? begin * 0.9f : begin, end})
                    for (float y : {-1.0f, 1.0f})
                        for (float x : {-1.0f, 1.0f})
                        {
                            auto p = Math::TransformPoint(
                                {x * depth / projection.At(0, 0), y * depth / projection.At(1, 1), depth}, cameraWorld);
                            corners[index++] = p;
                            center = center + p * 0.125f;
                        }
                float radius = 0;
                for (const auto& p : corners)
                    radius = (std::max)(radius, std::sqrt(Math::Dot(p - center, p - center)));
                radius = std::ceil(radius * 16) / 16;
                const float texel = 2 * radius / ShadowResolution;
                const float cx = Math::Dot(center, right), cy = Math::Dot(center, up);
                center =
                    center + right * (std::round(cx / texel) * texel - cx) + up * (std::round(cy / texel) * texel - cy);
                // Include casters in front of the receiver slice, without camera-frustum culling.
                const float depthExtent = radius + 100;
                Math::Mat4 view{right[0],
                                up[0],
                                forward[0],
                                0,
                                right[1],
                                up[1],
                                forward[1],
                                0,
                                right[2],
                                up[2],
                                forward[2],
                                0,
                                -Math::Dot(center, right),
                                -Math::Dot(center, up),
                                -Math::Dot(center, forward) + depthExtent,
                                1};
                Math::Mat4 ortho{1 / radius, 0, 0, 0, 0, 1 / radius, 0, 0, 0, 0, 1 / (2 * depthExtent), 0, 0, 0, 0, 1};
                data.shadowMatrices[cascade] = view * ortho;
                begin = end;
            }
            return data;
        }
    } // namespace
    Renderer::Renderer(RHI::Device& owner, RHIRenderScene scene)
        : device(owner), pipelines(device, scene), assets(device, scene)
    {
        if (scene.gpu_driven)
        {
            cullCapacity = scene.gpu_driven->max_instances;
            for (auto& resources : cullFrames)
                resources = std::make_unique<CullResources>(device, cullCapacity);
        }
    }
    void Renderer::ReleaseSizeResources()
    {
        giPass.reset();
        size.reset();
        ResetHistory();
    }
    void Renderer::Resize(uint32_t width, uint32_t height)
    {
        size = std::make_unique<SizeResources>(device, width, height);
        if (pipelines.gi)
        {
            if (!giPass)
                giPass = std::make_unique<ReSTIRGIPass>(device);
            giPass->Resize(width, height);
        }
        ResetHistory();
    }
    void Renderer::ResetHistory() noexcept
    {
        previous.valid = pending.valid = false;
    }
    void Renderer::CommitFrame() noexcept
    {
        frames[pendingSlot].timestampsSubmitted = true;
        std::swap(previous, pending);
        pending.valid = false;
    }
    void Renderer::Render(RHI::CommandList& cmd, uint32_t slot, const RenderCore::RenderFrame& frame,
                          const RenderOutput& output)
    {
        const bool linearScene = pipelines.tone || pipelines.taa;
        const bool sceneVisible = !pipelines.ui || frame.scene_extent.HasArea();
        const bool giEnabled = pipelines.gi && assets.giTriangles && sceneVisible && frame.main_view.valid &&
                               frame.enable_gi && !frame.rendering.normal_debug && !frame.visualize_motion_vectors &&
                               frame.main_view.projection.At(2, 3) != 0;
        const bool historyCompatible =
            previous.valid && frame.main_view.valid && previous.cameraId == frame.main_view.camera_id &&
            previous.revision == frame.history_revision && previous.projection == frame.main_view.projection;
        const bool taaEnabled = sceneVisible && frame.main_view.valid && frame.enable_taa && pipelines.taa &&
                                !frame.visualize_motion_vectors;
        const bool reuseHistory =
            historyCompatible && previous.taaEnabled && taaEnabled && previous.giEnabled == giEnabled;
        pending.giEnabled = giEnabled;
        pending.giFrameIndex = previous.giFrameIndex + 1;
        const auto& sceneSize = size->color->Description();
        const std::array<float, 2> inverseSize{1.0f / sceneSize.width, 1.0f / sceneSize.height};
        pending.taaEnabled = taaEnabled;
        pending.historyIndex = reuseHistory ? 1 - previous.historyIndex : 0;
        pending.jitterIndex = reuseHistory ? (previous.jitterIndex + 1) % 8 : 0;
        const auto jitter = taaEnabled ? TemporalJitter(pending.jitterIndex) : std::array<float, 2>{};
        const std::array<float, 2> jitterUV{jitter[0] * inverseSize[0], jitter[1] * inverseSize[1]};
        const auto clipOffset = Math::TranslationMatrix({2 * jitterUV[0], -2 * jitterUV[1], 0});
        pending.valid = sceneVisible && frame.main_view.valid;
        pending.worlds.clear();
        pending.worlds.reserve(frame.render_items.size());
        pending.cameraId = frame.main_view.camera_id;
        pending.revision = frame.history_revision;
        pending.projection = frame.main_view.projection;
        auto& dynamic = frames[slot];
        dynamic.Prepare(device, frame);
        if (dynamic.timestampsSubmitted)
        {
            for (uint32_t i = 0; i < 5; ++i)
                gpuTimings.milliseconds[i] = dynamic.timestamps->Milliseconds(i, i + 1);
            gpuTimings.valid = true;
        }
        dynamic.timestampsSubmitted = false;
        pendingSlot = slot;
        cmd.WriteTimestamp(*dynamic.timestamps, 0);
        const bool shadows = sceneVisible && frame.main_view.valid && frame.rendering.shadows && pipelines.shadow &&
                             !frame.rendering.normal_debug && frame.main_view.projection.At(2, 3) != 0;
        auto lighting = MakeLighting(frame, assets, shadows);
        lighting.counts[3] = giEnabled ? 1.0f : 0.0f;
        dynamic.lighting->Write(0, &lighting, sizeof(lighting));
        for (uint32_t cascade = 0; cascade < ShadowCascades; ++cascade)
            dynamic.shadowConstants->Write(cascade * 256, &lighting.shadowMatrices[cascade], sizeof(Math::Mat4));
        const auto viewProjection = frame.main_view.view * frame.main_view.projection;
        pending.viewProjection = viewProjection;
        const FrameConstants constants{viewProjection,
                                       historyCompatible ? previous.viewProjection : viewProjection,
                                       viewProjection * clipOffset,
                                       frame.main_view.camera_world_position,
                                       {frame.rendering.light_direction[0], frame.rendering.light_direction[1],
                                        frame.rendering.light_direction[2], frame.rendering.light_intensity},
                                       {frame.rendering.ambient, frame.rendering.exposure,
                                        frame.rendering.normal_debug ? 1.0f : 0.0f, linearScene ? 0.0f : 1.0f}};
        dynamic.frame->Write(0, &constants, sizeof(constants));
        std::vector<DrawPacket> draws;
        std::vector<CullInstance> instances;
        std::vector<RHI::DrawIndexedIndirectArguments> seeds;
        draws.reserve(frame.render_items.size());
        bool hasNanite = false;
        bool debugOnly = !frame.render_items.empty();
        for (size_t i = 0; i < frame.render_items.size(); ++i)
        {
            const auto& item = frame.render_items[i];
            Math::Mat4 normal;
            if (!Math::TryMakeNormalMatrix(item.local_to_world, normal))
                throw std::runtime_error("Singular object transform");
            auto previousWorld = item.local_to_world;
            if (historyCompatible && item.object_id != 0)
            {
                const auto found = previous.worlds.find(item.object_id);
                if (found != previous.worlds.end())
                    previousWorld = found->second;
            }
            if (pending.valid && item.object_id != 0)
                pending.worlds.emplace(item.object_id, item.local_to_world);
            auto& material = assets.materials.at(item.material.value);
            auto tint = item.color_tint;
            debugOnly = debugOnly && tint[3] < 0;
            if (tint[3] >= 0)
                for (size_t channel = 0; channel < 4; ++channel)
                    tint[channel] *= material.baseColor[channel];
            const ObjectConstants object{
                item.local_to_world,
                normal,
                previousWorld,
                tint,
                {material.uvScale[0], material.uvScale[1], material.alphaCutoff, material.normalScale},
                {material.metallic, material.roughness, material.occlusionStrength, 0},
                {material.emissive[0], material.emissive[1], material.emissive[2], 0}};
            dynamic.objects->Write(i * 256, &object, sizeof(object));
            auto& texture = assets.textures.at(material.texture);
            auto node = assets.nodes.find(item.mesh.value);
            GpuMesh* mesh = nullptr;
            std::span<const DrawRange> ranges;
            const bool isNanite = node != assets.nodes.end();
            if (isNanite)
            {
                mesh = &assets.nanite[node->second.mesh].geometry;
                ranges = node->second.draws;
                hasNanite = true;
            }
            else
                mesh = &assets.meshes.at(item.mesh.value);
            draws.push_back({mesh->vertices.get(),
                             mesh->indices.get(),
                             texture.ColorView(),
                             material.sampler.get(),
                             i * 256,
                             ranges,
                             mesh->indexCount,
                             isNanite,
                             {assets.textures.at(material.normalTexture).view.get(),
                              assets.textures.at(material.metallicRoughnessTexture).view.get(),
                              assets.textures.at(material.emissiveTexture).ColorView(),
                              assets.textures.at(material.occlusionTexture).view.get()}});
            const auto center = Math::TransformPoint(mesh->center, item.local_to_world);
            float scale = 0;
            for (size_t axis = 0; axis < 3; ++axis)
            {
                const float x = item.local_to_world.At(axis, 0), y = item.local_to_world.At(axis, 1),
                            z = item.local_to_world.At(axis, 2);
                scale = (std::max)(scale, std::sqrt(x * x + y * y + z * z));
            }
            instances.push_back({{center[0], center[1], center[2]}, mesh->radius * scale});
            seeds.push_back({mesh->indexCount, 0, 0, 0, 0});
        }
        const bool useCull =
            sceneVisible && pipelines.cull && !hasNanite && !draws.empty() && draws.size() <= cullCapacity;
        if (useCull)
        {
            // Widen the unjittered side planes by half a pixel, covering every jitter sample.
            Math::Mat4 guard;
            if (taaEnabled)
            {
                guard.At(0, 0) = 1.0f / (1.0f + inverseSize[0]);
                guard.At(1, 1) = 1.0f / (1.0f + inverseSize[1]);
            }
            cullPass.Execute(cmd, {*pipelines.cull, *pipelines.cullLayout, *cullFrames[slot], instances, seeds,
                                   viewProjection * guard});
        }
        std::optional<SkyInput> sky;
        cmd.WriteTimestamp(*dynamic.timestamps, 1);
        if (shadows && !debugOnly)
            scenePass.DrawShadows(cmd, assets, pipelines, *dynamic.shadowConstants, *dynamic.objects, draws);
        cmd.WriteTimestamp(*dynamic.timestamps, 2);
        if (pipelines.sky && frame.main_view.valid)
        {
            auto view = frame.main_view.view;
            view.At(3, 0) = view.At(3, 1) = view.At(3, 2) = 0;
            pending.skyViewProjection = view * frame.main_view.projection;
            const SkyConstants skyConstants{pending.skyViewProjection,
                                            historyCompatible ? previous.skyViewProjection : pending.skyViewProjection,
                                            pending.skyViewProjection * clipOffset,
                                            assets.skyIntensity * assets.skyExposure,
                                            frame.rendering.exposure,
                                            linearScene ? 0.0f : 1.0f,
                                            frame.rendering.environment_rotation * Math::Pi / 180.0f};
            dynamic.skyConstants->Write(0, &skyConstants, sizeof(skyConstants));
            sky.emplace(SkyInput{*pipelines.sky, *pipelines.skyLayout, *dynamic.skyConstants, *assets.skyVertices,
                                 *assets.skyIndices, *assets.textures.at(assets.skyTexture).view, *assets.skySampler});
        }
        if (sceneVisible)
        {
            auto& displayColor = pipelines.ui ? *size->colorRTV : output.color;
            auto& sceneColor = linearScene ? *size->hdrRTV : displayColor;
            scenePass.Execute(cmd, {sceneColor, *size->depthDSV, *size->velocityRTV, *pipelines.scene,
                                    *pipelines.sceneLayout, *dynamic.frame, *dynamic.objects, draws,
                                    useCull ? cullFrames[slot]->arguments.get() : nullptr, &assets,
                                    dynamic.lighting.get(), giPass ? &giPass->NormalTarget() : nullptr,
                                    giPass ? &giPass->DiffuseTarget() : nullptr});
            if (sky)
                scenePass.DrawSky(cmd, sceneColor, *size->depthDSV, *size->velocityRTV, *sky);
            cmd.WriteTimestamp(*dynamic.timestamps, 3);
            RHI::TextureView* resolved = linearScene ? size->hdrSRV.get() : size->colorSRV.get();
            if (giEnabled)
                resolved = &giPass->Execute(cmd, slot, frame, assets, pipelines, *dynamic.lighting, *size->depthSRV,
                                            *resolved, jitterUV, pending.giFrameIndex);
            if (taaEnabled)
            {
                const TAAConstants taaConstants{inverseSize, jitterUV, 0.1f, uint32_t(reuseHistory)};
                dynamic.taaConstants->Write(0, &taaConstants, sizeof(taaConstants));
                const auto write = pending.historyIndex;
                ResolveTAA(cmd,
                           {*resolved, *size->velocitySRV, *size->depthSRV, *size->historySRV[1 - write],
                            *size->historyRTV[write], *pipelines.taa, *pipelines.taaLayout, *dynamic.taaConstants});
                resolved = size->historySRV[write].get();
            }
            if (pipelines.tone)
            {
                const std::array<float, 4> toneConstants{
                    frame.rendering.exposure, (frame.rendering.normal_debug || debugOnly) ? 1.0f : 0.0f, 0, 0};
                dynamic.toneConstants->Write(0, toneConstants.data(), sizeof(toneConstants));
                ToneMapScene(cmd, {*resolved, displayColor, *pipelines.tone, *pipelines.toneLayout},
                             *dynamic.toneConstants);
            }
            else if (linearScene)
                DrawSceneOutput(cmd, {*resolved, displayColor, *pipelines.display, *pipelines.displayLayout});
            if (frame.visualize_motion_vectors && pipelines.motion)
            {
                const auto& extent = displayColor.Resource().Description();
                const std::array<float, 4> motionConstants{float(extent.width), float(extent.height), 0, 0};
                dynamic.motionConstants->Write(0, motionConstants.data(), sizeof(motionConstants));
                motionPass.Execute(cmd, {displayColor, *size->velocitySRV, *pipelines.motion, *pipelines.motionLayout,
                                         *dynamic.motionConstants});
            }
            cmd.WriteTimestamp(*dynamic.timestamps, 4);
            if (pipelines.gizmoVisible && !frame.gizmo.IsEmpty())
            {
                dynamic.gizmoVertices->Write(0, frame.gizmo.vertices.data(),
                                             frame.gizmo.vertices.size() * sizeof(RenderCore::GizmoVertex));
                dynamic.gizmoIndices->Write(0, frame.gizmo.indices.data(),
                                            frame.gizmo.indices.size() * sizeof(uint32_t));
                editorDebugPass.Execute(cmd, {displayColor, *size->depthDSV, *pipelines.gizmoVisible,
                                              *pipelines.gizmoOccluded, *pipelines.gizmoLayout, *dynamic.gizmoVertices,
                                              *dynamic.gizmoIndices, uint32_t(frame.gizmo.indices.size())});
            }
        }
        else
        {
            cmd.WriteTimestamp(*dynamic.timestamps, 3);
            cmd.WriteTimestamp(*dynamic.timestamps, 4);
        }
        if (pipelines.ui)
        {
            if (!frame.ui.IsEmpty())
            {
                dynamic.uiVertices->Write(0, frame.ui.vertices.data(),
                                          frame.ui.vertices.size() * sizeof(RenderCore::UIVertex));
                dynamic.uiIndices->Write(0, frame.ui.indices.data(), frame.ui.indices.size() * sizeof(uint32_t));
                const auto& ui = frame.ui;
                const UIConstants uiConstants{{2 / ui.display_size[0], -2 / ui.display_size[1]},
                                              {-1 - ui.display_position[0] * 2 / ui.display_size[0],
                                               1 + ui.display_position[1] * 2 / ui.display_size[1]}};
                dynamic.uiConstants->Write(0, &uiConstants, sizeof(uiConstants));
            }
            uiPass.Execute(cmd, {output.color, *size->colorSRV, *assets.textures.at(assets.fontTexture).view,
                                 *assets.uiSampler, *pipelines.ui, *pipelines.uiLayout, *dynamic.uiConstants,
                                 *dynamic.uiVertices, *dynamic.uiIndices, frame.ui});
        }
        cmd.Transition(output.color.Resource(), output.finalState);
        cmd.WriteTimestamp(*dynamic.timestamps, 5);
        cmd.ResolveTimestamps(*dynamic.timestamps, 6);
    }
} // namespace GE::Render
