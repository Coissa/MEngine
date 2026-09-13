#pragma once

#include "GizmoRenderData.h"
#include "RenderExtent.h"
#include "RenderItem.h"
#include "RenderView.h"
#include "UIRenderData.h"

#include <cstdint>
#include <vector>

namespace GE::RenderCore
{
    struct RenderFrame final
    {
        std::uint64_t frame_number = 0;
        RenderExtent output_extent;

        // Editor 模式下场景先渲染到此尺寸的离屏目标，再由 UI Viewport 采样。
        RenderExtent scene_extent;

        // Game/示例层生成的观察与投影值；Renderer 不读取 Camera 或 World。
        RenderView main_view;

        // 从本次 Run 开始累计的逻辑时间；Renderer 只消费值快照，不读取 World。
        float elapsed_seconds = 0.0F;

        // 本帧逐物体绘制快照；包被移动给 Renderer，内部不含 World/Object 指针。
        std::vector<RenderItem> render_items;

        // Editor 的屏幕恒定尺寸 Gizmo；Game 帧保持为空。
        GizmoRenderData gizmo;

        // 可选的最终 UI 合成值包；Game 帧保持为空。
        UIRenderData ui;
    };
}
