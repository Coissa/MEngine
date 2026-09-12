#pragma once

#include "RenderHandles.h"

#include "RenderView.h"

#include <array>

namespace GE::RenderCore
{
    // 一次逐物体绘制的纯值快照；句柄只引用 Renderer 进程资源，不传 Object 指针。
    struct RenderItem final
    {
        MeshHandle mesh;
        MaterialHandle material;

        // 行主序 Local-to-World；默认值为单位矩阵。
        RenderMatrix4 local_to_world{
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F};
        std::array<float, 4> color_tint{1.0F, 1.0F, 1.0F, 1.0F};
    };
}
