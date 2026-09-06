#pragma once

#include <string>

namespace GE::Render
{
    struct RHIRenderScene;
}

namespace GE::Editor
{
    struct EditorFontAtlas;

    [[nodiscard]] bool AddD3D12EditorUIResources(
        const EditorFontAtlas& font_atlas,
        GE::Render::RHIRenderScene& scene,
        std::string& out_error) noexcept;
}
