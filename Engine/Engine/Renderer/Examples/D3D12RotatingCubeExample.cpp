#include "D3D12RotatingCubeExample.h"

#include <Engine/Core/Math/angle.h>
#include <Engine/Core/Math/matrix4.h>
#include <Engine/Core/Math/vector.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_HDR
#include <ThirdParty/stb/stb_image.h>

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace GE::Render::Examples
{
    namespace
    {
        constexpr RenderCore::MeshHandle CubeMesh{1};
        constexpr RenderCore::TextureHandle CheckerboardTexture{1};
        constexpr RenderCore::TextureHandle SkyboxTexture{2};
        constexpr RenderCore::MaterialHandle CubeMaterial{1};

        [[nodiscard]] RenderCore::RenderMatrix4 MakeViewMatrix() noexcept
        {
            // 左手坐标系：相机位于 z=-5，因此把世界沿 +z 变换到观察空间。
            return Math::TranslationMatrix({0.0F, 0.0F, 5.0F});
        }

        [[nodiscard]] RenderCore::RenderMatrix4 MakeProjectionMatrix(
            float aspect) noexcept
        {
            constexpr float NearPlane = 0.1F;
            constexpr float FarPlane = 100.0F;
            const float safe_aspect = aspect > 0.0F ? aspect : 1.0F;
            return Math::PerspectiveFovLH_ZO(
                Math::Radians{Math::Pi / 3.0F},
                safe_aspect,
                NearPlane,
                FarPlane);
        }

        [[nodiscard]] bool CompileShader(
            const std::filesystem::path& path,
            const char* entry_point,
            const char* target,
            std::vector<std::byte>& out_bytecode,
            std::string& out_error)
        {
            out_bytecode.clear();
            std::error_code path_error;
            if (!std::filesystem::is_regular_file(path, path_error))
            {
                out_error = "Shader file not found: " + path.string();
                return false;
            }
            Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
            Microsoft::WRL::ComPtr<ID3DBlob> errors;
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            const HRESULT result = D3DCompileFromFile(
                path.c_str(),
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entry_point,
                target,
                flags,
                0,
                bytecode.ReleaseAndGetAddressOf(),
                errors.ReleaseAndGetAddressOf());
            if (FAILED(result))
            {
                if (errors != nullptr)
                {
                    const char* text = static_cast<const char*>(
                        errors->GetBufferPointer());
                    out_error.assign(text, errors->GetBufferSize());
                }
                else
                {
                    out_error = "Shader compile failed: " + path.string();
                }
                return false;
            }

            const auto* begin = static_cast<const std::byte*>(
                bytecode->GetBufferPointer());
            out_bytecode.assign(begin, begin + bytecode->GetBufferSize());
            return true;
        }

        [[nodiscard]] bool LoadPiccoloSkybox(
            const std::filesystem::path& asset_directory,
            RHITextureDescription& out_texture,
            std::string& out_error)
        {
            constexpr std::array<const char*, 6> face_names{{
                "skybox_specular_X+.hdr",
                "skybox_specular_X-.hdr",
                "skybox_specular_Y+.hdr",
                "skybox_specular_Y-.hdr",
                "skybox_specular_Z+.hdr",
                "skybox_specular_Z-.hdr"
            }};
            const std::filesystem::path sky_directory =
                asset_directory / "Textures" / "Sky" / "Piccolo";
            int expected_width = 0;
            int expected_height = 0;
            out_texture = {};
            out_texture.handle = SkyboxTexture;
            out_texture.array_layers = 6;
            out_texture.mip_levels = 1;
            out_texture.format = RHI::RHIFormat::R32G32B32A32_Float;
            out_texture.view.dimension =
                RHI::SampledTextureViewDimension::TextureCube;

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
                    stbi_loadf(path.string().c_str(),
                        &width, &height, &channels, STBI_rgb_alpha),
                    &stbi_image_free);
                if (pixels == nullptr || width <= 0 || height <= 0)
                {
                    const char* reason = stbi_failure_reason();
                    out_error = "Failed to load HDR skybox face: "
                        + path.string();
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
                    out_error = "Skybox faces must have identical dimensions: "
                        + path.string();
                    return false;
                }
                if (width != height)
                {
                    out_error = "Skybox faces must be square: " + path.string();
                    return false;
                }

                const std::size_t float_count =
                    static_cast<std::size_t>(width)
                    * static_cast<std::size_t>(height) * 4u;
                const std::size_t byte_count = float_count * sizeof(float);
                const std::size_t old_size = out_texture.pixels.size();
                out_texture.pixels.resize(old_size + byte_count);
                std::memcpy(
                    out_texture.pixels.data() + old_size,
                    pixels.get(),
                    byte_count);
            }

            out_texture.width = static_cast<std::uint32_t>(expected_width);
            out_texture.height = static_cast<std::uint32_t>(expected_height);
            return true;
        }
    }

    bool BuildD3D12RotatingCubeScene(
        const std::filesystem::path& shader_directory,
        RHIRenderScene& out_scene,
        std::string& out_error) noexcept
    {
        out_scene = {};
        out_error.clear();

        // 每个面独占 4 个顶点，因为同一空间角点在不同面上需要不同 UV。
        constexpr std::array<PositionColorTextureVertex, 24> vertices{{
            // 后面
            {{{-1.0F, -1.0F, -1.0F}}, {{0.95F, 0.35F, 0.35F}}, {{0.0F, 1.0F}}},
            {{{-1.0F,  1.0F, -1.0F}}, {{0.35F, 0.95F, 0.45F}}, {{0.0F, 0.0F}}},
            {{{ 1.0F,  1.0F, -1.0F}}, {{0.35F, 0.50F, 1.00F}}, {{1.0F, 0.0F}}},
            {{{ 1.0F, -1.0F, -1.0F}}, {{1.00F, 0.80F, 0.35F}}, {{1.0F, 1.0F}}},
            // 前面
            {{{-1.0F, -1.0F,  1.0F}}, {{0.95F, 0.35F, 0.35F}}, {{0.0F, 1.0F}}},
            {{{ 1.0F, -1.0F,  1.0F}}, {{1.00F, 0.80F, 0.35F}}, {{1.0F, 1.0F}}},
            {{{ 1.0F,  1.0F,  1.0F}}, {{0.35F, 0.50F, 1.00F}}, {{1.0F, 0.0F}}},
            {{{-1.0F,  1.0F,  1.0F}}, {{0.35F, 0.95F, 0.45F}}, {{0.0F, 0.0F}}},
            // 左面
            {{{-1.0F, -1.0F,  1.0F}}, {{0.95F, 0.35F, 0.35F}}, {{0.0F, 1.0F}}},
            {{{-1.0F,  1.0F,  1.0F}}, {{0.35F, 0.95F, 0.45F}}, {{0.0F, 0.0F}}},
            {{{-1.0F,  1.0F, -1.0F}}, {{0.35F, 0.50F, 1.00F}}, {{1.0F, 0.0F}}},
            {{{-1.0F, -1.0F, -1.0F}}, {{1.00F, 0.80F, 0.35F}}, {{1.0F, 1.0F}}},
            // 右面
            {{{ 1.0F, -1.0F, -1.0F}}, {{0.95F, 0.35F, 0.35F}}, {{0.0F, 1.0F}}},
            {{{ 1.0F,  1.0F, -1.0F}}, {{0.35F, 0.95F, 0.45F}}, {{0.0F, 0.0F}}},
            {{{ 1.0F,  1.0F,  1.0F}}, {{0.35F, 0.50F, 1.00F}}, {{1.0F, 0.0F}}},
            {{{ 1.0F, -1.0F,  1.0F}}, {{1.00F, 0.80F, 0.35F}}, {{1.0F, 1.0F}}},
            // 上面
            {{{-1.0F,  1.0F, -1.0F}}, {{0.95F, 0.35F, 0.35F}}, {{0.0F, 1.0F}}},
            {{{-1.0F,  1.0F,  1.0F}}, {{0.35F, 0.95F, 0.45F}}, {{0.0F, 0.0F}}},
            {{{ 1.0F,  1.0F,  1.0F}}, {{0.35F, 0.50F, 1.00F}}, {{1.0F, 0.0F}}},
            {{{ 1.0F,  1.0F, -1.0F}}, {{1.00F, 0.80F, 0.35F}}, {{1.0F, 1.0F}}},
            // 下面
            {{{-1.0F, -1.0F,  1.0F}}, {{0.95F, 0.35F, 0.35F}}, {{0.0F, 1.0F}}},
            {{{-1.0F, -1.0F, -1.0F}}, {{0.35F, 0.95F, 0.45F}}, {{0.0F, 0.0F}}},
            {{{ 1.0F, -1.0F, -1.0F}}, {{0.35F, 0.50F, 1.00F}}, {{1.0F, 0.0F}}},
            {{{ 1.0F, -1.0F,  1.0F}}, {{1.00F, 0.80F, 0.35F}}, {{1.0F, 1.0F}}}
        }};
        constexpr std::array<std::uint16_t, 36> indices{{
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            8, 9, 10, 8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
        }};
        try
        {
            const std::filesystem::path shader_path =
                shader_directory / "examples" / "rotating_cube.hlsl";
            const std::filesystem::path skybox_shader_path =
                shader_directory / "examples" / "skybox.hlsl";
            const std::filesystem::path asset_directory =
                shader_directory.parent_path() / "Assets";

            RHIMeshDescription mesh;
            mesh.handle = CubeMesh;
            mesh.vertices.assign(vertices.begin(), vertices.end());
            mesh.indices.assign(indices.begin(), indices.end());
            out_scene.meshes.push_back(std::move(mesh));

            RHITextureDescription texture;
            texture.handle = CheckerboardTexture;
            texture.width = 2;
            texture.height = 2;
            texture.format = RHI::RHIFormat::R8G8B8A8_UNorm;
            texture.pixels = {
                std::byte{255}, std::byte{  0}, std::byte{  0}, std::byte{255},
                std::byte{  0}, std::byte{255}, std::byte{  0}, std::byte{255},
                std::byte{  0}, std::byte{  0}, std::byte{255}, std::byte{255},
                std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}};
            out_scene.textures.push_back(std::move(texture));

            RHITextureDescription skybox_texture;
            if (!LoadPiccoloSkybox(
                    asset_directory, skybox_texture, out_error))
            {
                out_scene = {};
                return false;
            }
            out_scene.textures.push_back(std::move(skybox_texture));

            out_scene.materials.push_back({
                .handle = CubeMaterial,
                .albedo_texture = CheckerboardTexture,
                .sampler = {
                    .filter = RHI::SamplerFilter::Nearest,
                    .address_u = RHI::SamplerAddressMode::Repeat,
                    .address_v = RHI::SamplerAddressMode::Repeat,
                    .address_w = RHI::SamplerAddressMode::Repeat}});
            out_scene.shader_format = RHI::ShaderBytecodeFormat::D3DBytecode;

            if (!CompileShader(shader_path, "VSMain", "vs_5_0",
                    out_scene.vertex_shader, out_error)
                || !CompileShader(shader_path, "PSMain", "ps_5_0",
                    out_scene.pixel_shader, out_error))
            {
                out_scene = {};
                return false;
            }

            RHISkyboxDescription skybox;
            skybox.cubemap_texture = SkyboxTexture;
            skybox.sampler = {
                .filter = RHI::SamplerFilter::Linear,
                .address_u = RHI::SamplerAddressMode::ClampToEdge,
                .address_v = RHI::SamplerAddressMode::ClampToEdge,
                .address_w = RHI::SamplerAddressMode::ClampToEdge};
            skybox.intensity = 1.0F;
            skybox.exposure = 1.0F;
            if (!CompileShader(skybox_shader_path, "SkyboxVS", "vs_5_0",
                    skybox.vertex_shader, out_error)
                || !CompileShader(skybox_shader_path, "SkyboxPS", "ps_5_0",
                    skybox.pixel_shader, out_error))
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

    void BuildRotatingCubeRenderFrame(
        float elapsed_seconds,
        RenderCore::RenderFrame& frame) noexcept
    {
        const float aspect = frame.output_extent.HasArea()
            ? static_cast<float>(frame.output_extent.width)
                / static_cast<float>(frame.output_extent.height)
            : 1.0F;
        frame.main_view = {
            .view = MakeViewMatrix(),
            .projection = MakeProjectionMatrix(aspect)};

        const auto make_transform = [](float angle, float x) noexcept
        {
            // row-vector 约定：平移位于最后一行，与 HLSL mul(position, matrix) 一致。
            return Math::RotationYMatrix(Math::Radians{angle})
                * Math::TranslationMatrix({x, 0.0F, 0.0F});
        };

        try
        {
            frame.render_items.reserve(2);
            frame.render_items.push_back({
                .mesh = CubeMesh,
                .material = CubeMaterial,
                .local_to_world = make_transform(elapsed_seconds * 0.75F, -1.35F),
                .color_tint = {1.0F, 0.82F, 0.82F, 1.0F}});
            frame.render_items.push_back({
                .mesh = CubeMesh,
                .material = CubeMaterial,
                .local_to_world = make_transform(-elapsed_seconds * 0.55F, 1.35F),
                .color_tint = {0.78F, 0.88F, 1.0F, 1.0F}});
        }
        catch (...)
        {
            // noexcept 边界：分配失败时提交空列表，Renderer 仍可安全清屏并 Present。
            frame.render_items.clear();
        }
    }
}
