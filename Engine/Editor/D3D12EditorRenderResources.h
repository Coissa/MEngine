#pragma once

#include <filesystem>
#include <string>

namespace GE::Render
{
    struct RHIRenderScene;
}

namespace GE::Editor
{
    struct EditorFontAtlas;

    [[nodiscard]] bool AddD3D12EditorRenderResources(
        const EditorFontAtlas& font_atlas,
        const std::filesystem::path& shader_directory,
        GE::Render::RHIRenderScene& scene,
        std::string& out_error) noexcept;
}
