#include "D3D12EditorRenderResources.h"

#include "EditorUserInterface.h"

#include <Engine/Renderer/RHIRenderer.h>

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <exception>
#include <filesystem>

namespace GE::Editor
{
    namespace
    {
        constexpr GE::RenderCore::TextureHandle FontAtlasTexture{0xFFFF'FFF0U};

        bool Compile(
            const std::filesystem::path& path,
            const char* entry,
            const char* target,
            std::vector<std::byte>& out_bytecode,
            std::string& out_error)
        {
            std::error_code path_error;
            if (!std::filesystem::is_regular_file(path, path_error))
            {
                out_error = "Shader file not found: " + path.string();
                return false;
            }
            Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
            Microsoft::WRL::ComPtr<ID3DBlob> diagnostics;
            UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            const HRESULT result = D3DCompileFromFile(
                path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entry, target, flags, 0,
                bytecode.ReleaseAndGetAddressOf(),
                diagnostics.ReleaseAndGetAddressOf());
            if (FAILED(result))
            {
                out_error = "Shader compile failed: " + path.string();
                if (diagnostics != nullptr)
                {
                    out_error += "\n";
                    out_error += std::string(
                        static_cast<const char*>(diagnostics->GetBufferPointer()),
                        diagnostics->GetBufferSize());
                }
                return false;
            }
            const auto* begin = static_cast<const std::byte*>(
                bytecode->GetBufferPointer());
            out_bytecode.assign(begin, begin + bytecode->GetBufferSize());
            return true;
        }
    }

    bool AddD3D12EditorRenderResources(
        const EditorFontAtlas& font_atlas,
        const std::filesystem::path& shader_directory,
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

        const std::size_t original_texture_count = scene.textures.size();
        try
        {
            const std::filesystem::path ui_shader =
                shader_directory / "editor" / "editor_ui.hlsl";
            const std::filesystem::path gizmo_shader =
                shader_directory / "editor" / "editor_gizmo.hlsl";

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
            if (!Compile(ui_shader, "VSMain", "vs_5_0",
                    composition.vertex_shader, out_error)
                || !Compile(ui_shader, "PSMain", "ps_5_0",
                    composition.pixel_shader, out_error))
            {
                scene.textures.pop_back();
                return false;
            }

            GE::Render::RHIGizmoCompositionDescription gizmo;
            if (!Compile(gizmo_shader, "GizmoVS", "vs_5_0",
                    gizmo.vertex_shader, out_error)
                || !Compile(gizmo_shader, "GizmoVisiblePS", "ps_5_0",
                    gizmo.visible_pixel_shader, out_error)
                || !Compile(gizmo_shader, "GizmoOccludedPS", "ps_5_0",
                    gizmo.occluded_pixel_shader, out_error))
            {
                scene.textures.pop_back();
                return false;
            }
            scene.ui_composition = std::move(composition);
            scene.gizmo_composition = std::move(gizmo);
            return true;
        }
        catch (const std::exception& exception)
        {
            out_error = exception.what();
            scene.textures.resize(original_texture_count);
            scene.ui_composition.reset();
            scene.gizmo_composition.reset();
            return false;
        }
    }
}
