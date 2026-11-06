#include <Renderer/ShaderLibrary.h>
#include "SceneResources.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_HDR
#include <ThirdParty/stb/stb_image.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace GE::Editor
{
    using namespace GE::Render;
    namespace
    {
        constexpr RenderCore::TextureHandle SkyboxTexture{0xFFFF'FFE0U};

        using GE::Render::LoadShaderBytecode;

        [[nodiscard]] bool LoadGeneratedSkybox(const std::filesystem::path& asset_directory,
                                               RHITextureDescription& out_texture, std::string& out_error)
        {
            constexpr std::array<const char*, 6> face_names{
                {"skybox_X+.hdr", "skybox_X-.hdr", "skybox_Y+.hdr", "skybox_Y-.hdr", "skybox_Z+.hdr", "skybox_Z-.hdr"}};
            const std::filesystem::path sky_directory = asset_directory / "Textures" / "Sky" / "GeneratedDayBlack_v1";
            int expected_width = 0;
            int expected_height = 0;
            out_texture = {};
            out_texture.handle = SkyboxTexture;
            out_texture.array_layers = 6;
            out_texture.mip_levels = 1;
            out_texture.format = RHI::RHIFormat::R32G32B32A32_Float;
            out_texture.view.dimension = RHI::SampledTextureViewDimension::TextureCube;

            for (const char* face_name : face_names)
            {
                const std::filesystem::path path = sky_directory / face_name;
                std::error_code path_error;
                if (!std::filesystem::is_regular_file(path, path_error))
                {
                    out_error = "Skybox face not found: " + path.string();
                    return false;
                }
                int width = 0;
                int height = 0;
                int channels = 0;
                std::unique_ptr<float, decltype(&stbi_image_free)> pixels(
                    stbi_loadf(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha), &stbi_image_free);
                if (pixels == nullptr || width <= 0 || height <= 0)
                {
                    const char* reason = stbi_failure_reason();
                    out_error = "Failed to load HDR skybox face: " + path.string();
                    if (reason != nullptr)
                    {
                        out_error += " (";
                        out_error += reason;
                        out_error += ')';
                    }
                    return false;
                }
                if (expected_width == 0)
                {
                    expected_width = width;
                    expected_height = height;
                }
                else if (width != expected_width || height != expected_height)
                {
                    out_error = "Skybox faces must have identical dimensions: " + path.string();
                    return false;
                }
                if (width != height)
                {
                    out_error = "Skybox faces must be square: " + path.string();
                    return false;
                }

                const std::size_t float_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
                const std::size_t byte_count = float_count * sizeof(float);
                const std::size_t old_size = out_texture.pixels.size();
                out_texture.pixels.resize(old_size + byte_count);
                std::memcpy(out_texture.pixels.data() + old_size, pixels.get(), byte_count);
            }

            out_texture.width = static_cast<std::uint32_t>(expected_width);
            out_texture.height = static_cast<std::uint32_t>(expected_height);
            return true;
        }
    } // namespace

    bool BuildEditorScene(const std::filesystem::path& shader_directory, RHIRenderScene& out_scene,
                          std::string& out_error) noexcept
    {
        out_scene = {};
        out_error.clear();

        try
        {
            const std::filesystem::path shader_path = shader_directory / "examples" / "rotating_cube.hlsl";
            const std::filesystem::path skybox_shader_path = shader_directory / "examples" / "skybox.hlsl";
            const std::filesystem::path gpu_culling_shader_path = shader_directory / "gpu_scene_culling.hlsl";
            const std::filesystem::path asset_directory = shader_directory.parent_path() / "Assets";

            RHITextureDescription skybox_texture;
            if (!LoadGeneratedSkybox(asset_directory, skybox_texture, out_error))
            {
                out_scene = {};
                return false;
            }
            out_scene.textures.push_back(std::move(skybox_texture));

            if (!LoadShaderBytecode(shader_path, "VSMain", out_scene.vertex_shader, out_error) ||
                !LoadShaderBytecode(shader_path, "PSMain", out_scene.pixel_shader, out_error))
            {
                out_scene = {};
                return false;
            }

            RHIGpuDrivenDescription gpu_driven;
            if (!LoadShaderBytecode(gpu_culling_shader_path, "CSMain", gpu_driven.culling_compute_shader, out_error))
            {
                out_scene = {};
                return false;
            }
            out_scene.gpu_driven = std::move(gpu_driven);

            RHIMotionVectorVisualizationDescription motion;
            const auto motion_shader = shader_directory / "motion_vector_visualization.hlsl";
            if (!LoadShaderBytecode(motion_shader, "FullscreenVS", motion.vertex_shader, out_error) ||
                !LoadShaderBytecode(motion_shader, "VisualizePS", motion.pixel_shader, out_error))
            {
                out_scene = {};
                return false;
            }
            out_scene.motion_vector_visualization = std::move(motion);

            RHIPostProcessDescription post;
            const auto post_shader = shader_directory / "temporal_aa.hlsl";
            if (!LoadShaderBytecode(post_shader, "FullscreenVS", post.vertex_shader, out_error) ||
                !LoadShaderBytecode(post_shader, "ResolvePS", post.taa_pixel_shader, out_error) ||
                !LoadShaderBytecode(post_shader, "DisplayPS", post.display_pixel_shader, out_error) ||
                !LoadShaderBytecode(post_shader, "ToneMapPS", post.tone_map_pixel_shader, out_error))
            {
                out_scene = {};
                return false;
            }
            out_scene.post_process = std::move(post);

            RHILightingDescription lighting;
            const auto environment_shader = shader_directory / "environment.hlsl";
            if (!LoadShaderBytecode(environment_shader, "IrradianceCS", lighting.irradiance_shader, out_error) ||
                !LoadShaderBytecode(environment_shader, "PrefilterCS", lighting.prefilter_shader, out_error) ||
                !LoadShaderBytecode(environment_shader, "IntegrateBRDFCS", lighting.brdf_shader, out_error) ||
                !LoadShaderBytecode(shader_path, "ShadowVS", lighting.shadow_vertex_shader, out_error) ||
                !LoadShaderBytecode(shader_path, "ShadowPS", lighting.shadow_pixel_shader, out_error))
                return false;
            out_scene.lighting = std::move(lighting);

            RHISkyboxDescription skybox;
            skybox.cubemap_texture = SkyboxTexture;
            skybox.sampler = {.filter = RHI::SamplerFilter::Linear,
                              .address_u = RHI::SamplerAddressMode::ClampToEdge,
                              .address_v = RHI::SamplerAddressMode::ClampToEdge,
                              .address_w = RHI::SamplerAddressMode::ClampToEdge};
            skybox.intensity = 1.0F;
            skybox.exposure = 1.0F;
            if (!LoadShaderBytecode(skybox_shader_path, "SkyboxVS", skybox.vertex_shader, out_error) ||
                !LoadShaderBytecode(skybox_shader_path, "SkyboxPS", skybox.pixel_shader, out_error))
            {
                out_scene = {};
                return false;
            }
            out_scene.skybox = std::move(skybox);
            return true;
        }
        catch (const std::exception& exception)
        {
            out_error = exception.what();
            out_scene = {};
            return false;
        }
    }

} // namespace GE::Editor
