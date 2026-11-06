#include <Renderer/RenderData.h>

#include <type_traits>

namespace
{
    using GE::RenderCore::RenderExtent;

    static_assert(std::is_standard_layout_v<RenderExtent>);
    static_assert(std::is_trivially_copyable_v<RenderExtent>);
    static_assert(noexcept(RenderExtent{}.HasArea()));
} // namespace

int main()
{
    constexpr RenderExtent empty_extent;
    static_assert(!empty_extent.HasArea());

    constexpr RenderExtent zero_width_extent{.width = 0, .height = 720};
    static_assert(!zero_width_extent.HasArea());

    constexpr RenderExtent zero_height_extent{.width = 1280, .height = 0};
    static_assert(!zero_height_extent.HasArea());

    constexpr RenderExtent unit_extent{.width = 1, .height = 1};
    static_assert(unit_extent.HasArea());

    const RenderExtent normal_extent{.width = 1280, .height = 720};
    if (!normal_extent.HasArea())
    {
        return 1;
    }

    return 0;
}
