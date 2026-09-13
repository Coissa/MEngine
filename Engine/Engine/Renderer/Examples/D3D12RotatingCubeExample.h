#pragma once

#include <Engine/Renderer/RHIRenderer.h>

#include <Engine/RenderCore/RenderFrame.h>

#include <filesystem>
#include <string>

namespace GE::Render::Examples
{
    // 构造立方体纯值数据并从打包目录编译 HLSL；不创建任何 GPU 对象。
    [[nodiscard]] bool BuildD3D12RotatingCubeScene(
        const std::filesystem::path& shader_directory,
        RHIRenderScene& out_scene,
        std::string& out_error) noexcept;

    // 构建两个旋转立方体的逐帧纯值快照，不拥有 Renderer 或 GPU 资源。
    void BuildRotatingCubeRenderFrame(
        float elapsed_seconds,
        RenderCore::RenderFrame& frame) noexcept;
}
