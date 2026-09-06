#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace GE::RenderCore
{
    enum class UITexture : std::uint8_t
    {
        FontAtlas,
        SceneViewport
    };

    struct UIVertex final
    {
        std::array<float, 2> position{};
        std::array<float, 2> texture_coordinate{};
        std::uint32_t color = 0;
    };

    struct UIDrawCommand final
    {
        std::array<float, 4> clip_rectangle{};
        UITexture texture = UITexture::FontAtlas;
        std::uint32_t index_count = 0;
        std::uint32_t first_index = 0;
        std::int32_t vertex_offset = 0;
    };

    // Editor 把 ImGui 绘制结果转换为这一纯值包；Renderer 不依赖 ImGui 类型。
    struct UIRenderData final
    {
        std::array<float, 2> display_position{};
        std::array<float, 2> display_size{};
        std::array<float, 2> framebuffer_scale{1.0F, 1.0F};
        std::vector<UIVertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<UIDrawCommand> commands;

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return vertices.empty() || indices.empty() || commands.empty();
        }
    };
}
