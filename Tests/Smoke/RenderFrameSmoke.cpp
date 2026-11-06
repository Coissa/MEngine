#include <Renderer/RenderData.h>

#include <type_traits>
#include <utility>

namespace
{
    using GE::RenderCore::RenderFrame;

    // RenderFrame 现在是拥有 RenderItem 数组的移动包，不再是 trivially-copyable POD。
    static_assert(std::is_nothrow_move_constructible_v<RenderFrame>);
} // namespace

int main()
{
    const RenderFrame default_frame;
    if (default_frame.frame_number != 0 || default_frame.output_extent.HasArea() ||
        default_frame.main_view.view[0] != 1.0F || default_frame.main_view.projection[15] != 1.0F ||
        default_frame.main_view.camera_world_position != std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F} ||
        !default_frame.render_items.empty())
    {
        return 1;
    }

    RenderFrame submitted_frame{.frame_number = 42,
                                .output_extent = {.width = 1920, .height = 1080},
                                .main_view = {.view = {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                                       0.0F, 0.0F, 0.0F, 5.0F, 1.0F},
                                              .projection = {2.0F, 0.0F, 0.0F, 0.0F, 0.0F, 3.0F, 0.0F, 0.0F, 0.0F, 0.0F,
                                                             1.0F, 1.0F, 0.0F, 0.0F, -0.1F, 0.0F},
                                              .camera_world_position = {0.0F, 0.0F, -5.0F, 1.0F}},
                                .real_elapsed_seconds = 1.5};
    submitted_frame.render_items.push_back({.mesh = {1}, .material = {2}});

    RenderFrame moved_frame = std::move(submitted_frame);
    moved_frame.frame_number = 43;
    moved_frame.output_extent.width = 1280;

    if (moved_frame.frame_number != 43 || moved_frame.output_extent.width != 1280 ||
        moved_frame.main_view.view[14] != 5.0F || moved_frame.main_view.projection[0] != 2.0F ||
        moved_frame.main_view.camera_world_position[2] != -5.0F || moved_frame.real_elapsed_seconds != 1.5 ||
        moved_frame.render_items.size() != 1 || !moved_frame.render_items.front().mesh.IsValid() ||
        moved_frame.render_items.front().material.value != 2)
    {
        return 2;
    }

    return 0;
}
