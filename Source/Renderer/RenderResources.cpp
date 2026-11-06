#include "RenderResources.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace GE::Render
{
    std::unique_ptr<RHI::Buffer> UploadBuffer(RHI::Device& device, const void* data, size_t bytes)
    {
        auto buffer = device.CreateBuffer({bytes, RHI::Memory::GPU});
        device.UploadBuffer(*buffer, data, bytes);
        return buffer;
    }
    namespace
    {
        GpuMesh UploadMesh(RHI::Device& device, const std::vector<PositionNormalColorTextureVertex>& vertices,
                           const std::vector<uint16_t>& indices)
        {
            GpuMesh result;
            result.vertices = UploadBuffer(device, vertices.data(), vertices.size() * sizeof(vertices[0]));
            result.indices = UploadBuffer(device, indices.data(), indices.size() * sizeof(indices[0]));
            result.indexCount = uint32_t(indices.size());
            Math::Vec3 lo{vertices[0].position[0], vertices[0].position[1], vertices[0].position[2]}, hi = lo;
            for (const auto& v : vertices)
                for (int axis = 0; axis < 3; ++axis)
                {
                    lo[axis] = (std::min)(lo[axis], v.position[axis]);
                    hi[axis] = (std::max)(hi[axis], v.position[axis]);
                }
            result.center = (lo + hi) * 0.5f;
            const auto extent = hi - result.center;
            result.radius = std::sqrt(Math::Dot(extent, extent));
            return result;
        }
        void EnsureUpload(RHI::Device& device, std::unique_ptr<RHI::Buffer>& buffer, size_t bytes)
        {
            bytes = (std::max)(bytes, size_t(256));
            if (!buffer || buffer->Description().bytes < bytes)
                buffer = device.CreateBuffer({bytes, RHI::Memory::Upload});
        }

        GpuTexture EnvironmentTexture(RHI::Device& device, uint32_t width, uint32_t layers, uint32_t mips)
        {
            GpuTexture result;
            result.texture = device.CreateTexture(
                {width, width, RHI::RHIFormat::R32G32B32A32_Float, false, false, layers, mips, false, true});
            RHI::SampledTextureViewDescription view;
            if (layers == 6)
                view.dimension = RHI::SampledTextureViewDimension::TextureCube;
            result.view = device.CreateView(*result.texture, RHI::ViewKind::ShaderResource, view);
            return result;
        }

        void PrepareEnvironment(RHI::Device& device, RHIRenderScene& scene, SceneAssets& assets)
        {
            assets.environmentSampler =
                device.CreateSampler({RHI::SamplerFilter::Linear, RHI::SamplerAddressMode::ClampToEdge,
                                      RHI::SamplerAddressMode::ClampToEdge, RHI::SamplerAddressMode::ClampToEdge});
            assets.hasEnvironment = scene.lighting.has_value() && scene.skybox.has_value();
            assets.irradiance = EnvironmentTexture(device, assets.hasEnvironment ? 16 : 1, 6, 1);
            assets.prefilter =
                EnvironmentTexture(device, assets.hasEnvironment ? 64 : 1, 6, assets.hasEnvironment ? 7 : 1);
            assets.brdf = EnvironmentTexture(device, assets.hasEnvironment ? 64 : 1, 1, 1);
            if (!assets.hasEnvironment)
            {
                const float black[4]{};
                for (auto* image : {&assets.irradiance, &assets.prefilter, &assets.brdf})
                    for (uint32_t layer = 0; layer < image->texture->Description().arrayLayers; ++layer)
                        device.UploadTexture(*image->texture, black, sizeof(black), 0, layer);
                return;
            }
            using namespace RHI;
            auto layout = device.CreateBindingLayout({{{BindingKind::ConstantBuffer, 0},
                                                       {BindingKind::Texture, 0},
                                                       {BindingKind::Sampler, 0},
                                                       {BindingKind::StorageTexture, 0},
                                                       {BindingKind::StorageTexture, 1}}});
            ShaderBytecode irradiance{scene.lighting->irradiance_shader}, prefilter{scene.lighting->prefilter_shader},
                brdf{scene.lighting->brdf_shader};
            auto diffusePipeline = device.CreateComputePipeline({layout.get(), &irradiance});
            auto specularPipeline = device.CreateComputePipeline({layout.get(), &prefilter});
            auto brdfPipeline = device.CreateComputePipeline({layout.get(), &brdf});
            auto constants = device.CreateBuffer({9 * 256, Memory::Upload});
            struct FilterConstants
            {
                uint32_t size;
                float roughness;
                uint32_t samples, padding = 0;
            };
            const FilterConstants diffuseConstants{16, 0, 128}, brdfConstants{64, 0, 256};
            constants->Write(0, &diffuseConstants, sizeof(diffuseConstants));
            constants->Write(8 * 256, &brdfConstants, sizeof(brdfConstants));
            auto diffuseUAV = device.CreateView(*assets.irradiance.texture, ViewKind::Storage);
            auto brdfUAV = device.CreateView(*assets.brdf.texture, ViewKind::Storage);
            std::array<std::unique_ptr<TextureView>, 7> specularUAV;
            for (uint32_t mip = 0; mip < 7; ++mip)
            {
                SampledTextureViewDescription view;
                view.base_mip_level = mip;
                specularUAV[mip] = device.CreateView(*assets.prefilter.texture, ViewKind::Storage, view);
                const FilterConstants settings{64u >> mip, float(mip) / 6, 128};
                constants->Write((mip + 1) * 256, &settings, sizeof(settings));
            }
            device.GetCommandContext().ExecuteImmediate(
                [&](CommandList& cmd)
                {
                    auto& source = assets.textures.at(scene.skybox->cubemap_texture.value);
                    cmd.Transition(*source.texture, State::ShaderResource);
                    auto bind = [&](ComputePipeline& pipeline, size_t offset)
                    {
                        cmd.SetComputePipeline(pipeline);
                        cmd.SetBindingLayout(*layout);
                        cmd.BindConstantBuffer(0, *constants, offset);
                        cmd.BindTexture(1, *source.view);
                        cmd.BindSampler(2, *assets.environmentSampler);
                    };
                    cmd.Transition(*assets.irradiance.texture, State::Storage);
                    bind(*diffusePipeline, 0);
                    cmd.BindTexture(3, *diffuseUAV);
                    cmd.Dispatch(2, 2, 6);
                    cmd.Transition(*assets.irradiance.texture, State::ShaderResource);
                    for (uint32_t mip = 0; mip < 7; ++mip)
                    {
                        for (uint32_t face = 0; face < 6; ++face)
                            cmd.Transition(*assets.prefilter.texture, State::Storage, mip + face * 7);
                        bind(*specularPipeline, (mip + 1) * 256);
                        cmd.BindTexture(3, *specularUAV[mip]);
                        cmd.Dispatch((std::max)(1u, (64u >> mip) / 8), (std::max)(1u, (64u >> mip) / 8), 6);
                    }
                    cmd.Transition(*assets.prefilter.texture, State::ShaderResource);
                    cmd.Transition(*assets.brdf.texture, State::Storage);
                    bind(*brdfPipeline, 8 * 256);
                    cmd.BindTexture(4, *brdfUAV);
                    cmd.Dispatch(8, 8);
                    cmd.Transition(*assets.brdf.texture, State::ShaderResource);
                });
        }
    } // namespace
    SceneAssets::SceneAssets(RHI::Device& device, RHIRenderScene& scene)
    {
        for (auto& mesh : scene.meshes)
            meshes.emplace(mesh.handle.value, UploadMesh(device, mesh.vertices, mesh.indices));
        for (auto& source : scene.nanite_meshes)
        {
            std::vector<PositionNormalColorTextureVertex> vertices;
            for (const auto& v : source.data.vertices)
                vertices.push_back({{v.position[0], v.position[1], v.position[2]},
                                    {v.normal[0], v.normal[1], v.normal[2]},
                                    {1, 1, 1},
                                    {v.texcoord[0], v.texcoord[1]}});
            uint32_t leaf = 0;
            for (const auto& c : source.data.clusters)
                if (c.generating_group_index == UINT32_MAX)
                {
                    for (uint32_t i = 0; i < c.vertex_count; ++i)
                        vertices[c.vertex_begin + i].color = {float(leaf), 0, 0};
                    ++leaf;
                }
            const auto meshIndex = nanite.size();
            nanite.push_back({UploadMesh(device, vertices, source.data.indices), std::move(source.data)});
            const auto& data = nanite.back().data;
            for (size_t i = 0; i < data.nodes.size(); ++i)
            {
                NaniteNode node;
                node.mesh = meshIndex;
                for (const auto& c : data.clusters)
                    if (c.node_index == data.nodes[i].node_index && c.generating_group_index == UINT32_MAX)
                        node.draws.push_back({c.index_count, c.index_begin, int32_t(c.vertex_begin)});
                nodes.emplace(source.first_mesh_handle + uint32_t(i), std::move(node));
            }
        }
        // Missing material textures use white; the shader disables normal mapping without a normal texture.
        RHITextureDescription white;
        white.handle = {0};
        white.width = white.height = 1;
        white.format = RHI::RHIFormat::R8G8B8A8_UNorm;
        white.pixels = {std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
        scene.textures.push_back(std::move(white));
        for (const auto& source : scene.textures)
        {
            GpuTexture texture;
            texture.texture =
                device.CreateTexture({source.width, source.height, source.format, false, false, source.array_layers,
                                      source.mip_levels, source.format == RHI::RHIFormat::R8G8B8A8_UNorm});
            size_t offset = 0;
            const size_t texelBytes = source.format == RHI::RHIFormat::R32G32B32A32_Float ? 16 : 4;
            for (uint32_t layer = 0; layer < source.array_layers; ++layer)
                for (uint32_t mip = 0; mip < source.mip_levels; ++mip)
                {
                    uint32_t width = (std::max)(1u, source.width >> mip), height = (std::max)(1u, source.height >> mip);
                    device.UploadTexture(*texture.texture, source.pixels.data() + offset, uint32_t(width * texelBytes),
                                         mip, layer);
                    offset += width * height * texelBytes;
                }
            texture.view = device.CreateView(*texture.texture, RHI::ViewKind::ShaderResource, source.view);
            if (source.format == RHI::RHIFormat::R8G8B8A8_UNorm)
            {
                auto colorView = source.view;
                colorView.srgb = true;
                texture.colorView = device.CreateView(*texture.texture, RHI::ViewKind::ShaderResource, colorView);
            }
            textures.emplace(source.handle.value, std::move(texture));
        }
        for (const auto& source : scene.materials)
        {
            GpuMaterial material;
            material.texture = source.albedo_texture.value;
            material.sampler = device.CreateSampler(source.sampler);
            material.baseColor = source.base_color;
            material.uvScale = source.uv_scale;
            material.alphaCutoff = source.alpha_cutoff;
            material.metallic = source.metallic;
            material.roughness = source.roughness;
            material.emissive = source.emissive;
            material.normalScale = source.normal_texture.IsValid() ? source.normal_scale : 0;
            material.occlusionStrength = source.occlusion_strength;
            material.normalTexture = source.normal_texture.value;
            material.metallicRoughnessTexture = source.metallic_roughness_texture.value;
            material.emissiveTexture = source.emissive_texture.value;
            material.occlusionTexture = source.occlusion_texture.value;
            materials.emplace(source.handle.value, std::move(material));
        }
        if (scene.ui_composition)
        {
            fontTexture = scene.ui_composition->font_atlas_texture.value;
            uiSampler = device.CreateSampler(scene.ui_composition->sampler);
        }
        if (scene.skybox)
        {
            skyTexture = scene.skybox->cubemap_texture.value;
            skyIntensity = scene.skybox->intensity;
            skyExposure = scene.skybox->exposure;
            skySampler = device.CreateSampler(scene.skybox->sampler);
            constexpr float vertices[][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
                                             {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
            constexpr uint16_t indices[] = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 4, 7, 0, 7, 3,
                                            1, 2, 6, 1, 6, 5, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2};
            skyVertices = UploadBuffer(device, vertices, sizeof(vertices));
            skyIndices = UploadBuffer(device, indices, sizeof(indices));
        }
        PrepareEnvironment(device, scene, *this);
        const uint32_t resolution = scene.lighting ? ShadowResolution : 1;
        shadowMap =
            device.CreateTexture({resolution, resolution, RHI::RHIFormat::D32_Float, false, true, ShadowCascades});
        shadowSRV = device.CreateView(*shadowMap, RHI::ViewKind::ShaderResource);
        for (uint32_t cascade = 0; cascade < ShadowCascades; ++cascade)
        {
            RHI::SampledTextureViewDescription view;
            view.base_array_layer = cascade;
            view.array_layer_count = 1;
            shadowDSV[cascade] = device.CreateView(*shadowMap, RHI::ViewKind::DepthStencil, view);
        }
    }
    SizeResources::SizeResources(RHI::Device& device, uint32_t width, uint32_t height)
    {
        hdr = device.CreateTexture({width, height, RHI::RHIFormat::R16G16B16A16_Float, true});
        hdrRTV = device.CreateView(*hdr, RHI::ViewKind::RenderTarget);
        hdrSRV = device.CreateView(*hdr, RHI::ViewKind::ShaderResource);
        color = device.CreateTexture({width, height, RHI::RHIFormat::B8G8R8A8_UNorm, true});
        depth = device.CreateTexture({width, height, RHI::RHIFormat::D32_Float, false, true});
        velocity = device.CreateTexture({width, height, RHI::RHIFormat::R16G16_Float, true});
        colorRTV = device.CreateView(*color, RHI::ViewKind::RenderTarget);
        colorSRV = device.CreateView(*color, RHI::ViewKind::ShaderResource);
        depthDSV = device.CreateView(*depth, RHI::ViewKind::DepthStencil);
        velocityRTV = device.CreateView(*velocity, RHI::ViewKind::RenderTarget);
        velocitySRV = device.CreateView(*velocity, RHI::ViewKind::ShaderResource);
        depthSRV = device.CreateView(*depth, RHI::ViewKind::ShaderResource);
        for (size_t i = 0; i < history.size(); ++i)
        {
            history[i] = device.CreateTexture({width, height, RHI::RHIFormat::R16G16B16A16_Float, true});
            historyRTV[i] = device.CreateView(*history[i], RHI::ViewKind::RenderTarget);
            historySRV[i] = device.CreateView(*history[i], RHI::ViewKind::ShaderResource);
        }
    }
    void DynamicFrameResources::Prepare(RHI::Device& device, const RenderCore::RenderFrame& data)
    {
        EnsureUpload(device, frame, 256);
        EnsureUpload(device, objects, data.render_items.size() * 256);
        EnsureUpload(device, uiConstants, 256);
        EnsureUpload(device, skyConstants, 256);
        EnsureUpload(device, motionConstants, 256);
        EnsureUpload(device, taaConstants, 256);
        EnsureUpload(device, toneConstants, 256);
        EnsureUpload(device, lighting, sizeof(LightingConstants));
        EnsureUpload(device, shadowConstants, ShadowCascades * 256);
        if (!timestamps)
            timestamps = device.CreateTimestampQuery(6);
        EnsureUpload(device, uiVertices, data.ui.vertices.size() * sizeof(RenderCore::UIVertex));
        EnsureUpload(device, uiIndices, data.ui.indices.size() * sizeof(uint32_t));
        EnsureUpload(device, gizmoVertices, data.gizmo.vertices.size() * sizeof(RenderCore::GizmoVertex));
        EnsureUpload(device, gizmoIndices, data.gizmo.indices.size() * sizeof(uint32_t));
    }
} // namespace GE::Render
