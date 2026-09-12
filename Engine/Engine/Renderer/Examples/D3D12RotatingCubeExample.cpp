#include "D3D12RotatingCubeExample.h"

#include <Engine/Core/Math/angle.h>
#include <Engine/Core/Math/matrix4.h>
#include <Engine/Core/Math/vector.h>

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <string>
#include <utility>

namespace GE::Render::Examples
{
    namespace
    {
        constexpr RenderCore::MeshHandle CubeMesh{1};
        constexpr RenderCore::TextureHandle CheckerboardTexture{1};
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
            const char* source,
            const char* entry_point,
            const char* target,
            std::vector<std::byte>& out_bytecode,
            std::string& out_error) noexcept
        {
            out_bytecode.clear();
            Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
            Microsoft::WRL::ComPtr<ID3DBlob> errors;
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            const HRESULT result = D3DCompile(
                source,
                std::strlen(source),
                "MEngineRotatingCube.hlsl",
                nullptr,
                nullptr,
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
                    out_error = "D3DCompile failed without diagnostics";
                }
                return false;
            }

            try
            {
                const auto* begin = static_cast<const std::byte*>(
                    bytecode->GetBufferPointer());
                out_bytecode.assign(begin, begin + bytecode->GetBufferSize());
            }
            catch (const std::exception& exception)
            {
                out_error = exception.what();
                return false;
            }
            return true;
        }
    }

    bool BuildD3D12RotatingCubeScene(
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
        constexpr const char* shader_source = R"(
            cbuffer FrameConstants : register(b0)
            {
                row_major float4x4 world_view_projection;
                float4 color_tint;
            };

            Texture2D<float4> albedo_texture : register(t1);
            SamplerState albedo_sampler : register(s2);

            struct VertexInput
            {
                float3 position : ATTRIBUTE0;
                float3 color : ATTRIBUTE1;
                float2 texture_coordinate : ATTRIBUTE2;
            };

            struct VertexOutput
            {
                float4 position : SV_Position;
                float3 color : COLOR0;
                float2 texture_coordinate : TEXCOORD0;
            };

            VertexOutput VSMain(VertexInput input)
            {
                VertexOutput output;
                output.position = mul(
                    float4(input.position, 1.0f), world_view_projection);
                output.color = input.color;
                output.texture_coordinate = input.texture_coordinate;
                return output;
            }

            float4 PSMain(VertexOutput input) : SV_Target0
            {
                float4 sampled = albedo_texture.Sample(
                    albedo_sampler, input.texture_coordinate);
                float3 tint = lerp(float3(1.0f, 1.0f, 1.0f), input.color, 0.35f);
                return sampled * float4(tint, 1.0f) * color_tint;
            }
        )";

        try
        {
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

            out_scene.materials.push_back({
                .handle = CubeMaterial,
                .albedo_texture = CheckerboardTexture,
                .sampler = {
                    .filter = RHI::SamplerFilter::Nearest,
                    .address_u = RHI::SamplerAddressMode::Repeat,
                    .address_v = RHI::SamplerAddressMode::Repeat,
                    .address_w = RHI::SamplerAddressMode::Repeat}});
            out_scene.shader_format = RHI::ShaderBytecodeFormat::D3DBytecode;
        }
        catch (const std::exception& exception)
        {
            out_error = exception.what();
            out_scene = {};
            return false;
        }

        if (!CompileShader(shader_source, "VSMain", "vs_5_0",
                out_scene.vertex_shader, out_error)
            || !CompileShader(shader_source, "PSMain", "ps_5_0",
                out_scene.pixel_shader, out_error))
        {
            out_scene = {};
            return false;
        }
        return true;
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
