#include <Core/Math/angle.h>
#include <Renderer/RenderData.h>
#include "../RenderSceneFixture.h"

#include <cmath>
#include <iostream>

namespace
{
    [[nodiscard]] bool Near(float left, float right, float epsilon = 0.0001F) noexcept
    {
        return std::abs(left - right) <= epsilon;
    }

    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[RenderMathTests] " << message << '\n';
        return code;
    }
} // namespace
int main()
{
    GE::RenderCore::RenderFrame frame{.output_extent = {.width = 1600, .height = 900}};
    GE::Tests::BuildRotatingCubeRenderFrame(0.0F, frame);
    if (frame.render_items.size() != 2 || !Near(frame.main_view.view[14], 5.0F) ||
        !Near(frame.main_view.projection[11], 1.0F) || !Near(frame.main_view.camera_world_position[2], -5.0F) ||
        !Near(frame.main_view.camera_world_position[3], 1.0F) ||
        !Near(frame.render_items[0].local_to_world[12], -1.35F) ||
        !Near(frame.render_items[1].local_to_world[0], 0.70F) ||
        !Near(frame.render_items[1].local_to_world[5], 1.20F) ||
        !Near(frame.render_items[1].local_to_world[10], 1.45F) ||
        !Near(frame.render_items[1].local_to_world[12], 1.35F))
    {
        return Fail(1, "example view, projection, or translation changed");
    }

    frame.render_items.clear();
    const float quarter_turn_time = (GE::Math::Pi * 0.5F) / 0.75F;
    GE::Tests::BuildRotatingCubeRenderFrame(quarter_turn_time, frame);
    const GE::Math::Mat4& first_transform = frame.render_items[0].local_to_world;
    if (!Near(first_transform[0], 0.0F) || !Near(first_transform[2], -1.0F) || !Near(first_transform[8], 1.0F) ||
        !Near(first_transform[10], 0.0F))
    {
        return Fail(2, "example rotation direction changed");
    }

    std::cout << "[RenderMathTests] Passed\n";
    return 0;
}
