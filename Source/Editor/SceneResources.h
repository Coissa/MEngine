#pragma once

#include <Renderer/SceneData.h>

#include <Renderer/RenderData.h>
#include <Assets/Registry/AssetRegistry.h>
#include <Runtime/World.h>

#include <filesystem>
#include <string>

namespace GE::Editor
{
    using namespace GE::Render;
    // Shared shaders and the existing sky; scene geometry is loaded separately from asset references.
    [[nodiscard]] bool BuildEditorScene(const std::filesystem::path& shader_directory, RHIRenderScene& out_scene,
                                        std::string& out_error) noexcept;

    [[nodiscard]] bool LoadSceneAssets(const Runtime::World&, const Asset::AssetRegistry&,
                                       const std::filesystem::path& asset_root, RHIRenderScene&, std::string& error);
} // namespace GE::Editor
