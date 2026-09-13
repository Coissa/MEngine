#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace GE::RenderCore
{
    struct GizmoVertex final
    {
        // 由 Editor 生成的齐次裁剪空间位置；Renderer 不需要读取 World 或 Camera。
        std::array<float, 4> clip_position{};
        std::uint32_t color = 0;
    };

    // 每帧临时编辑器几何。Renderer 用场景深度执行遮挡与可见两个 Pass。
    struct GizmoRenderData final
    {
        std::vector<GizmoVertex> vertices;
        std::vector<std::uint32_t> indices;

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return vertices.empty() || indices.empty();
        }
    };
}
