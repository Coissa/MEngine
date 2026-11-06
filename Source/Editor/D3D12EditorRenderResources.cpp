#include <Renderer/ShaderLibrary.h>
#include "D3D12EditorRenderResources.h"

#include "EditorUserInterface.h"

#include <Renderer/SceneData.h>

#include <exception>
#include <filesystem>

namespace GE::Editor
{
    namespace
    {
        constexpr GE::RenderCore::TextureHandle FontAtlasTexture{0xFFFF'FFF0U};

        using GE::Render::LoadShaderBytecode;

    } // namespace

    bool AddD3D12EditorRenderResources(const EditorFontAtlas& font_atlas, const std::filesystem::path& shader_directory,
                                       GE::Render::RHIRenderScene& scene, std::string& out_error) noexcept
    {
        out_error.clear();
        if (font_atlas.width == 0 || font_atlas.height == 0 ||
            font_atlas.rgba_pixels.size() != static_cast<std::size_t>(font_atlas.width) * font_atlas.height * 4)
        {
            out_error = "Invalid editor font atlas";
            return false;
        }

        const std::size_t original_texture_count = scene.textures.size();
        try
        {
            const std::filesystem::path ui_shader = shader_directory / "editor" / "editor_ui.hlsl";
            const std::filesystem::path gizmo_shader = shader_directory / "editor" / "editor_gizmo.hlsl";

            GE::Render::RHITextureDescription texture;
            texture.handle = FontAtlasTexture;
            texture.width = font_atlas.width;
            texture.height = font_atlas.height;
            texture.format = GE::RHI::RHIFormat::R8G8B8A8_UNorm;
            texture.pixels = font_atlas.rgba_pixels;
            scene.textures.push_back(std::move(texture));

            GE::Render::RHIUICompositionDescription composition;
            composition.font_atlas_texture = FontAtlasTexture;
            composition.sampler = {.filter = GE::RHI::SamplerFilter::Linear,
                                   .address_u = GE::RHI::SamplerAddressMode::ClampToEdge,
                                   .address_v = GE::RHI::SamplerAddressMode::ClampToEdge,
                                   .address_w = GE::RHI::SamplerAddressMode::ClampToEdge};
            if (!LoadShaderBytecode(ui_shader, "VSMain", composition.vertex_shader, out_error) ||
                !LoadShaderBytecode(ui_shader, "PSMain", composition.pixel_shader, out_error))
            {
                scene.textures.pop_back();
                return false;
            }

            GE::Render::RHIGizmoCompositionDescription gizmo;
            if (!LoadShaderBytecode(gizmo_shader, "GizmoVS", gizmo.vertex_shader, out_error) ||
                !LoadShaderBytecode(gizmo_shader, "GizmoVisiblePS", gizmo.visible_pixel_shader, out_error) ||
                !LoadShaderBytecode(gizmo_shader, "GizmoOccludedPS", gizmo.occluded_pixel_shader, out_error))
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
} // namespace GE::Editor
