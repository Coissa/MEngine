#pragma once

#include <array>

namespace GE::RenderCore
{
    using RenderMatrix4 = std::array<float, 16>;

    // 单个观察视图的纯值快照。它不拥有 Camera，也不暴露图形 API 对象。
    // 当前 M2 只有主视图；未来可由 RenderFrame 扩展为多视图数组。
    struct RenderView final
    {
        // 行主序矩阵，与 RenderItem 及 HLSL mul(row_vector, matrix) 约定一致。
        RenderMatrix4 view{
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F};
        RenderMatrix4 projection{
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F};
    };
}
