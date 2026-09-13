#include "RHIRenderer.h"
#include "Private/ResourceStateTracker.h"

#include <Engine/Core/Math/matrix4.h>
#include <Engine/RHI/RHIBuffer.h>
#include <Engine/RHI/RHICommand.h>
#include <Engine/RHI/RHIDevice.h>
#include <Engine/RHI/RHIFence.h>
#include <Engine/RHI/RHIPipeline.h>
#include <Engine/RHI/RHIQueue.h>
#include <Engine/RHI/RHISwapchain.h>
#include <Engine/RHI/RHITexture.h>
#include <Engine/RHI/RHITextureView.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace GE::Render
{
    namespace
    {
        constexpr RHI::RHIFormat ColorFormat =
            RHI::RHIFormat::B8G8R8A8_UNorm;
        constexpr RHI::RHIFormat DepthFormat = RHI::RHIFormat::D32_Float;
        constexpr std::uint32_t GpuWaitTimeoutMilliseconds = 5'000;

        using Matrix4 = RenderCore::RenderMatrix4;

        struct ObjectConstants final
        {
            Matrix4 world_view_projection;
            std::array<float, 4> color_tint{};
        };

        static_assert(sizeof(Matrix4) == sizeof(float) * 16);
        static_assert(sizeof(ObjectConstants) == sizeof(float) * 20);
        static_assert(sizeof(RenderCore::GizmoVertex) == sizeof(float) * 5);
        static_assert(offsetof(RenderCore::GizmoVertex, color)
            == sizeof(float) * 4);

        struct UIConstants final
        {
            std::array<float, 2> scale{};
            std::array<float, 2> translate{};
        };

        struct SkyboxConstants final
        {
            Matrix4 view_projection;
            float intensity = 1.0F;
            float exposure = 1.0F;
            std::array<float, 2> padding{};
        };

        struct SkyboxVertex final
        {
            std::array<float, 3> position{};
        };

        [[nodiscard]] constexpr std::uint32_t BytesPerTexel(
            RHI::RHIFormat format) noexcept
        {
            switch (format)
            {
            case RHI::RHIFormat::R8G8B8A8_UNorm:
            case RHI::RHIFormat::B8G8R8A8_UNorm:
                return 4;
            case RHI::RHIFormat::R32G32B32A32_Float:
                return 16;
            default:
                return 0;
            }
        }

        [[nodiscard]] bool TryGetTightTextureDataSize(
            const RHITextureDescription& texture,
            std::uint64_t& out_size) noexcept
        {
            out_size = 0;
            const std::uint32_t bytes_per_texel = BytesPerTexel(texture.format);
            if (texture.width == 0 || texture.height == 0
                || texture.array_layers == 0 || texture.mip_levels == 0
                || bytes_per_texel == 0)
            {
                return false;
            }
            for (std::uint32_t layer = 0; layer < texture.array_layers; ++layer)
            {
                for (std::uint32_t mip = 0; mip < texture.mip_levels; ++mip)
                {
                    const std::uint64_t width = mip < 32
                        ? (std::max)(1u, texture.width >> mip) : 1u;
                    const std::uint64_t height = mip < 32
                        ? (std::max)(1u, texture.height >> mip) : 1u;
                    if (width > (std::numeric_limits<std::uint64_t>::max)()
                            / height / bytes_per_texel)
                    {
                        return false;
                    }
                    const std::uint64_t subresource_size =
                        width * height * bytes_per_texel;
                    if (subresource_size
                        > (std::numeric_limits<std::uint64_t>::max)() - out_size)
                    {
                        return false;
                    }
                    out_size += subresource_size;
                }
            }
            return true;
        }

        [[nodiscard]] std::uint64_t AlignUp(
            std::uint64_t value,
            std::uint64_t alignment) noexcept
        {
            return alignment == 0
                ? value
                : (value + alignment - 1u) / alignment * alignment;
        }

        [[nodiscard]] bool Check(
            RHI::RHIResult result,
            const char* operation) noexcept
        {
            if (result == RHI::RHIResult::Success)
            {
                return true;
            }
            std::fprintf(
                stderr,
                "[Renderer] %s failed, RHIResult=%u\n",
                operation,
                static_cast<unsigned int>(result));
            return false;
        }
    }

    class RHIRenderer::Impl final
    {
    public:
        explicit Impl(RHIRendererCreateInfo create_info) noexcept
            : backend(std::move(create_info.backend)),
              surface(create_info.surface),
              extent(create_info.initial_extent),
              scene(std::move(create_info.scene))
        {
            if (scene.ui_composition.has_value())
            {
                ui_enabled = true;
                ui_font_texture = scene.ui_composition->font_atlas_texture;
            }
            gizmo_enabled = scene.gizmo_composition.has_value();
            skybox_enabled = scene.skybox.has_value();
            if (scene.skybox.has_value())
            {
                skybox_texture = scene.skybox->cubemap_texture;
                skybox_intensity = scene.skybox->intensity;
                skybox_exposure = scene.skybox->exposure;
            }
        }

        enum class State
        {
            Created,
            Initialized,
            Failed,
            Shutdown
        };

        [[nodiscard]] bool Initialize();
        [[nodiscard]] bool SubmitFrame(RenderCore::RenderFrame frame);
        void Shutdown() noexcept;

    private:
        struct FrameResource final
        {
            // 每个在途帧独占 CPU 可写常量和命令录制内存。
            std::unique_ptr<RHI::Buffer> constant_buffer;
            std::unique_ptr<RHI::Buffer> ui_constant_buffer;
            std::unique_ptr<RHI::Buffer> ui_vertex_buffer;
            std::unique_ptr<RHI::Buffer> ui_index_buffer;
            std::unique_ptr<RHI::Buffer> gizmo_vertex_buffer;
            std::unique_ptr<RHI::Buffer> gizmo_index_buffer;
            std::unique_ptr<RHI::Buffer> skybox_constant_buffer;
            std::unique_ptr<RHI::ResourceSet> skybox_resource_set;
            // ResourceSet 借用常量槽、纹理 View 和 Sampler，至少活到该帧 Fence 完成。
            std::vector<std::unique_ptr<RHI::ResourceSet>> resource_sets;
            std::vector<std::unique_ptr<RHI::ResourceSet>> ui_resource_sets;
            std::unique_ptr<RHI::CommandAllocator> command_allocator;
            std::unique_ptr<RHI::CommandList> command_list;
            std::uint64_t fence_value = 0;
        };

        struct GpuMesh final
        {
            RenderCore::MeshHandle handle;
            std::unique_ptr<RHI::Buffer> vertex_buffer;
            std::unique_ptr<RHI::Buffer> index_buffer;
            std::uint32_t index_count = 0;
        };

        struct GpuTexture final
        {
            RenderCore::TextureHandle handle;
            // View 后声明，因此 vector 销毁元素时会先销毁 View，再销毁 Texture。
            std::unique_ptr<RHI::Texture> texture;
            std::unique_ptr<RHI::SampledTextureView> view;
        };

        struct GpuMaterial final
        {
            RenderCore::MaterialHandle handle;
            RenderCore::TextureHandle albedo_texture;
            // Material 独占采样规则；纹理及其 View 由 GpuTexture 独占。
            std::unique_ptr<RHI::Sampler> sampler;
        };

        static constexpr std::size_t FrameResourceCount = 2;
        static constexpr std::size_t InitialObjectCapacity = 16;
        static constexpr std::size_t InitialUIVertexCapacity = 8'192;
        static constexpr std::size_t InitialUIIndexCapacity = 16'384;
        static constexpr std::size_t InitialGizmoVertexCapacity = 128;
        static constexpr std::size_t InitialGizmoIndexCapacity = 384;

        [[nodiscard]] bool CreateAttachments();
        [[nodiscard]] bool CreateSceneAttachments(
            RenderCore::RenderExtent new_extent);
        [[nodiscard]] bool ValidateSceneDescription() const noexcept;
        [[nodiscard]] bool CreateSceneResources();
        [[nodiscard]] bool CreateUIResources();
        [[nodiscard]] bool CreateGizmoResources();
        [[nodiscard]] bool CreateSkyboxResources();
        [[nodiscard]] bool CreateFrameResources();
        [[nodiscard]] bool CreateObjectConstantBuffer(
            FrameResource& frame_resource,
            std::size_t frame_index,
            std::size_t object_capacity);
        [[nodiscard]] bool CreateSkyboxFrameResources(
            FrameResource& frame_resource,
            std::size_t frame_index);
        [[nodiscard]] bool EnsureObjectCapacity(std::size_t required_capacity);
        [[nodiscard]] bool EnsureUICapacity(
            std::size_t required_vertices,
            std::size_t required_indices);
        [[nodiscard]] bool CreateUIBuffers(
            FrameResource& frame_resource,
            std::size_t frame_index,
            std::size_t vertex_capacity,
            std::size_t index_capacity);
        [[nodiscard]] bool EnsureGizmoCapacity(
            std::size_t required_vertices,
            std::size_t required_indices);
        [[nodiscard]] bool CreateGizmoBuffers(
            FrameResource& frame_resource,
            std::size_t frame_index,
            std::size_t vertex_capacity,
            std::size_t index_capacity);
        [[nodiscard]] bool Resize(RenderCore::RenderExtent new_extent);
        [[nodiscard]] bool WaitForFrameResource(
            FrameResource& frame_resource) noexcept;
        [[nodiscard]] bool WaitForAllFrameResources() noexcept;
        [[nodiscard]] bool TransitionTrackedBuffer(
            RHI::CommandList& command_list,
            RHI::Buffer& buffer,
            RHI::BufferState after,
            const char* operation) noexcept;
        [[nodiscard]] bool TransitionTrackedTexture(
            RHI::CommandList& command_list,
            RHI::Texture& texture,
            RHI::TextureState after,
            const char* operation) noexcept;
        [[nodiscard]] GpuMesh* FindMesh(
            RenderCore::MeshHandle handle) noexcept;
        [[nodiscard]] GpuTexture* FindTexture(
            RenderCore::TextureHandle handle) noexcept;
        [[nodiscard]] GpuMaterial* FindMaterial(
            RenderCore::MaterialHandle handle) noexcept;
        void ReleaseResources() noexcept;

        State state = State::Created;
        std::unique_ptr<RHI::Backend> backend;
        RHI::NativeWindowSurface surface;
        RenderCore::RenderExtent extent;
        RHIRenderScene scene;
        std::uint64_t object_constant_stride = 0;
        std::size_t object_capacity = 0;
        std::size_t ui_vertex_capacity = 0;
        std::size_t ui_index_capacity = 0;
        std::size_t gizmo_vertex_capacity = 0;
        std::size_t gizmo_index_capacity = 0;
        bool ui_enabled = false;
        bool gizmo_enabled = false;
        bool skybox_enabled = false;
        RenderCore::TextureHandle ui_font_texture;
        RenderCore::TextureHandle skybox_texture;
        float skybox_intensity = 1.0F;
        float skybox_exposure = 1.0F;
        RenderCore::RenderExtent scene_extent;

        std::unique_ptr<RHI::Device> device;
        std::unique_ptr<RHI::Swapchain> swapchain;
        std::unique_ptr<RHI::Fence> fence;
        std::uint64_t last_signaled_fence_value = 0;
        std::array<FrameResource, FrameResourceCount> frame_resources;
        std::size_t next_frame_resource_index = 0;
        Private::ResourceStateTracker resource_states;

        std::vector<std::unique_ptr<RHI::ColorAttachmentView>> color_views;
        std::unique_ptr<RHI::Texture> depth_texture;
        std::unique_ptr<RHI::DepthStencilView> depth_view;
        std::unique_ptr<RHI::Texture> scene_color_texture;
        std::unique_ptr<RHI::ColorAttachmentView> scene_color_view;
        std::unique_ptr<RHI::SampledTextureView> scene_sampled_view;

        std::vector<GpuMesh> meshes;
        std::vector<GpuTexture> textures;
        std::vector<GpuMaterial> materials;
        std::unique_ptr<RHI::Shader> vertex_shader;
        std::unique_ptr<RHI::Shader> pixel_shader;
        std::unique_ptr<RHI::PipelineLayout> pipeline_layout;
        std::unique_ptr<RHI::GraphicsPipeline> pipeline;
        std::unique_ptr<RHI::Shader> ui_vertex_shader;
        std::unique_ptr<RHI::Shader> ui_pixel_shader;
        std::unique_ptr<RHI::PipelineLayout> ui_pipeline_layout;
        std::unique_ptr<RHI::GraphicsPipeline> ui_pipeline;
        std::unique_ptr<RHI::Sampler> ui_sampler;
        std::unique_ptr<RHI::Shader> gizmo_vertex_shader;
        std::unique_ptr<RHI::Shader> gizmo_visible_pixel_shader;
        std::unique_ptr<RHI::Shader> gizmo_occluded_pixel_shader;
        std::unique_ptr<RHI::PipelineLayout> gizmo_pipeline_layout;
        std::unique_ptr<RHI::GraphicsPipeline> gizmo_visible_pipeline;
        std::unique_ptr<RHI::GraphicsPipeline> gizmo_occluded_pipeline;
        std::unique_ptr<RHI::Shader> skybox_vertex_shader;
        std::unique_ptr<RHI::Shader> skybox_pixel_shader;
        std::unique_ptr<RHI::PipelineLayout> skybox_pipeline_layout;
        std::unique_ptr<RHI::GraphicsPipeline> skybox_pipeline;
        std::unique_ptr<RHI::Sampler> skybox_sampler;
        std::unique_ptr<RHI::Buffer> skybox_vertex_buffer;
        std::unique_ptr<RHI::Buffer> skybox_index_buffer;
        std::uint32_t skybox_index_count = 0;
    };

    bool RHIRenderer::Impl::ValidateSceneDescription() const noexcept
    {
        if (scene.meshes.empty() || scene.textures.empty()
            || scene.materials.empty()
            || scene.vertex_shader.empty() || scene.pixel_shader.empty())
        {
            return false;
        }

        for (std::size_t index = 0; index < scene.meshes.size(); ++index)
        {
            const RHIMeshDescription& mesh = scene.meshes[index];
            if (!mesh.handle.IsValid() || mesh.vertices.empty()
                || mesh.indices.empty()
                || mesh.indices.size()
                    > (std::numeric_limits<std::uint32_t>::max)()
                || std::any_of(
                    scene.meshes.begin() + static_cast<std::ptrdiff_t>(index + 1),
                    scene.meshes.end(),
                    [&mesh](const RHIMeshDescription& candidate)
                    { return candidate.handle == mesh.handle; }))
            {
                return false;
            }
        }

        for (std::size_t index = 0; index < scene.textures.size(); ++index)
        {
            const RHITextureDescription& texture = scene.textures[index];
            std::uint64_t expected_bytes = 0;
            const bool valid_view =
                (texture.view.dimension
                        == RHI::SampledTextureViewDimension::Texture2D
                    && texture.array_layers == 1)
                || (texture.view.dimension
                        == RHI::SampledTextureViewDimension::TextureCube
                    && texture.array_layers == 6
                    && texture.width == texture.height);
            if (!texture.handle.IsValid() || texture.width == 0
                || texture.height == 0 || !valid_view
                || texture.view.base_mip_level >= texture.mip_levels
                || (texture.view.mip_level_count != 0
                    && texture.view.mip_level_count
                        > texture.mip_levels - texture.view.base_mip_level)
                || !TryGetTightTextureDataSize(texture, expected_bytes))
            {
                return false;
            }
            if (expected_bytes != texture.pixels.size()
                || std::any_of(
                    scene.textures.begin()
                        + static_cast<std::ptrdiff_t>(index + 1),
                    scene.textures.end(),
                    [&texture](const RHITextureDescription& candidate)
                    { return candidate.handle == texture.handle; }))
            {
                return false;
            }
        }

        for (std::size_t index = 0; index < scene.materials.size(); ++index)
        {
            const RHIMaterialDescription& material = scene.materials[index];
            const bool texture_exists = std::any_of(
                scene.textures.begin(), scene.textures.end(),
                [&material](const RHITextureDescription& texture)
                { return texture.handle == material.albedo_texture; });
            if (!material.handle.IsValid() || !material.albedo_texture.IsValid()
                || !texture_exists
                || std::any_of(
                    scene.materials.begin()
                        + static_cast<std::ptrdiff_t>(index + 1),
                    scene.materials.end(),
                    [&material](const RHIMaterialDescription& candidate)
                    { return candidate.handle == material.handle; }))
            {
                return false;
            }
        }

        if (scene.ui_composition.has_value())
        {
            const RHIUICompositionDescription& ui = *scene.ui_composition;
            const bool font_exists = std::any_of(
                scene.textures.begin(), scene.textures.end(),
                [&ui](const RHITextureDescription& texture)
                { return texture.handle == ui.font_atlas_texture; });
            if (!ui.font_atlas_texture.IsValid() || !font_exists
                || ui.vertex_shader.empty() || ui.pixel_shader.empty())
            {
                return false;
            }
        }

        if (scene.gizmo_composition.has_value())
        {
            const RHIGizmoCompositionDescription& gizmo =
                *scene.gizmo_composition;
            if (gizmo.vertex_shader.empty()
                || gizmo.visible_pixel_shader.empty()
                || gizmo.occluded_pixel_shader.empty())
            {
                return false;
            }
        }

        if (scene.skybox.has_value())
        {
            const RHISkyboxDescription& skybox = *scene.skybox;
            const auto texture = std::find_if(
                scene.textures.begin(), scene.textures.end(),
                [&skybox](const RHITextureDescription& candidate)
                { return candidate.handle == skybox.cubemap_texture; });
            if (!skybox.cubemap_texture.IsValid()
                || texture == scene.textures.end()
                || texture->view.dimension
                    != RHI::SampledTextureViewDimension::TextureCube
                || skybox.vertex_shader.empty() || skybox.pixel_shader.empty()
                || !std::isfinite(skybox.intensity) || skybox.intensity < 0.0F
                || !std::isfinite(skybox.exposure) || skybox.exposure <= 0.0F)
            {
                return false;
            }
        }

        return true;
    }

    bool RHIRenderer::Impl::Initialize()
    {
        if (state != State::Created)
        {
            return false;
        }
        state = State::Failed;

        if (backend == nullptr || !surface.IsValid() || !extent.HasArea()
            || !ValidateSceneDescription())
        {
            std::fprintf(stderr, "[Renderer] Invalid renderer create info\n");
            return false;
        }
        std::vector<RHI::AdapterInfo> adapters;
        if (!Check(backend->EnumerateAdapters(adapters), "EnumerateAdapters"))
        {
            return false;
        }
        const auto adapter = std::find_if(
            adapters.begin(), adapters.end(),
            [](const RHI::AdapterInfo& candidate)
            {
                return candidate.capabilities.features.graphics_pipeline.support
                    != RHI::FeatureSupport::Unsupported;
            });
        if (adapter == adapters.end())
        {
            std::fprintf(stderr, "[Renderer] No graphics-capable adapter\n");
            return false;
        }

        RHI::FormatCapabilities color_capabilities;
        RHI::FormatCapabilities depth_capabilities;
        if (!Check(backend->QueryFormatCapabilities(
                adapter->id, ColorFormat, color_capabilities),
                "Query color format")
            || color_capabilities.render_target
                == RHI::FeatureSupport::Unsupported
            || !Check(backend->QueryFormatCapabilities(
                adapter->id, DepthFormat, depth_capabilities),
                "Query depth format")
            || depth_capabilities.depth_stencil
                == RHI::FeatureSupport::Unsupported)
        {
            std::fprintf(stderr, "[Renderer] Required render format unsupported\n");
            return false;
        }
        for (const RHITextureDescription& texture : scene.textures)
        {
            RHI::FormatCapabilities sampled_capabilities;
            if (!Check(backend->QueryFormatCapabilities(
                    adapter->id, texture.format, sampled_capabilities),
                    "Query sampled texture format")
                || sampled_capabilities.sampled_texture
                    == RHI::FeatureSupport::Unsupported)
            {
                std::fprintf(stderr,
                    "[Renderer] Required sampled texture format unsupported\n");
                return false;
            }
        }

        if (!Check(backend->CreateDevice({.adapter_id = adapter->id}, device),
                "CreateDevice")
            || !Check(device->CreateFence(fence), "CreateFence")
            || !Check(device->CreateSwapchain(
                {.surface = surface,
                 .width = extent.width,
                 .height = extent.height,
                 .format = ColorFormat,
                 .image_count = 2,
                 .present_mode = RHI::PresentMode::Fifo},
                swapchain), "CreateSwapchain")
            || !CreateAttachments()
            || !CreateSceneResources()
            || !CreateFrameResources())
        {
            ReleaseResources();
            return false;
        }

        // GPU 对象已复制所需内容，释放启动期 CPU 数据，避免长期保存两份资源。
        scene = {};
        state = State::Initialized;
        std::fprintf(stdout, "[Renderer] RHI renderer initialized\n");
        return true;
    }

    bool RHIRenderer::Impl::CreateAttachments()
    {
        // Resize 只替换附件；不能清掉 Renderer 持有的普通采样纹理状态。
        for (std::uint32_t index = 0;
             index < swapchain->GetImageCount();
             ++index)
        {
            if (RHI::Texture* image = swapchain->GetImage(index))
            {
                resource_states.ForgetTexture(*image);
            }
        }
        if (depth_texture != nullptr)
        {
            resource_states.ForgetTexture(*depth_texture);
        }
        color_views.clear();
        depth_view.reset();
        depth_texture.reset();

        try
        {
            color_views.reserve(swapchain->GetImageCount());
        }
        catch (...)
        {
            return false;
        }

        for (std::uint32_t index = 0; index < swapchain->GetImageCount(); ++index)
        {
            RHI::Texture* image = swapchain->GetImage(index);
            std::unique_ptr<RHI::ColorAttachmentView> view;
            if (image == nullptr
                || !Check(device->CreateColorAttachmentView(*image, view),
                    "CreateColorAttachmentView")
                || !resource_states.TrackTexture(
                    *image, RHI::TextureState::Present))
            {
                return false;
            }
            color_views.push_back(std::move(view));
        }

        if (ui_enabled)
        {
            return CreateSceneAttachments(
                scene_extent.HasArea() ? scene_extent : extent);
        }

        if (!Check(device->CreateTexture(
                {.width = extent.width,
                 .height = extent.height,
                 .format = DepthFormat,
                 .usage = RHI::TextureUsage::DepthStencilAttachment},
                RHI::TextureState::DepthWrite,
                depth_texture), "Create depth texture")
            || !Check(device->CreateDepthStencilView(
                *depth_texture, depth_view), "CreateDepthStencilView")
            || !resource_states.TrackTexture(
                *depth_texture, RHI::TextureState::DepthWrite))
        {
            return false;
        }
        return true;
    }

    bool RHIRenderer::Impl::CreateSceneAttachments(
        RenderCore::RenderExtent new_extent)
    {
        if (!new_extent.HasArea())
        {
            return false;
        }
        if (scene_color_texture != nullptr)
        {
            resource_states.ForgetTexture(*scene_color_texture);
        }
        if (depth_texture != nullptr)
        {
            resource_states.ForgetTexture(*depth_texture);
        }
        scene_sampled_view.reset();
        scene_color_view.reset();
        depth_view.reset();
        scene_color_texture.reset();
        depth_texture.reset();

        if (!Check(device->CreateTexture(
                {.width = new_extent.width,
                 .height = new_extent.height,
                 .format = ColorFormat,
                 .usage = RHI::TextureUsage::ColorAttachment
                    | RHI::TextureUsage::Sampled},
                RHI::TextureState::ColorAttachment,
                scene_color_texture), "Create scene color texture")
            || !Check(device->CreateColorAttachmentView(
                *scene_color_texture, scene_color_view),
                "Create scene color attachment view")
            || !Check(device->CreateSampledTextureView(
                *scene_color_texture, {}, scene_sampled_view),
                "Create scene sampled view")
            || !resource_states.TrackTexture(
                *scene_color_texture, RHI::TextureState::ColorAttachment)
            || !Check(device->CreateTexture(
                {.width = new_extent.width,
                 .height = new_extent.height,
                 .format = DepthFormat,
                 .usage = RHI::TextureUsage::DepthStencilAttachment},
                RHI::TextureState::DepthWrite,
                depth_texture), "Create scene depth texture")
            || !Check(device->CreateDepthStencilView(
                *depth_texture, depth_view),
                "Create scene depth view")
            || !resource_states.TrackTexture(
                *depth_texture, RHI::TextureState::DepthWrite))
        {
            return false;
        }
        scene_extent = new_extent;
        return true;
    }

    bool RHIRenderer::Impl::CreateSceneResources()
    {
        struct MeshUploadBuffers final
        {
            std::unique_ptr<RHI::Buffer> vertices;
            std::unique_ptr<RHI::Buffer> indices;
        };
        struct TextureUploadBuffer final
        {
            std::unique_ptr<RHI::Buffer> buffer;
            std::vector<RHI::TextureUploadLayout> layouts;
        };

        std::vector<MeshUploadBuffers> mesh_uploads;
        std::vector<TextureUploadBuffer> texture_uploads;
        std::unique_ptr<RHI::CommandAllocator> upload_command_allocator;
        std::unique_ptr<RHI::CommandList> upload_command_list;

        if (!Check(device->CreateCommandAllocator(upload_command_allocator),
                "Create upload command allocator")
            || !Check(device->CreateCommandList(
                *upload_command_allocator,
                upload_command_list), "Create upload command list"))
        {
            return false;
        }

        meshes.reserve(scene.meshes.size());
        textures.reserve(scene.textures.size());
        materials.reserve(scene.materials.size());
        mesh_uploads.reserve(scene.meshes.size());
        texture_uploads.reserve(scene.textures.size());

        for (const RHIMeshDescription& source : scene.meshes)
        {
            const std::uint64_t vertex_bytes = source.vertices.size()
                * sizeof(PositionColorTextureVertex);
            const std::uint64_t index_bytes = source.indices.size()
                * sizeof(std::uint16_t);
            const std::string suffix = std::to_string(source.handle.value);
            MeshUploadBuffers upload;
            GpuMesh mesh;
            mesh.handle = source.handle;
            mesh.index_count = static_cast<std::uint32_t>(source.indices.size());

            if (!Check(device->CreateBuffer(
                    {.size = vertex_bytes,
                     .usage = RHI::BufferUsage::CopySource,
                     .memory = RHI::MemoryType::Upload,
                     .debug_name = "Renderer.Mesh." + suffix + ".Vertices.Upload"},
                    RHI::BufferState::CopySource, upload.vertices),
                    "Create vertex upload buffer")
                || !Check(device->CreateBuffer(
                    {.size = vertex_bytes,
                     .usage = RHI::BufferUsage::Vertex
                        | RHI::BufferUsage::CopyDestination,
                     .memory = RHI::MemoryType::DeviceLocal,
                     .debug_name = "Renderer.Mesh." + suffix + ".Vertices"},
                    RHI::BufferState::CopyDestination, mesh.vertex_buffer),
                    "Create device-local vertex buffer")
                || !Check(device->CreateBuffer(
                    {.size = index_bytes,
                     .usage = RHI::BufferUsage::CopySource,
                     .memory = RHI::MemoryType::Upload,
                     .debug_name = "Renderer.Mesh." + suffix + ".Indices.Upload"},
                    RHI::BufferState::CopySource, upload.indices),
                    "Create index upload buffer")
                || !Check(device->CreateBuffer(
                    {.size = index_bytes,
                     .usage = RHI::BufferUsage::Index
                        | RHI::BufferUsage::CopyDestination,
                     .memory = RHI::MemoryType::DeviceLocal,
                     .debug_name = "Renderer.Mesh." + suffix + ".Indices"},
                    RHI::BufferState::CopyDestination, mesh.index_buffer),
                    "Create device-local index buffer")
                || !resource_states.TrackBuffer(
                    *upload.vertices, RHI::BufferState::CopySource)
                || !resource_states.TrackBuffer(
                    *mesh.vertex_buffer, RHI::BufferState::CopyDestination)
                || !resource_states.TrackBuffer(
                    *upload.indices, RHI::BufferState::CopySource)
                || !resource_states.TrackBuffer(
                    *mesh.index_buffer, RHI::BufferState::CopyDestination))
            {
                return false;
            }

            void* mapped = nullptr;
            if (!Check(upload.vertices->Map(0, vertex_bytes, mapped),
                    "Map vertex upload buffer"))
            {
                return false;
            }
            std::memcpy(mapped, source.vertices.data(),
                static_cast<std::size_t>(vertex_bytes));
            upload.vertices->Unmap(0, vertex_bytes);
            if (!Check(upload_command_list->CopyBuffer(
                    *upload.vertices, 0, *mesh.vertex_buffer, 0, vertex_bytes),
                    "Copy vertex buffer")
                || !TransitionTrackedBuffer(
                    *upload_command_list, *mesh.vertex_buffer,
                    RHI::BufferState::Vertex, "Transition vertex buffer"))
            {
                return false;
            }

            if (!Check(upload.indices->Map(0, index_bytes, mapped),
                    "Map index upload buffer"))
            {
                return false;
            }
            std::memcpy(mapped, source.indices.data(),
                static_cast<std::size_t>(index_bytes));
            upload.indices->Unmap(0, index_bytes);
            if (!Check(upload_command_list->CopyBuffer(
                    *upload.indices, 0, *mesh.index_buffer, 0, index_bytes),
                    "Copy index buffer")
                || !TransitionTrackedBuffer(
                    *upload_command_list, *mesh.index_buffer,
                    RHI::BufferState::Index, "Transition index buffer"))
            {
                return false;
            }

            mesh_uploads.push_back(std::move(upload));
            meshes.push_back(std::move(mesh));
        }

        for (const RHITextureDescription& source : scene.textures)
        {
            const RHI::TextureDescription description{
                .width = source.width,
                .height = source.height,
                .array_layers = source.array_layers,
                .mip_levels = source.mip_levels,
                .format = source.format,
                .usage = RHI::TextureUsage::Sampled
                    | RHI::TextureUsage::CopyDestination};
            const std::string suffix = std::to_string(source.handle.value);
            TextureUploadBuffer upload;
            GpuTexture texture;
            texture.handle = source.handle;

            std::uint64_t upload_size = 0;
            try
            {
                upload.layouts.reserve(
                    static_cast<std::size_t>(source.array_layers)
                        * source.mip_levels);
            }
            catch (...)
            {
                return false;
            }
            for (std::uint32_t layer = 0;
                 layer < source.array_layers;
                 ++layer)
            {
                for (std::uint32_t mip = 0; mip < source.mip_levels; ++mip)
                {
                    RHI::TextureUploadLayout layout;
                    if (!Check(device->GetTextureUploadLayout(
                            description,
                            {.mip_level = mip, .array_layer = layer},
                            upload_size,
                            layout),
                            "Get texture upload layout"))
                    {
                        return false;
                    }
                    upload_size = layout.required_buffer_size;
                    upload.layouts.push_back(layout);
                }
            }

            if (!Check(device->CreateTexture(
                    description, RHI::TextureState::CopyDestination,
                    texture.texture), "Create sampled texture")
                || !Check(device->CreateBuffer(
                    {.size = upload_size,
                     .usage = RHI::BufferUsage::CopySource,
                     .memory = RHI::MemoryType::Upload,
                     .debug_name = "Renderer.Texture." + suffix + ".Upload"},
                    RHI::BufferState::CopySource, upload.buffer),
                    "Create texture upload buffer")
                || !resource_states.TrackTexture(
                    *texture.texture, RHI::TextureState::CopyDestination)
                || !resource_states.TrackBuffer(
                    *upload.buffer, RHI::BufferState::CopySource))
            {
                return false;
            }

            void* mapped = nullptr;
            if (!Check(upload.buffer->Map(
                    0, upload_size, mapped),
                    "Map texture upload buffer"))
            {
                return false;
            }
            auto* mapped_bytes = static_cast<std::byte*>(mapped);
            std::uint64_t source_offset = 0;
            for (const RHI::TextureUploadLayout& layout : upload.layouts)
            {
                for (std::uint32_t row = 0; row < layout.row_count; ++row)
                {
                    std::memcpy(
                        mapped_bytes + layout.buffer_offset
                            + static_cast<std::uint64_t>(row)
                                * layout.row_pitch,
                        source.pixels.data() + source_offset
                            + static_cast<std::uint64_t>(row)
                                * layout.row_size,
                        static_cast<std::size_t>(layout.row_size));
                }
                source_offset += layout.row_size * layout.row_count;
            }
            upload.buffer->Unmap(0, upload_size);
            for (const RHI::TextureUploadLayout& layout : upload.layouts)
            {
                if (!Check(upload_command_list->CopyBufferToTexture(
                        *upload.buffer, *texture.texture, layout),
                        "Copy texture upload buffer"))
                {
                    return false;
                }
            }
            if (!TransitionTrackedTexture(
                    *upload_command_list, *texture.texture,
                    RHI::TextureState::ShaderRead,
                    "Transition sampled texture")
                || !Check(device->CreateSampledTextureView(
                    *texture.texture, source.view, texture.view),
                    "Create sampled texture view"))
            {
                return false;
            }

            texture_uploads.push_back(std::move(upload));
            textures.push_back(std::move(texture));
        }

        for (const RHIMaterialDescription& source : scene.materials)
        {
            if (FindTexture(source.albedo_texture) == nullptr)
            {
                return false;
            }
            GpuMaterial material;
            material.handle = source.handle;
            material.albedo_texture = source.albedo_texture;
            if (!Check(device->CreateSampler(source.sampler, material.sampler),
                    "Create material sampler"))
            {
                return false;
            }
            materials.push_back(std::move(material));
        }

        if (!Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Vertex,
                 .format = scene.shader_format,
                 .bytecode = scene.vertex_shader,
                 .debug_name = "Renderer.PositionColorVS"},
                vertex_shader), "Create vertex shader")
            || !Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Pixel,
                 .format = scene.shader_format,
                 .bytecode = scene.pixel_shader,
                 .debug_name = "Renderer.PositionColorPS"},
                pixel_shader), "Create pixel shader"))
        {
            return false;
        }

        constexpr std::array<RHI::ResourceBindingLayout, 3> bindings{{
            {.binding = 0,
             .type = RHI::ResourceBindingType::ConstantBuffer,
             // 同一逐物体常量由 VS 读取矩阵、PS 读取颜色。
             .visibility = RHI::ShaderVisibility::All},
            {.binding = 1,
             .type = RHI::ResourceBindingType::SampledTexture,
             .visibility = RHI::ShaderVisibility::Pixel},
            {.binding = 2,
             .type = RHI::ResourceBindingType::Sampler,
             .visibility = RHI::ShaderVisibility::Pixel}}};
        if (!Check(device->CreatePipelineLayout(
                {.bindings = bindings}, pipeline_layout),
                "CreatePipelineLayout"))
        {
            return false;
        }

        constexpr std::array<RHI::VertexAttribute, 3> attributes{{
            {.location = 0,
             .format = RHI::RHIFormat::R32G32B32_Float,
             .offset = 0},
            {.location = 1,
             .format = RHI::RHIFormat::R32G32B32_Float,
             .offset = 12},
            {.location = 2,
             .format = RHI::RHIFormat::R32G32_Float,
             .offset = 24}}};
        if (!Check(device->CreateGraphicsPipeline(
                {.layout = pipeline_layout.get(),
                 .vertex_shader = vertex_shader.get(),
                 .pixel_shader = pixel_shader.get(),
                 .vertex_attributes = attributes,
                 .vertex_stride = sizeof(PositionColorTextureVertex),
                 .topology = RHI::PrimitiveTopology::TriangleList,
                 // 示例网格从外部观察时在 D3D 屏幕坐标中为顺时针正面。
                 .cull_mode = RHI::CullMode::Back,
                 .front_face = RHI::FrontFace::Clockwise,
                 .color_format = ColorFormat,
                 .depth_format = DepthFormat,
                 .depth = {.test_enabled = true,
                           .write_enabled = true,
                           .compare = RHI::CompareOperation::Less}},
                pipeline),
                "CreateGraphicsPipeline"))
        {
            return false;
        }

        if (skybox_enabled && !CreateSkyboxResources())
        {
            return false;
        }
        if (ui_enabled && !CreateUIResources())
        {
            return false;
        }
        if (gizmo_enabled && !CreateGizmoResources())
        {
            return false;
        }

        // 完成并提交初始化复制命令；提交后用 Fence 证明临时 Buffer 可释放。
        if (!Check(upload_command_list->Close(), "Close upload command list"))
        {
            return false;
        }

        RHI::Queue& graphics_queue = device->GetGraphicsQueue();
        if (!Check(graphics_queue.Execute(*upload_command_list),
                "Execute buffer uploads"))
        {
            return false;
        }

        if (last_signaled_fence_value
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            std::fprintf(stderr, "[Renderer] Fence value exhausted\n");
            return false;
        }
        ++last_signaled_fence_value;

        if (!Check(graphics_queue.Signal(*fence, last_signaled_fence_value),
                "Signal buffer uploads")
            || !Check(fence->Wait(
                last_signaled_fence_value,
                GpuWaitTimeoutMilliseconds),
                "Wait for buffer uploads"))
        {
            return false;
        }

        // 初始化上传已经完成，临时 Buffer 即将随局部容器离开作用域。
        for (const MeshUploadBuffers& upload : mesh_uploads)
        {
            resource_states.ForgetBuffer(*upload.vertices);
            resource_states.ForgetBuffer(*upload.indices);
        }
        for (const TextureUploadBuffer& upload : texture_uploads)
        {
            resource_states.ForgetBuffer(*upload.buffer);
        }
        return true;
    }

    bool RHIRenderer::Impl::CreateUIResources()
    {
        if (!scene.ui_composition.has_value()
            || FindTexture(ui_font_texture) == nullptr)
        {
            return false;
        }
        const RHIUICompositionDescription& ui = *scene.ui_composition;
        if (!Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Vertex,
                 .format = ui.shader_format,
                 .bytecode = ui.vertex_shader,
                 .debug_name = "Renderer.UI.VS"},
                ui_vertex_shader), "Create UI vertex shader")
            || !Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Pixel,
                 .format = ui.shader_format,
                 .bytecode = ui.pixel_shader,
                 .debug_name = "Renderer.UI.PS"},
                ui_pixel_shader), "Create UI pixel shader")
            || !Check(device->CreateSampler(ui.sampler, ui_sampler),
                "Create UI sampler"))
        {
            return false;
        }

        constexpr std::array<RHI::ResourceBindingLayout, 3> bindings{{
            {.binding = 0,
             .type = RHI::ResourceBindingType::ConstantBuffer,
             .visibility = RHI::ShaderVisibility::All},
            {.binding = 1,
             .type = RHI::ResourceBindingType::SampledTexture,
             .visibility = RHI::ShaderVisibility::Pixel},
            {.binding = 2,
             .type = RHI::ResourceBindingType::Sampler,
             .visibility = RHI::ShaderVisibility::Pixel}}};
        if (!Check(device->CreatePipelineLayout(
                {.bindings = bindings}, ui_pipeline_layout),
                "Create UI pipeline layout"))
        {
            return false;
        }

        constexpr std::array<RHI::VertexAttribute, 3> attributes{{
            {.location = 0,
             .format = RHI::RHIFormat::R32G32_Float,
             .offset = 0},
            {.location = 1,
             .format = RHI::RHIFormat::R32G32_Float,
             .offset = 8},
            {.location = 2,
             .format = RHI::RHIFormat::R8G8B8A8_UNorm,
             .offset = 16}}};
        return Check(device->CreateGraphicsPipeline(
                {.layout = ui_pipeline_layout.get(),
                 .vertex_shader = ui_vertex_shader.get(),
                 .pixel_shader = ui_pixel_shader.get(),
                 .vertex_attributes = attributes,
                 .vertex_stride = sizeof(RenderCore::UIVertex),
                 .topology = RHI::PrimitiveTopology::TriangleList,
                 .cull_mode = RHI::CullMode::None,
                 .front_face = RHI::FrontFace::CounterClockwise,
                 .color_format = ColorFormat,
                 .depth_format = RHI::RHIFormat::Unknown,
                 .depth = {},
                 .blend = {.alpha_blending_enabled = true}},
                ui_pipeline), "Create UI graphics pipeline");
    }

    bool RHIRenderer::Impl::CreateGizmoResources()
    {
        if (!scene.gizmo_composition.has_value())
        {
            return false;
        }
        const RHIGizmoCompositionDescription& gizmo =
            *scene.gizmo_composition;
        if (!Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Vertex,
                 .format = gizmo.shader_format,
                 .bytecode = gizmo.vertex_shader,
                 .debug_name = "Renderer.Gizmo.VS"},
                gizmo_vertex_shader), "Create Gizmo vertex shader")
            || !Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Pixel,
                 .format = gizmo.shader_format,
                 .bytecode = gizmo.visible_pixel_shader,
                 .debug_name = "Renderer.Gizmo.Visible.PS"},
                gizmo_visible_pixel_shader),
                "Create visible Gizmo pixel shader")
            || !Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Pixel,
                 .format = gizmo.shader_format,
                 .bytecode = gizmo.occluded_pixel_shader,
                 .debug_name = "Renderer.Gizmo.Occluded.PS"},
                gizmo_occluded_pixel_shader),
                "Create occluded Gizmo pixel shader")
            || !Check(device->CreatePipelineLayout(
                {}, gizmo_pipeline_layout), "Create Gizmo pipeline layout"))
        {
            return false;
        }

        constexpr std::array<RHI::VertexAttribute, 2> attributes{{
            {.location = 0,
             .format = RHI::RHIFormat::R32G32B32A32_Float,
             .offset = 0},
            {.location = 1,
             .format = RHI::RHIFormat::R8G8B8A8_UNorm,
             .offset = 16}}};
        const auto create_pipeline = [this, &attributes](
            RHI::Shader& pixel_shader,
            RHI::CompareOperation compare,
            std::unique_ptr<RHI::GraphicsPipeline>& out_pipeline,
            const char* operation)
        {
            return Check(device->CreateGraphicsPipeline(
                {.layout = gizmo_pipeline_layout.get(),
                 .vertex_shader = gizmo_vertex_shader.get(),
                 .pixel_shader = &pixel_shader,
                 .vertex_attributes = attributes,
                 .vertex_stride = sizeof(RenderCore::GizmoVertex),
                 .topology = RHI::PrimitiveTopology::TriangleList,
                 .cull_mode = RHI::CullMode::None,
                 .front_face = RHI::FrontFace::CounterClockwise,
                 .color_format = ColorFormat,
                 .depth_format = DepthFormat,
                 .depth = {.test_enabled = true,
                           .write_enabled = false,
                           .compare = compare},
                 .blend = {.alpha_blending_enabled = true}},
                out_pipeline), operation);
        };

        return create_pipeline(
                *gizmo_occluded_pixel_shader,
                RHI::CompareOperation::Greater,
                gizmo_occluded_pipeline,
                "Create occluded Gizmo pipeline")
            && create_pipeline(
                *gizmo_visible_pixel_shader,
                RHI::CompareOperation::LessEqual,
                gizmo_visible_pipeline,
                "Create visible Gizmo pipeline");
    }

    bool RHIRenderer::Impl::CreateSkyboxResources()
    {
        if (!scene.skybox.has_value())
        {
            return false;
        }
        const RHISkyboxDescription& skybox = *scene.skybox;
        GpuTexture* cubemap = FindTexture(skybox.cubemap_texture);
        if (cubemap == nullptr)
        {
            return false;
        }

        constexpr std::array<SkyboxVertex, 8> vertices{{
            {{{-1.0F, -1.0F, -1.0F}}},
            {{{-1.0F,  1.0F, -1.0F}}},
            {{{ 1.0F,  1.0F, -1.0F}}},
            {{{ 1.0F, -1.0F, -1.0F}}},
            {{{-1.0F, -1.0F,  1.0F}}},
            {{{-1.0F,  1.0F,  1.0F}}},
            {{{ 1.0F,  1.0F,  1.0F}}},
            {{{ 1.0F, -1.0F,  1.0F}}}
        }};
        constexpr std::array<std::uint16_t, 36> indices{{
            0, 1, 2, 0, 2, 3,
            4, 6, 5, 4, 7, 6,
            0, 4, 5, 0, 5, 1,
            3, 2, 6, 3, 6, 7,
            1, 5, 6, 1, 6, 2,
            0, 3, 7, 0, 7, 4
        }};

        const std::uint64_t vertex_bytes = sizeof(vertices);
        const std::uint64_t index_bytes = sizeof(indices);
        if (!Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Vertex,
                 .format = skybox.shader_format,
                 .bytecode = skybox.vertex_shader,
                 .debug_name = "Renderer.Skybox.VS"},
                skybox_vertex_shader), "Create Skybox vertex shader")
            || !Check(device->CreateShader(
                {.stage = RHI::ShaderStage::Pixel,
                 .format = skybox.shader_format,
                 .bytecode = skybox.pixel_shader,
                 .debug_name = "Renderer.Skybox.PS"},
                skybox_pixel_shader), "Create Skybox pixel shader"))
        {
            return false;
        }

        constexpr std::array<RHI::ResourceBindingLayout, 3> bindings{{
            {.binding = 0,
             .type = RHI::ResourceBindingType::ConstantBuffer,
             .visibility = RHI::ShaderVisibility::All},
            {.binding = 1,
             .type = RHI::ResourceBindingType::SampledTexture,
             .visibility = RHI::ShaderVisibility::Pixel},
            {.binding = 2,
             .type = RHI::ResourceBindingType::Sampler,
             .visibility = RHI::ShaderVisibility::Pixel}
        }};
        constexpr std::array<RHI::VertexAttribute, 1> attributes{{
            {.location = 0,
             .format = RHI::RHIFormat::R32G32B32_Float,
             .offset = 0}
        }};
        if (!Check(device->CreatePipelineLayout(
                {.bindings = bindings}, skybox_pipeline_layout),
                "Create Skybox pipeline layout")
            || !Check(device->CreateGraphicsPipeline(
                {.layout = skybox_pipeline_layout.get(),
                 .vertex_shader = skybox_vertex_shader.get(),
                 .pixel_shader = skybox_pixel_shader.get(),
                 .vertex_attributes = attributes,
                 .vertex_stride = sizeof(SkyboxVertex),
                 .topology = RHI::PrimitiveTopology::TriangleList,
                 .cull_mode = RHI::CullMode::None,
                 .front_face = RHI::FrontFace::Clockwise,
                 .color_format = ColorFormat,
                 .depth_format = DepthFormat,
                 .depth = {.test_enabled = true,
                           .write_enabled = false,
                           .compare = RHI::CompareOperation::LessEqual}},
                skybox_pipeline), "Create Skybox graphics pipeline")
            || !Check(device->CreateSampler(
                skybox.sampler, skybox_sampler), "Create Skybox sampler")
            || !Check(device->CreateBuffer(
                {.size = vertex_bytes,
                 .usage = RHI::BufferUsage::Vertex,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Renderer.Skybox.Vertices"},
                RHI::BufferState::Vertex,
                skybox_vertex_buffer), "Create Skybox vertex buffer")
            || !Check(device->CreateBuffer(
                {.size = index_bytes,
                 .usage = RHI::BufferUsage::Index,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Renderer.Skybox.Indices"},
                RHI::BufferState::Index,
                skybox_index_buffer), "Create Skybox index buffer")
            || !resource_states.TrackBuffer(
                *skybox_vertex_buffer, RHI::BufferState::Vertex)
            || !resource_states.TrackBuffer(
                *skybox_index_buffer, RHI::BufferState::Index))
        {
            return false;
        }

        void* mapped = nullptr;
        if (!Check(skybox_vertex_buffer->Map(
                0, vertex_bytes, mapped), "Map Skybox vertices"))
        {
            return false;
        }
        std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(vertex_bytes));
        skybox_vertex_buffer->Unmap(0, vertex_bytes);
        if (!Check(skybox_index_buffer->Map(
                0, index_bytes, mapped), "Map Skybox indices"))
        {
            return false;
        }
        std::memcpy(mapped, indices.data(), static_cast<std::size_t>(index_bytes));
        skybox_index_buffer->Unmap(0, index_bytes);
        skybox_index_count = static_cast<std::uint32_t>(indices.size());
        return true;
    }

    RHIRenderer::Impl::GpuMesh* RHIRenderer::Impl::FindMesh(
        RenderCore::MeshHandle handle) noexcept
    {
        const auto found = std::find_if(
            meshes.begin(), meshes.end(),
            [handle](const GpuMesh& mesh) { return mesh.handle == handle; });
        return found == meshes.end() ? nullptr : &*found;
    }

    RHIRenderer::Impl::GpuTexture* RHIRenderer::Impl::FindTexture(
        RenderCore::TextureHandle handle) noexcept
    {
        const auto found = std::find_if(
            textures.begin(), textures.end(),
            [handle](const GpuTexture& texture)
            { return texture.handle == handle; });
        return found == textures.end() ? nullptr : &*found;
    }

    RHIRenderer::Impl::GpuMaterial* RHIRenderer::Impl::FindMaterial(
        RenderCore::MaterialHandle handle) noexcept
    {
        const auto found = std::find_if(
            materials.begin(), materials.end(),
            [handle](const GpuMaterial& material)
            { return material.handle == handle; });
        return found == materials.end() ? nullptr : &*found;
    }

    bool RHIRenderer::Impl::CreateFrameResources()
    {
        const std::uint64_t required_alignment =
            device->GetCapabilities().limits.constant_buffer_offset_alignment;
        if (required_alignment == 0
            || sizeof(ObjectConstants)
                > (std::numeric_limits<std::uint64_t>::max)()
                    - required_alignment + 1u)
        {
            return false;
        }
        object_constant_stride = AlignUp(
            sizeof(ObjectConstants), required_alignment);

        for (std::size_t index = 0; index < frame_resources.size(); ++index)
        {
            FrameResource& frame_resource = frame_resources[index];
            if (!CreateObjectConstantBuffer(
                    frame_resource, index, InitialObjectCapacity)
                || (skybox_enabled && !CreateSkyboxFrameResources(
                    frame_resource, index))
                || (ui_enabled && !CreateUIBuffers(
                    frame_resource,
                    index,
                    InitialUIVertexCapacity,
                    InitialUIIndexCapacity))
                || (gizmo_enabled && !CreateGizmoBuffers(
                    frame_resource,
                    index,
                    InitialGizmoVertexCapacity,
                    InitialGizmoIndexCapacity))
                || !Check(device->CreateCommandAllocator(
                    frame_resource.command_allocator),
                    "Create frame command allocator")
                || !Check(device->CreateCommandList(
                    *frame_resource.command_allocator,
                    frame_resource.command_list),
                    "Create frame command list")
                || !Check(frame_resource.command_list->Close(),
                    "Close initial frame command list"))
            {
                return false;
            }
        }
        object_capacity = InitialObjectCapacity;
        if (ui_enabled)
        {
            ui_vertex_capacity = InitialUIVertexCapacity;
            ui_index_capacity = InitialUIIndexCapacity;
        }
        if (gizmo_enabled)
        {
            gizmo_vertex_capacity = InitialGizmoVertexCapacity;
            gizmo_index_capacity = InitialGizmoIndexCapacity;
        }
        return true;
    }

    bool RHIRenderer::Impl::CreateSkyboxFrameResources(
        FrameResource& frame_resource,
        std::size_t frame_index)
    {
        if (!scene.skybox.has_value() || skybox_pipeline_layout == nullptr
            || skybox_sampler == nullptr)
        {
            return false;
        }
        GpuTexture* cubemap = FindTexture(skybox_texture);
        if (cubemap == nullptr || cubemap->view == nullptr)
        {
            return false;
        }
        if (!Check(device->CreateBuffer(
                {.size = sizeof(SkyboxConstants),
                 .usage = RHI::BufferUsage::Constant,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Renderer.Skybox.Constants."
                    + std::to_string(frame_index)},
                RHI::BufferState::Constant,
                frame_resource.skybox_constant_buffer),
                "Create Skybox constant buffer")
            || !resource_states.TrackBuffer(
                *frame_resource.skybox_constant_buffer,
                RHI::BufferState::Constant))
        {
            return false;
        }

        const std::array<RHI::BufferBinding, 1> buffers{{
            {.binding = 0,
             .buffer = frame_resource.skybox_constant_buffer.get(),
             .size = sizeof(SkyboxConstants)}
        }};
        const std::array<RHI::TextureBinding, 1> textures{{
            {.binding = 1, .view = cubemap->view.get()}
        }};
        const std::array<RHI::SamplerBinding, 1> samplers{{
            {.binding = 2, .sampler = skybox_sampler.get()}
        }};
        return Check(device->CreateResourceSet(
            {.layout = skybox_pipeline_layout.get(),
             .buffers = buffers,
             .textures = textures,
             .samplers = samplers},
            frame_resource.skybox_resource_set),
            "Create Skybox resource set");
    }

    bool RHIRenderer::Impl::CreateUIBuffers(
        FrameResource& frame_resource,
        std::size_t frame_index,
        std::size_t vertex_capacity,
        std::size_t index_capacity)
    {
        if (vertex_capacity == 0 || index_capacity == 0
            || vertex_capacity
                > (std::numeric_limits<std::uint64_t>::max)()
                    / sizeof(RenderCore::UIVertex)
            || index_capacity
                > (std::numeric_limits<std::uint64_t>::max)()
                    / sizeof(std::uint32_t))
        {
            return false;
        }
        if (frame_resource.ui_constant_buffer != nullptr)
        {
            resource_states.ForgetBuffer(*frame_resource.ui_constant_buffer);
        }
        if (frame_resource.ui_vertex_buffer != nullptr)
        {
            resource_states.ForgetBuffer(*frame_resource.ui_vertex_buffer);
        }
        if (frame_resource.ui_index_buffer != nullptr)
        {
            resource_states.ForgetBuffer(*frame_resource.ui_index_buffer);
        }
        frame_resource.ui_resource_sets.clear();
        frame_resource.ui_constant_buffer.reset();
        frame_resource.ui_vertex_buffer.reset();
        frame_resource.ui_index_buffer.reset();

        const std::string suffix = std::to_string(frame_index);
        if (!Check(device->CreateBuffer(
                {.size = 256,
                 .usage = RHI::BufferUsage::Constant,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Renderer.UI.Constants." + suffix},
                RHI::BufferState::Constant,
                frame_resource.ui_constant_buffer),
                "Create UI constant buffer")
            || !Check(device->CreateBuffer(
                {.size = vertex_capacity * sizeof(RenderCore::UIVertex),
                 .usage = RHI::BufferUsage::Vertex,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Renderer.UI.Vertices." + suffix},
                RHI::BufferState::Vertex,
                frame_resource.ui_vertex_buffer),
                "Create UI vertex buffer")
            || !Check(device->CreateBuffer(
                {.size = index_capacity * sizeof(std::uint32_t),
                 .usage = RHI::BufferUsage::Index,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Renderer.UI.Indices." + suffix},
                RHI::BufferState::Index,
                frame_resource.ui_index_buffer),
                "Create UI index buffer")
            || !resource_states.TrackBuffer(
                *frame_resource.ui_constant_buffer, RHI::BufferState::Constant)
            || !resource_states.TrackBuffer(
                *frame_resource.ui_vertex_buffer, RHI::BufferState::Vertex)
            || !resource_states.TrackBuffer(
                *frame_resource.ui_index_buffer, RHI::BufferState::Index))
        {
            return false;
        }
        return true;
    }

    bool RHIRenderer::Impl::EnsureUICapacity(
        std::size_t required_vertices,
        std::size_t required_indices)
    {
        if (!ui_enabled
            || (required_vertices <= ui_vertex_capacity
                && required_indices <= ui_index_capacity))
        {
            return true;
        }
        if (!WaitForAllFrameResources())
        {
            return false;
        }
        std::size_t new_vertex_capacity = ui_vertex_capacity;
        std::size_t new_index_capacity = ui_index_capacity;
        while (new_vertex_capacity < required_vertices)
        {
            new_vertex_capacity = new_vertex_capacity
                > (std::numeric_limits<std::size_t>::max)() / 2
                ? required_vertices : new_vertex_capacity * 2;
        }
        while (new_index_capacity < required_indices)
        {
            new_index_capacity = new_index_capacity
                > (std::numeric_limits<std::size_t>::max)() / 2
                ? required_indices : new_index_capacity * 2;
        }
        for (std::size_t index = 0; index < frame_resources.size(); ++index)
        {
            if (!CreateUIBuffers(
                    frame_resources[index], index,
                    new_vertex_capacity, new_index_capacity))
            {
                return false;
            }
        }
        ui_vertex_capacity = new_vertex_capacity;
        ui_index_capacity = new_index_capacity;
        return true;
    }

    bool RHIRenderer::Impl::CreateGizmoBuffers(
        FrameResource& frame_resource,
        std::size_t frame_index,
        std::size_t vertex_capacity,
        std::size_t index_capacity)
    {
        if (vertex_capacity == 0 || index_capacity == 0
            || vertex_capacity
                > (std::numeric_limits<std::uint64_t>::max)()
                    / sizeof(RenderCore::GizmoVertex)
            || index_capacity
                > (std::numeric_limits<std::uint64_t>::max)()
                    / sizeof(std::uint32_t))
        {
            return false;
        }
        if (frame_resource.gizmo_vertex_buffer != nullptr)
        {
            resource_states.ForgetBuffer(*frame_resource.gizmo_vertex_buffer);
        }
        if (frame_resource.gizmo_index_buffer != nullptr)
        {
            resource_states.ForgetBuffer(*frame_resource.gizmo_index_buffer);
        }
        frame_resource.gizmo_vertex_buffer.reset();
        frame_resource.gizmo_index_buffer.reset();

        const std::string suffix = std::to_string(frame_index);
        if (!Check(device->CreateBuffer(
                {.size = vertex_capacity * sizeof(RenderCore::GizmoVertex),
                 .usage = RHI::BufferUsage::Vertex,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Renderer.Gizmo.Vertices." + suffix},
                RHI::BufferState::Vertex,
                frame_resource.gizmo_vertex_buffer),
                "Create Gizmo vertex buffer")
            || !Check(device->CreateBuffer(
                {.size = index_capacity * sizeof(std::uint32_t),
                 .usage = RHI::BufferUsage::Index,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = "Renderer.Gizmo.Indices." + suffix},
                RHI::BufferState::Index,
                frame_resource.gizmo_index_buffer),
                "Create Gizmo index buffer")
            || !resource_states.TrackBuffer(
                *frame_resource.gizmo_vertex_buffer, RHI::BufferState::Vertex)
            || !resource_states.TrackBuffer(
                *frame_resource.gizmo_index_buffer, RHI::BufferState::Index))
        {
            return false;
        }
        return true;
    }

    bool RHIRenderer::Impl::EnsureGizmoCapacity(
        std::size_t required_vertices,
        std::size_t required_indices)
    {
        if (!gizmo_enabled
            || (required_vertices <= gizmo_vertex_capacity
                && required_indices <= gizmo_index_capacity))
        {
            return true;
        }
        if (!WaitForAllFrameResources())
        {
            return false;
        }

        std::size_t new_vertex_capacity = gizmo_vertex_capacity;
        std::size_t new_index_capacity = gizmo_index_capacity;
        while (new_vertex_capacity < required_vertices)
        {
            new_vertex_capacity = new_vertex_capacity
                > (std::numeric_limits<std::size_t>::max)() / 2
                ? required_vertices : new_vertex_capacity * 2;
        }
        while (new_index_capacity < required_indices)
        {
            new_index_capacity = new_index_capacity
                > (std::numeric_limits<std::size_t>::max)() / 2
                ? required_indices : new_index_capacity * 2;
        }
        for (std::size_t index = 0; index < frame_resources.size(); ++index)
        {
            if (!CreateGizmoBuffers(
                    frame_resources[index], index,
                    new_vertex_capacity, new_index_capacity))
            {
                return false;
            }
        }
        gizmo_vertex_capacity = new_vertex_capacity;
        gizmo_index_capacity = new_index_capacity;
        return true;
    }

    bool RHIRenderer::Impl::CreateObjectConstantBuffer(
        FrameResource& frame_resource,
        std::size_t frame_index,
        std::size_t requested_capacity)
    {
        if (requested_capacity == 0
            || object_constant_stride
                > (std::numeric_limits<std::uint64_t>::max)()
                    / requested_capacity)
        {
            return false;
        }
        const std::string debug_name =
            "Renderer.ObjectConstants." + std::to_string(frame_index);
        return Check(device->CreateBuffer(
                {.size = object_constant_stride * requested_capacity,
                 .usage = RHI::BufferUsage::Constant,
                 .memory = RHI::MemoryType::Upload,
                 .debug_name = debug_name},
                RHI::BufferState::Constant,
                frame_resource.constant_buffer),
                "Create object constant buffer")
            && resource_states.TrackBuffer(
                *frame_resource.constant_buffer, RHI::BufferState::Constant);
    }

    bool RHIRenderer::Impl::EnsureObjectCapacity(std::size_t required_capacity)
    {
        if (required_capacity <= object_capacity)
        {
            return true;
        }
        if (!WaitForAllFrameResources())
        {
            return false;
        }

        std::size_t new_capacity = object_capacity;
        while (new_capacity < required_capacity)
        {
            if (new_capacity > (std::numeric_limits<std::size_t>::max)() / 2u)
            {
                new_capacity = required_capacity;
                break;
            }
            new_capacity *= 2u;
        }

        for (std::size_t index = 0; index < frame_resources.size(); ++index)
        {
            FrameResource& frame_resource = frame_resources[index];
            frame_resource.resource_sets.clear();
            if (frame_resource.constant_buffer != nullptr)
            {
                resource_states.ForgetBuffer(*frame_resource.constant_buffer);
                frame_resource.constant_buffer.reset();
            }
            if (!CreateObjectConstantBuffer(frame_resource, index, new_capacity))
            {
                return false;
            }
        }
        object_capacity = new_capacity;
        return true;
    }

    bool RHIRenderer::Impl::WaitForFrameResource(
        FrameResource& frame_resource) noexcept
    {
        if (frame_resource.fence_value == 0)
        {
            return true;
        }
        if (!Check(fence->Wait(
                frame_resource.fence_value,
                GpuWaitTimeoutMilliseconds),
                "Wait for frame resource"))
        {
            return false;
        }
        frame_resource.fence_value = 0;
        return true;
    }

    bool RHIRenderer::Impl::WaitForAllFrameResources() noexcept
    {
        bool all_completed = true;
        for (FrameResource& frame_resource : frame_resources)
        {
            if (!WaitForFrameResource(frame_resource))
            {
                all_completed = false;
            }
        }
        return all_completed;
    }

    bool RHIRenderer::Impl::TransitionTrackedBuffer(
        RHI::CommandList& command_list,
        RHI::Buffer& buffer,
        RHI::BufferState after,
        const char* operation) noexcept
    {
        RHI::BufferState before;
        if (!resource_states.GetBufferState(buffer, before))
        {
            std::fprintf(stderr, "[Renderer] Untracked buffer transition\n");
            return false;
        }
        if (before == after)
        {
            return true;
        }
        if (!Check(command_list.TransitionBuffer(buffer, before, after), operation)
            || !resource_states.CommitBufferTransition(
                buffer, before, after))
        {
            return false;
        }
        return true;
    }

    bool RHIRenderer::Impl::TransitionTrackedTexture(
        RHI::CommandList& command_list,
        RHI::Texture& texture,
        RHI::TextureState after,
        const char* operation) noexcept
    {
        RHI::TextureState before;
        if (!resource_states.GetTextureState(texture, before))
        {
            std::fprintf(stderr, "[Renderer] Untracked texture transition\n");
            return false;
        }
        if (before == after)
        {
            return true;
        }
        if (!Check(command_list.TransitionTexture(
                texture, before, after), operation)
            || !resource_states.CommitTextureTransition(
                texture, before, after))
        {
            return false;
        }
        return true;
    }

    bool RHIRenderer::Impl::Resize(RenderCore::RenderExtent new_extent)
    {
        if (!WaitForAllFrameResources())
        {
            return false;
        }

        // RTV 借用旧 Back Buffer，必须先释放，再让 Swapchain 更换图片。
        for (std::uint32_t index = 0;
            index < swapchain->GetImageCount(); ++index)
        {
            if (RHI::Texture* image = swapchain->GetImage(index))
            {
                resource_states.ForgetTexture(*image);
            }
        }
        color_views.clear();
        if (!Check(swapchain->Resize(new_extent.width, new_extent.height),
                "Swapchain Resize"))
        {
            return false;
        }
        extent = new_extent;
        return CreateAttachments();
    }

    bool RHIRenderer::Impl::SubmitFrame(RenderCore::RenderFrame frame)
    {
        if (state != State::Initialized)
        {
            return false;
        }
        if (!frame.output_extent.HasArea())
        {
            return true;
        }
        if (frame.output_extent.width != extent.width
            || frame.output_extent.height != extent.height)
        {
            if (!Resize(frame.output_extent))
            {
                state = State::Failed;
                return false;
            }
        }
        if (!EnsureObjectCapacity(frame.render_items.size()))
        {
            state = State::Failed;
            return false;
        }
        if ((!frame.gizmo.vertices.empty() || !frame.gizmo.indices.empty())
            && (!gizmo_enabled
                || frame.gizmo.vertices.empty()
                || frame.gizmo.indices.empty()
                || frame.gizmo.indices.size()
                    > (std::numeric_limits<std::uint32_t>::max)()
                || std::any_of(
                    frame.gizmo.indices.begin(), frame.gizmo.indices.end(),
                    [&frame](std::uint32_t index)
                    { return index >= frame.gizmo.vertices.size(); })))
        {
            std::fprintf(stderr, "[Renderer] Invalid Gizmo render data\n");
            state = State::Failed;
            return false;
        }
        if (!EnsureGizmoCapacity(
                frame.gizmo.vertices.size(), frame.gizmo.indices.size()))
        {
            state = State::Failed;
            return false;
        }
        if (ui_enabled)
        {
            const RenderCore::RenderExtent requested_scene_extent =
                frame.scene_extent.HasArea()
                ? frame.scene_extent : RenderCore::RenderExtent{1, 1};
            if ((requested_scene_extent.width != scene_extent.width
                    || requested_scene_extent.height != scene_extent.height)
                && (!WaitForAllFrameResources()
                    || !CreateSceneAttachments(requested_scene_extent)))
            {
                state = State::Failed;
                return false;
            }
            if (!EnsureUICapacity(
                    frame.ui.vertices.size(), frame.ui.indices.size()))
            {
                state = State::Failed;
                return false;
            }
        }
        FrameResource& frame_resource = frame_resources[next_frame_resource_index];
        if (!WaitForFrameResource(frame_resource))
        {
            state = State::Failed;
            return false;
        }

        const Matrix4 view_projection =
            frame.main_view.view * frame.main_view.projection;

        struct DrawSubmission final
        {
            GpuMesh* mesh = nullptr;
        };
        std::vector<DrawSubmission> submissions;
        submissions.reserve(frame.render_items.size());
        frame_resource.resource_sets.clear();
        frame_resource.resource_sets.reserve(frame.render_items.size());

        for (std::size_t index = 0; index < frame.render_items.size(); ++index)
        {
            const RenderCore::RenderItem& item = frame.render_items[index];
            GpuMesh* mesh = FindMesh(item.mesh);
            GpuMaterial* material = FindMaterial(item.material);
            GpuTexture* texture = material == nullptr
                ? nullptr : FindTexture(material->albedo_texture);
            if (mesh == nullptr || material == nullptr || texture == nullptr)
            {
                std::fprintf(stderr,
                    "[Renderer] RenderItem references an unknown resource\n");
                state = State::Failed;
                return false;
            }

            const std::uint64_t constant_offset = object_constant_stride * index;
            const std::array<RHI::BufferBinding, 1> resources{{
                {.binding = 0,
                 .buffer = frame_resource.constant_buffer.get(),
                 .offset = constant_offset,
                 .size = sizeof(ObjectConstants)}}};
            const std::array<RHI::TextureBinding, 1> texture_bindings{{
                {.binding = 1, .view = texture->view.get()}}};
            const std::array<RHI::SamplerBinding, 1> samplers{{
                {.binding = 2, .sampler = material->sampler.get()}}};
            std::unique_ptr<RHI::ResourceSet> resource_set;
            if (!Check(device->CreateResourceSet(
                    {.layout = pipeline_layout.get(),
                     .buffers = resources,
                     .textures = texture_bindings,
                     .samplers = samplers},
                    resource_set),
                    "Create object resource set"))
            {
                state = State::Failed;
                return false;
            }
            frame_resource.resource_sets.push_back(std::move(resource_set));
            submissions.push_back({mesh});
        }

        if (!frame.render_items.empty())
        {
            const std::uint64_t mapped_size =
                object_constant_stride * frame.render_items.size();
            void* mapped = nullptr;
            if (!Check(frame_resource.constant_buffer->Map(
                    0, mapped_size, mapped),
                    "Map object constants"))
            {
                state = State::Failed;
                return false;
            }
            auto* mapped_bytes = static_cast<std::byte*>(mapped);
            for (std::size_t index = 0; index < frame.render_items.size(); ++index)
            {
                const ObjectConstants constants{
                    .world_view_projection =
                        frame.render_items[index].local_to_world
                            * view_projection,
                    .color_tint = frame.render_items[index].color_tint};
                std::memcpy(
                    mapped_bytes + object_constant_stride * index,
                    &constants,
                    sizeof(constants));
            }
            frame_resource.constant_buffer->Unmap(0, mapped_size);
        }

        if (skybox_enabled)
        {
            Matrix4 rotation_only_view = frame.main_view.view;
            rotation_only_view.At(3, 0) = 0.0F;
            rotation_only_view.At(3, 1) = 0.0F;
            rotation_only_view.At(3, 2) = 0.0F;
            const SkyboxConstants constants{
                .view_projection =
                    rotation_only_view * frame.main_view.projection,
                .intensity = skybox_intensity,
                .exposure = skybox_exposure};
            void* mapped = nullptr;
            if (!Check(frame_resource.skybox_constant_buffer->Map(
                    0, sizeof(constants), mapped), "Map Skybox constants"))
            {
                state = State::Failed;
                return false;
            }
            std::memcpy(mapped, &constants, sizeof(constants));
            frame_resource.skybox_constant_buffer->Unmap(0, sizeof(constants));
        }

        frame_resource.ui_resource_sets.clear();
        if (ui_enabled && !frame.ui.IsEmpty())
        {
            if (frame.ui.display_size[0] <= 0.0F
                || frame.ui.display_size[1] <= 0.0F
                || frame.ui.vertices.size() > ui_vertex_capacity
                || frame.ui.indices.size() > ui_index_capacity)
            {
                state = State::Failed;
                return false;
            }
            const std::uint64_t vertex_bytes = frame.ui.vertices.size()
                * sizeof(RenderCore::UIVertex);
            const std::uint64_t index_bytes = frame.ui.indices.size()
                * sizeof(std::uint32_t);
            void* mapped = nullptr;
            if (!Check(frame_resource.ui_vertex_buffer->Map(
                    0, vertex_bytes, mapped), "Map UI vertices"))
            {
                state = State::Failed;
                return false;
            }
            std::memcpy(mapped, frame.ui.vertices.data(),
                static_cast<std::size_t>(vertex_bytes));
            frame_resource.ui_vertex_buffer->Unmap(0, vertex_bytes);
            if (!Check(frame_resource.ui_index_buffer->Map(
                    0, index_bytes, mapped), "Map UI indices"))
            {
                state = State::Failed;
                return false;
            }
            std::memcpy(mapped, frame.ui.indices.data(),
                static_cast<std::size_t>(index_bytes));
            frame_resource.ui_index_buffer->Unmap(0, index_bytes);

            const UIConstants constants{
                .scale = {
                    2.0F / frame.ui.display_size[0],
                    -2.0F / frame.ui.display_size[1]},
                .translate = {
                    -1.0F - frame.ui.display_position[0]
                        * (2.0F / frame.ui.display_size[0]),
                    1.0F + frame.ui.display_position[1]
                        * (2.0F / frame.ui.display_size[1])}};
            if (!Check(frame_resource.ui_constant_buffer->Map(
                    0, sizeof(constants), mapped), "Map UI constants"))
            {
                state = State::Failed;
                return false;
            }
            std::memcpy(mapped, &constants, sizeof(constants));
            frame_resource.ui_constant_buffer->Unmap(0, sizeof(constants));

            GpuTexture* font = FindTexture(ui_font_texture);
            if (font == nullptr || scene_sampled_view == nullptr)
            {
                state = State::Failed;
                return false;
            }
            const std::array<RHI::SampledTextureView*, 2> views{
                font->view.get(), scene_sampled_view.get()};
            frame_resource.ui_resource_sets.reserve(views.size());
            for (RHI::SampledTextureView* view : views)
            {
                const std::array<RHI::BufferBinding, 1> buffers{{
                    {.binding = 0,
                     .buffer = frame_resource.ui_constant_buffer.get(),
                     .size = sizeof(UIConstants)}}};
                const std::array<RHI::TextureBinding, 1> textures{{
                    {.binding = 1, .view = view}}};
                const std::array<RHI::SamplerBinding, 1> samplers{{
                    {.binding = 2, .sampler = ui_sampler.get()}}};
                std::unique_ptr<RHI::ResourceSet> resource_set;
                if (!Check(device->CreateResourceSet(
                        {.layout = ui_pipeline_layout.get(),
                         .buffers = buffers,
                         .textures = textures,
                         .samplers = samplers},
                        resource_set), "Create UI resource set"))
                {
                    state = State::Failed;
                    return false;
                }
                frame_resource.ui_resource_sets.push_back(
                    std::move(resource_set));
            }
        }

        if (!frame.gizmo.IsEmpty())
        {
            const std::uint64_t vertex_bytes = frame.gizmo.vertices.size()
                * sizeof(RenderCore::GizmoVertex);
            const std::uint64_t index_bytes = frame.gizmo.indices.size()
                * sizeof(std::uint32_t);
            void* mapped = nullptr;
            if (!Check(frame_resource.gizmo_vertex_buffer->Map(
                    0, vertex_bytes, mapped), "Map Gizmo vertices"))
            {
                state = State::Failed;
                return false;
            }
            std::memcpy(mapped, frame.gizmo.vertices.data(),
                static_cast<std::size_t>(vertex_bytes));
            frame_resource.gizmo_vertex_buffer->Unmap(0, vertex_bytes);
            if (!Check(frame_resource.gizmo_index_buffer->Map(
                    0, index_bytes, mapped), "Map Gizmo indices"))
            {
                state = State::Failed;
                return false;
            }
            std::memcpy(mapped, frame.gizmo.indices.data(),
                static_cast<std::size_t>(index_bytes));
            frame_resource.gizmo_index_buffer->Unmap(0, index_bytes);
        }

        const std::uint32_t image_index = swapchain->GetCurrentImageIndex();
        if (image_index >= color_views.size()
            || !Check(frame_resource.command_allocator->Reset(),
                "CommandAllocator Reset")
            || !Check(frame_resource.command_list->Reset(
                *frame_resource.command_allocator),
                "CommandList Reset"))
        {
            state = State::Failed;
            return false;
        }

        RHI::Texture& back_buffer = swapchain->GetCurrentImage();
        RHI::ColorAttachmentView& back_buffer_view = *color_views[image_index];
        RHI::Texture& scene_target = ui_enabled
            ? *scene_color_texture : back_buffer;
        RHI::ColorAttachmentView& scene_target_view = ui_enabled
            ? *scene_color_view : back_buffer_view;
        const RenderCore::RenderExtent active_scene_extent = ui_enabled
            ? scene_extent : extent;
        const RHI::Viewport scene_viewport{
            .width = static_cast<float>(active_scene_extent.width),
            .height = static_cast<float>(active_scene_extent.height)};
        const RHI::ScissorRect scene_scissor{
            .right = static_cast<std::int32_t>(active_scene_extent.width),
            .bottom = static_cast<std::int32_t>(active_scene_extent.height)};
        constexpr RHI::ClearColor clear_color{
            .red = 0.025F, .green = 0.04F, .blue = 0.085F, .alpha = 1.0F};
        RHI::CommandList& command_list = *frame_resource.command_list;

        bool recorded =
            TransitionTrackedTexture(
                command_list,
                scene_target,
                RHI::TextureState::ColorAttachment,
                "Scene target color barrier")
            && Check(command_list.SetColorAttachment(scene_target_view),
                "Set scene color attachment")
            && Check(command_list.SetDepthStencilAttachment(*depth_view),
                "SetDepthStencilAttachment")
            && Check(command_list.SetViewportAndScissor(
                scene_viewport, scene_scissor), "Set scene viewport")
            && Check(command_list.ClearColorAttachment(
                scene_target_view, clear_color),
                "ClearColorAttachment")
            && Check(command_list.ClearDepthStencilAttachment(*depth_view, 1.0F),
                "ClearDepthStencilAttachment")
            && Check(command_list.SetGraphicsPipeline(*pipeline),
                "SetGraphicsPipeline");
        for (std::size_t index = 0; recorded && index < submissions.size(); ++index)
        {
            GpuMesh& mesh = *submissions[index].mesh;
            recorded = Check(command_list.SetResourceSet(
                    *frame_resource.resource_sets[index]), "SetResourceSet")
                && Check(command_list.SetVertexBuffer(
                    *mesh.vertex_buffer, 0, sizeof(PositionColorTextureVertex)),
                    "SetVertexBuffer")
                && Check(command_list.SetIndexBuffer(
                    *mesh.index_buffer, 0, RHI::RHIFormat::R16_UInt),
                    "SetIndexBuffer")
                && Check(command_list.DrawIndexed(mesh.index_count), "DrawIndexed");
        }
        if (recorded && skybox_enabled)
        {
            recorded = Check(command_list.SetGraphicsPipeline(
                    *skybox_pipeline), "Set Skybox pipeline")
                && Check(command_list.SetResourceSet(
                    *frame_resource.skybox_resource_set),
                    "Set Skybox resource set")
                && Check(command_list.SetVertexBuffer(
                    *skybox_vertex_buffer, 0, sizeof(SkyboxVertex)),
                    "Set Skybox vertex buffer")
                && Check(command_list.SetIndexBuffer(
                    *skybox_index_buffer, 0, RHI::RHIFormat::R16_UInt),
                    "Set Skybox index buffer")
                && Check(command_list.DrawIndexed(skybox_index_count),
                    "Draw Skybox");
        }
        if (recorded && !frame.gizmo.IsEmpty())
        {
            const std::uint32_t gizmo_index_count =
                static_cast<std::uint32_t>(frame.gizmo.indices.size());
            recorded = Check(command_list.SetVertexBuffer(
                    *frame_resource.gizmo_vertex_buffer,
                    0,
                    sizeof(RenderCore::GizmoVertex)),
                    "Set Gizmo vertex buffer")
                && Check(command_list.SetIndexBuffer(
                    *frame_resource.gizmo_index_buffer,
                    0,
                    RHI::RHIFormat::R32_UInt),
                    "Set Gizmo index buffer")
                // 两个 Pass 都不写深度，因此只和场景比较，轴之间不会互相遮挡。
                && Check(command_list.SetGraphicsPipeline(
                    *gizmo_occluded_pipeline),
                    "Set occluded Gizmo pipeline")
                && Check(command_list.DrawIndexed(gizmo_index_count),
                    "Draw occluded Gizmo")
                && Check(command_list.SetGraphicsPipeline(
                    *gizmo_visible_pipeline),
                    "Set visible Gizmo pipeline")
                && Check(command_list.DrawIndexed(gizmo_index_count),
                    "Draw visible Gizmo");
        }
        if (recorded && ui_enabled)
        {
            recorded = TransitionTrackedTexture(
                    command_list,
                    scene_target,
                    RHI::TextureState::ShaderRead,
                    "Scene color-to-shader barrier")
                && TransitionTrackedTexture(
                    command_list,
                    back_buffer,
                    RHI::TextureState::ColorAttachment,
                    "Present-to-UI barrier")
                && Check(command_list.SetColorAttachmentOnly(back_buffer_view),
                    "Set UI color attachment");

            constexpr RHI::ClearColor ui_clear{
                .red = 0.035F, .green = 0.038F,
                .blue = 0.048F, .alpha = 1.0F};
            const RHI::Viewport ui_viewport{
                .width = static_cast<float>(extent.width),
                .height = static_cast<float>(extent.height)};
            const RHI::ScissorRect ui_scissor{
                .right = static_cast<std::int32_t>(extent.width),
                .bottom = static_cast<std::int32_t>(extent.height)};
            recorded = recorded
                && Check(command_list.SetViewportAndScissor(
                    ui_viewport, ui_scissor), "Set UI viewport")
                && Check(command_list.ClearColorAttachment(
                    back_buffer_view, ui_clear), "Clear UI target");

            if (recorded && !frame.ui.IsEmpty())
            {
                recorded = Check(command_list.SetGraphicsPipeline(*ui_pipeline),
                        "Set UI graphics pipeline")
                    && Check(command_list.SetVertexBuffer(
                        *frame_resource.ui_vertex_buffer,
                        0,
                        sizeof(RenderCore::UIVertex)), "Set UI vertex buffer")
                    && Check(command_list.SetIndexBuffer(
                        *frame_resource.ui_index_buffer,
                        0,
                        RHI::RHIFormat::R32_UInt), "Set UI index buffer");
                for (const RenderCore::UIDrawCommand& draw : frame.ui.commands)
                {
                    if (!recorded)
                    {
                        break;
                    }
                    const float clip_left =
                        (draw.clip_rectangle[0]
                            - frame.ui.display_position[0])
                        * frame.ui.framebuffer_scale[0];
                    const float clip_top =
                        (draw.clip_rectangle[1]
                            - frame.ui.display_position[1])
                        * frame.ui.framebuffer_scale[1];
                    const float clip_right =
                        (draw.clip_rectangle[2]
                            - frame.ui.display_position[0])
                        * frame.ui.framebuffer_scale[0];
                    const float clip_bottom =
                        (draw.clip_rectangle[3]
                            - frame.ui.display_position[1])
                        * frame.ui.framebuffer_scale[1];
                    const RHI::ScissorRect draw_scissor{
                        .left = static_cast<std::int32_t>((std::max)(0.0F, clip_left)),
                        .top = static_cast<std::int32_t>((std::max)(0.0F, clip_top)),
                        .right = static_cast<std::int32_t>((std::min)(
                            static_cast<float>(extent.width), clip_right)),
                        .bottom = static_cast<std::int32_t>((std::min)(
                            static_cast<float>(extent.height), clip_bottom))};
                    if (draw_scissor.left >= draw_scissor.right
                        || draw_scissor.top >= draw_scissor.bottom)
                    {
                        continue;
                    }
                    const std::size_t resource_index =
                        draw.texture == RenderCore::UITexture::SceneViewport
                        ? 1 : 0;
                    if (resource_index
                            >= frame_resource.ui_resource_sets.size()
                        || draw.first_index > frame.ui.indices.size()
                        || draw.index_count
                            > frame.ui.indices.size() - draw.first_index)
                    {
                        recorded = false;
                        break;
                    }
                    recorded = Check(command_list.SetViewportAndScissor(
                            ui_viewport, draw_scissor), "Set UI scissor")
                        && Check(command_list.SetResourceSet(
                            *frame_resource.ui_resource_sets[resource_index]),
                            "Set UI resource set")
                        && Check(command_list.DrawIndexed(
                            draw.index_count,
                            1,
                            draw.first_index,
                            draw.vertex_offset), "Draw UI");
                }
            }
            recorded = recorded
                && TransitionTrackedTexture(
                    command_list,
                    back_buffer,
                    RHI::TextureState::Present,
                    "UI-to-present barrier");
        }
        else if (recorded)
        {
            recorded = TransitionTrackedTexture(
                command_list,
                back_buffer,
                RHI::TextureState::Present,
                "Color-to-present barrier");
        }
        recorded = recorded
            && Check(command_list.Close(), "CommandList Close");
        if (!recorded)
        {
            state = State::Failed;
            return false;
        }

        if (last_signaled_fence_value
            == (std::numeric_limits<std::uint64_t>::max)())
        {
            std::fprintf(stderr, "[Renderer] Fence value exhausted\n");
            state = State::Failed;
            return false;
        }

        RHI::Queue& graphics_queue = device->GetGraphicsQueue();
        if (!Check(graphics_queue.Execute(command_list), "Queue Execute"))
        {
            state = State::Failed;
            return false;
        }

        const bool presented = Check(swapchain->Present(), "Swapchain Present");
        const std::uint64_t submitted_fence_value =
            last_signaled_fence_value + 1;
        const bool signaled = Check(graphics_queue.Signal(
            *fence, submitted_fence_value), "Queue Signal");
        if (signaled)
        {
            last_signaled_fence_value = submitted_fence_value;
            frame_resource.fence_value = submitted_fence_value;
        }
        if (!presented || !signaled)
        {
            state = State::Failed;
            return false;
        }

        next_frame_resource_index =
            (next_frame_resource_index + 1) % frame_resources.size();
        return true;
    }

    void RHIRenderer::Impl::ReleaseResources() noexcept
    {
        resource_states.Reset();
        for (FrameResource& frame_resource : frame_resources)
        {
            frame_resource.command_list.reset();
            frame_resource.command_allocator.reset();
            frame_resource.resource_sets.clear();
            frame_resource.ui_resource_sets.clear();
            frame_resource.skybox_resource_set.reset();
            frame_resource.constant_buffer.reset();
            frame_resource.skybox_constant_buffer.reset();
            frame_resource.ui_index_buffer.reset();
            frame_resource.ui_vertex_buffer.reset();
            frame_resource.ui_constant_buffer.reset();
            frame_resource.gizmo_index_buffer.reset();
            frame_resource.gizmo_vertex_buffer.reset();
            frame_resource.fence_value = 0;
        }
        next_frame_resource_index = 0;
        skybox_pipeline.reset();
        skybox_pipeline_layout.reset();
        skybox_pixel_shader.reset();
        skybox_vertex_shader.reset();
        skybox_sampler.reset();
        skybox_index_buffer.reset();
        skybox_vertex_buffer.reset();
        skybox_index_count = 0;
        gizmo_occluded_pipeline.reset();
        gizmo_visible_pipeline.reset();
        gizmo_pipeline_layout.reset();
        gizmo_occluded_pixel_shader.reset();
        gizmo_visible_pixel_shader.reset();
        gizmo_vertex_shader.reset();
        ui_pipeline.reset();
        ui_pipeline_layout.reset();
        ui_pixel_shader.reset();
        ui_vertex_shader.reset();
        ui_sampler.reset();
        pipeline.reset();
        pipeline_layout.reset();
        pixel_shader.reset();
        vertex_shader.reset();
        // ResourceSet 已释放；随后先释放 Material/Sampler，再释放 Texture View/Texture。
        materials.clear();
        textures.clear();
        meshes.clear();
        object_constant_stride = 0;
        object_capacity = 0;
        ui_vertex_capacity = 0;
        ui_index_capacity = 0;
        gizmo_vertex_capacity = 0;
        gizmo_index_capacity = 0;
        scene_sampled_view.reset();
        scene_color_view.reset();
        depth_view.reset();
        scene_color_texture.reset();
        depth_texture.reset();
        scene_extent = {};
        color_views.clear();
        swapchain.reset();
        fence.reset();
        device.reset();
    }

    void RHIRenderer::Impl::Shutdown() noexcept
    {
        if (state == State::Shutdown)
        {
            return;
        }
        if (fence != nullptr && !WaitForAllFrameResources())
        {
            std::fprintf(stderr,
                "[Renderer] GPU did not finish before renderer shutdown\n");
        }
        ReleaseResources();
        backend.reset();
        state = State::Shutdown;
        std::fprintf(stdout, "[Renderer] RHI renderer shutdown\n");
    }

    RHIRenderer::RHIRenderer(RHIRendererCreateInfo create_info)
        : m_impl(std::make_unique<Impl>(std::move(create_info)))
    {
    }

    RHIRenderer::~RHIRenderer() noexcept
    {
        Shutdown();
    }

    bool RHIRenderer::Initialize()
    {
        return m_impl != nullptr && m_impl->Initialize();
    }

    bool RHIRenderer::SubmitFrame(RenderCore::RenderFrame frame)
    {
        return m_impl != nullptr && m_impl->SubmitFrame(std::move(frame));
    }

    void RHIRenderer::Shutdown() noexcept
    {
        if (m_impl != nullptr)
        {
            m_impl->Shutdown();
        }
    }
}
