#pragma once

#include "Renderer.h"

#include <Engine/RHI/RHIBackend.h>
#include <Engine/RHI/RHIFormat.h>
#include <Engine/RHI/RHISampler.h>
#include <Engine/RHI/RHIShader.h>
#include <Engine/RHI/RHISurface.h>
#include <Engine/RenderCore/RenderHandles.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace GE::Render
{
    // M2 的最小通用顶点契约；它属于 Renderer，不泄漏任何 D3D12 原生类型。
    struct PositionColorTextureVertex final
    {
        std::array<float, 3> position{};
        std::array<float, 3> color{};
        std::array<float, 2> texture_coordinate{};
    };

    // 启动期 Mesh 数据；Renderer 上传后独占对应 GPU Buffer。
    struct RHIMeshDescription final
    {
        RenderCore::MeshHandle handle;
        std::vector<PositionColorTextureVertex> vertices;
        std::vector<std::uint16_t> indices;
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
    };

    struct RHIUICompositionDescription final
    {
        RenderCore::TextureHandle font_atlas_texture;
        RHI::SamplerDescription sampler;
        RHI::ShaderBytecodeFormat shader_format =
            RHI::ShaderBytecodeFormat::D3DBytecode;
        std::vector<std::byte> vertex_shader;
        std::vector<std::byte> pixel_shader;
    };

    struct RHIGizmoCompositionDescription final
    {
        RHI::ShaderBytecodeFormat shader_format =
            RHI::ShaderBytecodeFormat::D3DBytecode;
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

        RHI::ShaderBytecodeFormat shader_format =
            RHI::ShaderBytecodeFormat::D3DBytecode;
        std::vector<std::byte> vertex_shader;
        std::vector<std::byte> pixel_shader;
    };

    // 示例层提交一次性的纯值资源。Renderer 会取得这些容器的所有权。
    struct RHIRenderScene final
    {
        std::vector<RHIMeshDescription> meshes;
        std::vector<RHITextureDescription> textures;
        std::vector<RHIMaterialDescription> materials;

        RHI::ShaderBytecodeFormat shader_format =
            RHI::ShaderBytecodeFormat::D3DBytecode;
        std::vector<std::byte> vertex_shader;
        std::vector<std::byte> pixel_shader;

        // 存在时启用“场景离屏 + UI 最终合成”；具体 UI 框架不进入 Renderer。
        std::optional<RHIUICompositionDescription> ui_composition;

        // 存在时允许消费 RenderFrame 中的编辑器 Gizmo 值包。
        std::optional<RHIGizmoCompositionDescription> gizmo_composition;

        // 静态天空环境；首轮只作为背景显示，不参与 IBL。
        std::optional<RHISkyboxDescription> skybox;
    };

    struct RHIRendererCreateInfo final
    {
        // Renderer 独占 Backend，并经它创建且独占 Device 和全部 GPU 资源。
        std::unique_ptr<RHI::Backend> backend;

        // Surface 只是窗口句柄的借用值；窗口必须比 Renderer 活得更久。
        RHI::NativeWindowSurface surface;
        RenderCore::RenderExtent initial_extent;
        RHIRenderScene scene;
    };

    // 通用 RHI 渲染器：未来 Vulkan 注入 Vulkan Backend 和 SPIR-V 即可复用此层。
    class RHIRenderer final : public Renderer
    {
    public:
        explicit RHIRenderer(RHIRendererCreateInfo create_info);
        ~RHIRenderer() noexcept override;

        RHIRenderer(const RHIRenderer&) = delete;
        RHIRenderer& operator=(const RHIRenderer&) = delete;
        RHIRenderer(RHIRenderer&&) = delete;
        RHIRenderer& operator=(RHIRenderer&&) = delete;

        [[nodiscard]] bool Initialize() override;
        [[nodiscard]] bool SubmitFrame(
            RenderCore::RenderFrame frame) override;
        void Shutdown() noexcept override;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
