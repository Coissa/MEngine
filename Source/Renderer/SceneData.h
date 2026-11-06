#pragma once

#include "Nanite/PackCluster.h"
#include "RenderData.h"

#include "RHI/RHIResource.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace GE::Render
{
    // M2 的最小通用顶点契约；它属于 Renderer，不泄漏任何 D3D12 原生类型。
    struct PositionNormalColorTextureVertex final
    {
        std::array<float, 3> position{};
        std::array<float, 3> normal{0.0F, 1.0F, 0.0F};
        std::array<float, 3> color{};
        std::array<float, 2> texture_coordinate{};
    };

    static_assert(sizeof(PositionNormalColorTextureVertex) == sizeof(float) * 11);
    static_assert(offsetof(PositionNormalColorTextureVertex, position) == 0);
    static_assert(offsetof(PositionNormalColorTextureVertex, normal) == sizeof(float) * 3);
    static_assert(offsetof(PositionNormalColorTextureVertex, color) == sizeof(float) * 6);
    static_assert(offsetof(PositionNormalColorTextureVertex, texture_coordinate) == sizeof(float) * 9);

    // 启动期 Mesh 数据；Renderer 上传后独占对应 GPU Buffer。
    struct RHIMeshDescription final
    {
        RenderCore::MeshHandle handle;
        std::vector<PositionNormalColorTextureVertex> vertices;
        std::vector<std::uint16_t> indices;
    };

    // Each source node uses first_mesh_handle + its position in data.nodes.
    struct RHINaniteMeshDescription final
    {
        std::uint32_t first_mesh_handle = 0;
        Nanite::NaniteMeshData data;
    };

    // 启动期纹理数据按 array layer、再按 mip 的顺序紧密排列；每个子资源无行填充。
    struct RHITextureDescription final
    {
        RenderCore::TextureHandle handle;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t array_layers = 1;
        std::uint32_t mip_levels = 1;
        RHI::RHIFormat format = RHI::RHIFormat::Unknown;
        RHI::SampledTextureViewDescription view;
        std::vector<std::byte> pixels;
    };

    // Material 只引用纹理句柄并保存采样规则，不拥有 Texture。
    struct RHIMaterialDescription final
    {
        RenderCore::MaterialHandle handle;
        RenderCore::TextureHandle albedo_texture;
        RHI::SamplerDescription sampler;
        std::array<float, 4> base_color{1, 1, 1, 1};
        std::array<float, 2> uv_scale{1, 1};
        float alpha_cutoff = 0;
        float metallic = 0, roughness = 1;
        std::array<float, 3> emissive{};
        float normal_scale = 1, occlusion_strength = 1;
        RenderCore::TextureHandle normal_texture, metallic_roughness_texture, emissive_texture, occlusion_texture;
    };

    struct RHIUICompositionDescription final
    {
        RenderCore::TextureHandle font_atlas_texture;
        RHI::SamplerDescription sampler;
        std::vector<std::byte> vertex_shader;
        std::vector<std::byte> pixel_shader;
    };

    struct RHIGizmoCompositionDescription final
    {
        std::vector<std::byte> vertex_shader;
        std::vector<std::byte> visible_pixel_shader;
        std::vector<std::byte> occluded_pixel_shader;
    };

    struct RHISkyboxDescription final
    {
        RenderCore::TextureHandle cubemap_texture;
        RHI::SamplerDescription sampler;
        float intensity = 1.0F;
        float exposure = 1.0F;
        std::vector<std::byte> vertex_shader;
        std::vector<std::byte> pixel_shader;
    };

    struct RHIGpuDrivenDescription final
    {
        std::vector<std::byte> culling_compute_shader;
        std::uint32_t max_instances = 4096;
    };

    struct RHIMotionVectorVisualizationDescription final
    {
        std::vector<std::byte> vertex_shader;
        std::vector<std::byte> pixel_shader;
    };

    struct RHIPostProcessDescription final
    {
        std::vector<std::byte> vertex_shader, taa_pixel_shader, display_pixel_shader, tone_map_pixel_shader;
    };

    struct RHILightingDescription
    {
        std::vector<std::byte> irradiance_shader, prefilter_shader, brdf_shader;
        std::vector<std::byte> shadow_vertex_shader, shadow_pixel_shader;
    };

    // 示例层提交一次性的纯值资源。Renderer 会取得这些容器的所有权。
    struct RHIRenderScene final
    {
        std::vector<RHIMeshDescription> meshes;
        std::vector<RHINaniteMeshDescription> nanite_meshes;
        std::vector<RHITextureDescription> textures;
        std::vector<RHIMaterialDescription> materials;
        std::vector<std::byte> vertex_shader;
        std::vector<std::byte> pixel_shader;

        // 存在时启用“场景离屏 + UI 最终合成”；具体 UI 框架不进入 Renderer。
        std::optional<RHIUICompositionDescription> ui_composition;

        // 存在时允许消费 RenderFrame 中的编辑器 Gizmo 值包。
        std::optional<RHIGizmoCompositionDescription> gizmo_composition;

        // 静态天空环境；首轮只作为背景显示，不参与 IBL。
        std::optional<RHISkyboxDescription> skybox;

        // 提供时启用 Compute Frustum Culling + ExecuteIndirect；省略则保留传统路径。
        std::optional<RHIGpuDrivenDescription> gpu_driven;
        std::optional<RHIMotionVectorVisualizationDescription> motion_vector_visualization;
        std::optional<RHIPostProcessDescription> post_process;
        std::optional<RHILightingDescription> lighting;
        std::vector<std::byte> gi_compute_shader;
        std::vector<std::byte> gi_pixel_shader;
    };

} // namespace GE::Render
