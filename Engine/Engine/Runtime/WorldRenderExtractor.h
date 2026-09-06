#pragma once

#include <Engine/RenderCore/RenderFrame.h>

namespace GE::Runtime
{
    class World;

    class WorldRenderExtractor final
    {
    public:
        // out_frame 的帧号、尺寸和逻辑时间由外层帧编排者预先填写。
        [[nodiscard]] static bool Extract(
            const World& world,
            RenderCore::RenderFrame& out_frame) noexcept;

        // Editor 可以提供独立相机，只复用 World 到绘制项的提取路径。
        [[nodiscard]] static bool ExtractRenderItems(
            const World& world,
            RenderCore::RenderFrame& out_frame) noexcept;

        // PIE 使用场景相机，但相机缺失时允许 Editor 保持 UI 存活。
        [[nodiscard]] static bool ExtractView(
            const World& world,
            RenderCore::RenderFrame& out_frame) noexcept;
    };
}
