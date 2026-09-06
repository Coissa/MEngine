#include "D3D12EditorUIResources.h"

#include "EditorUserInterface.h"

#include <Engine/Renderer/RHIRenderer.h>

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cstring>
#include <exception>

namespace GE::Editor
{
    namespace
    {
        constexpr GE::RenderCore::TextureHandle FontAtlasTexture{0xFFFF'FFF0U};

        bool Compile(
            const char* source,
            const char* entry,
            const char* target,
            std::vector<std::byte>& out_bytecode,
            std::string& out_error)
        {
            Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
            Microsoft::WRL::ComPtr<ID3DBlob> diagnostics;
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            const HRESULT result = D3DCompile(
                source, std::strlen(source), "MEngineEditorUI.hlsl",
                nullptr, nullptr, entry, target, flags, 0,
                bytecode.ReleaseAndGetAddressOf(),
                diagnostics.ReleaseAndGetAddressOf());
            if (FAILED(result))
            {
                out_error = diagnostics == nullptr
                    ? "D3DCompile failed without diagnostics"
                    : std::string(
                        static_cast<const char*>(diagnostics->GetBufferPointer()),
                        diagnostics->GetBufferSize());
                return false;
            }
            const auto* begin = static_cast<const std::byte*>(
                bytecode->GetBufferPointer());
            out_bytecode.assign(begin, begin + bytecode->GetBufferSize());
            return true;
        }
    }

    bool AddD3D12EditorUIResources(
        const EditorFontAtlas& font_atlas,
        GE::Render::RHIRenderScene& scene,
        std::string& out_error) noexcept
    {
        out_error.clear();
        if (font_atlas.width == 0 || font_atlas.height == 0
            || font_atlas.rgba_pixels.size()
                != static_cast<std::size_t>(font_atlas.width)
                    * font_atlas.height * 4)
        {
            out_error = "Invalid editor font atlas";
            return false;
        }

        constexpr const char* Shader = R"(
            cbuffer UIConstants : register(b0)
            {
                float2 scale;
                float2 translate;
            };

            Texture2D<float4> ui_texture : register(t1);
            SamplerState ui_sampler : register(s2);

            struct VertexInput
            {
                float2 position : ATTRIBUTE0;
                float2 uv : ATTRIBUTE1;
                float4 color : ATTRIBUTE2;
            };

            struct VertexOutput
            {
                float4 position : SV_Position;
                float2 uv : TEXCOORD0;
                float4 color : COLOR0;
            };

            VertexOutput VSMain(VertexInput input)
            {
                VertexOutput output;
                output.position = float4(
                    input.position * scale + translate, 0.0f, 1.0f);
                output.uv = input.uv;
                output.color = input.color;
                return output;
            }

            float4 PSMain(VertexOutput input) : SV_Target0
            {
                return input.color * ui_texture.Sample(ui_sampler, input.uv);
            }
        )";

        const std::size_t original_texture_count = scene.textures.size();
        try
        {
            GE::Render::RHITextureDescription texture;
            texture.handle = FontAtlasTexture;
            texture.width = font_atlas.width;
            texture.height = font_atlas.height;
            texture.format = GE::RHI::RHIFormat::R8G8B8A8_UNorm;
            texture.pixels = font_atlas.rgba_pixels;
            scene.textures.push_back(std::move(texture));

            GE::Render::RHIUICompositionDescription composition;
            composition.font_atlas_texture = FontAtlasTexture;
            composition.sampler = {
                .filter = GE::RHI::SamplerFilter::Linear,
                .address_u = GE::RHI::SamplerAddressMode::ClampToEdge,
                .address_v = GE::RHI::SamplerAddressMode::ClampToEdge,
                .address_w = GE::RHI::SamplerAddressMode::ClampToEdge};
            if (!Compile(Shader, "VSMain", "vs_5_0",
                    composition.vertex_shader, out_error)
                || !Compile(Shader, "PSMain", "ps_5_0",
                    composition.pixel_shader, out_error))
            {
                scene.textures.pop_back();
                return false;
            }
            scene.ui_composition = std::move(composition);
            return true;
        }
        catch (const std::exception& exception)
        {
            out_error = exception.what();
            scene.textures.resize(original_texture_count);
            scene.ui_composition.reset();
            return false;
        }
    }
}
