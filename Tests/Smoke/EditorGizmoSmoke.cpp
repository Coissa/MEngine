#include <Editor/EditorCamera.h>
#include <Editor/EditorGizmo.h>
#include <Runtime/SceneMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace
{
    [[nodiscard]] bool Near(float left, float right) noexcept
    {
        return std::abs(left - right) <= 0.001F;
    }

    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[EditorGizmoSmoke] Failure: " << message << '\n';
        return code;
    }
} // namespace

int main()
{
    constexpr GE::RenderCore::RenderExtent Extent{1000, 500};
    GE::Editor::EditorCamera camera;
    GE::RenderCore::RenderView view;
    if (!camera.BuildRenderView(Extent, view))
    {
        return Fail(1, "editor view construction failed");
    }

    GE::Editor::TranslationGizmoAxisProjection x_projection;
    if (!GE::Editor::BuildTranslationGizmoAxisProjection({}, GE::Editor::EditorGizmoAxis::X,
                                                         view.view * view.projection, Extent, x_projection) ||
        x_projection.screen_direction[0] <= 0.0F || x_projection.pixels_per_world_unit <= 0.0F)
    {
        return Fail(2, "X axis projection is invalid");
    }

    GE::Runtime::TransformComponent initial;
    initial.translation = {1.0F, 2.0F, 3.0F};
    const GE::Math::Vec2 two_units{x_projection.screen_direction[0] * x_projection.pixels_per_world_unit * 2.0F,
                                   x_projection.screen_direction[1] * x_projection.pixels_per_world_unit * 2.0F};
    const GE::Runtime::TransformComponent moved =
        GE::Editor::ApplyTranslationGizmoDrag(initial, GE::Editor::EditorGizmoAxis::X, x_projection, two_units);
    if (!Near(moved.translation[0], 3.0F) || !Near(moved.translation[1], 2.0F) || !Near(moved.translation[2], 3.0F))
    {
        return Fail(3, "screen drag did not map to world-axis translation");
    }

    const GE::Math::Vec2 perpendicular{-x_projection.screen_direction[1] * 100.0F,
                                       x_projection.screen_direction[0] * 100.0F};
    const GE::Runtime::TransformComponent unchanged =
        GE::Editor::ApplyTranslationGizmoDrag(initial, GE::Editor::EditorGizmoAxis::X, x_projection, perpendicular);
    if (!Near(unchanged.translation[0], initial.translation[0]))
    {
        return Fail(4, "perpendicular drag changed the selected axis");
    }

    GE::RenderCore::GizmoRenderData gizmo;
    constexpr std::uint32_t AxisColor = 0xFF4646E6U;
    float gizmo_world_length = 0.0F;
    if (!GE::Editor::ComputeTranslationGizmoWorldLength(x_projection.origin_clip, view.projection, Extent, 72.0F,
                                                        gizmo_world_length))
    {
        return Fail(5, "screen-size world scale is invalid");
    }
    if (!GE::Editor::AppendTranslationGizmoAxisGeometry(x_projection, Extent, gizmo_world_length, 5.0F, 6.0F, AxisColor,
                                                        gizmo) ||
        gizmo.vertices.size() != 17 || gizmo.indices.size() != 42)
    {
        return Fail(6, "screen-space axis geometry is invalid");
    }
    if (!std::all_of(gizmo.vertices.begin(), gizmo.vertices.end(),
                     [](const GE::RenderCore::GizmoVertex& vertex)
                     {
                         return vertex.color == AxisColor && std::isfinite(vertex.clip_position[0]) &&
                                std::isfinite(vertex.clip_position[1]) && std::isfinite(vertex.clip_position[2]) &&
                                std::isfinite(vertex.clip_position[3]) && vertex.clip_position[3] > 0.0F;
                     }) ||
        !std::all_of(gizmo.indices.begin(), gizmo.indices.end(),
                     [&gizmo](std::uint32_t index) { return index < gizmo.vertices.size(); }))
    {
        return Fail(7, "generated Gizmo vertices or indices are invalid");
    }
    const auto clip_to_pixels = [](const GE::RenderCore::GizmoVertex& vertex)
    {
        const float inverse_w = 1.0F / vertex.clip_position[3];
        return GE::Math::Vec2{(vertex.clip_position[0] * inverse_w * 0.5F + 0.5F) * static_cast<float>(Extent.width),
                              (0.5F - vertex.clip_position[1] * inverse_w * 0.5F) * static_cast<float>(Extent.height)};
    };
    const GE::Math::Vec2 shaft_origin_a = clip_to_pixels(gizmo.vertices[0]);
    const GE::Math::Vec2 shaft_origin_b = clip_to_pixels(gizmo.vertices[1]);
    const GE::Math::Vec2 shaft_end_a = clip_to_pixels(gizmo.vertices[2]);
    const GE::Math::Vec2 shaft_end_b = clip_to_pixels(gizmo.vertices[3]);
    const GE::Math::Vec2 shaft_origin{(shaft_origin_a[0] + shaft_origin_b[0]) * 0.5F,
                                      (shaft_origin_a[1] + shaft_origin_b[1]) * 0.5F};
    const GE::Math::Vec2 shaft_end{(shaft_end_a[0] + shaft_end_b[0]) * 0.5F, (shaft_end_a[1] + shaft_end_b[1]) * 0.5F};
    const float shaft_x = shaft_end[0] - shaft_origin[0];
    const float shaft_y = shaft_end[1] - shaft_origin[1];
    if (std::abs(std::sqrt(shaft_x * shaft_x + shaft_y * shaft_y) - 72.0F) > 0.05F)
    {
        return Fail(8, "generated Gizmo axis is not screen-size invariant");
    }

    GE::Editor::EditorCamera angled_camera;
    angled_camera.ApplyInput({.mouse_delta_x = 200.0F, .mouse_delta_y = -100.0F}, 0.0F);
    GE::RenderCore::RenderView angled_view;
    if (!angled_camera.BuildRenderView(Extent, angled_view))
    {
        return Fail(9, "angled editor view construction failed");
    }
    const GE::Runtime::SceneCameraBasis angled_basis =
        GE::Runtime::MakeSceneCameraBasis(angled_camera.GetTransform().rotation_degrees);
    const GE::Math::Vec3 angled_origin{angled_camera.GetTransform().translation[0] + angled_basis.forward[0] * 7.0F,
                                       angled_camera.GetTransform().translation[1] + angled_basis.forward[1] * 7.0F,
                                       angled_camera.GetTransform().translation[2] + angled_basis.forward[2] * 7.0F};
    constexpr std::array<GE::Editor::EditorGizmoAxis, 3> Axes{
        GE::Editor::EditorGizmoAxis::X, GE::Editor::EditorGizmoAxis::Y, GE::Editor::EditorGizmoAxis::Z};
    std::array<GE::Editor::TranslationGizmoAxisProjection, 3> angled_projections{};
    for (std::size_t index = 0; index < Axes.size(); ++index)
    {
        if (!GE::Editor::BuildTranslationGizmoAxisProjection(angled_origin, Axes[index],
                                                             angled_view.view * angled_view.projection, Extent,
                                                             angled_projections[index]))
        {
            return Fail(10, "angled axis projection is invalid");
        }
    }
    float common_world_length = 0.0F;
    if (!GE::Editor::ComputeTranslationGizmoWorldLength(angled_projections[0].origin_clip, angled_view.projection,
                                                        Extent, 72.0F, common_world_length))
    {
        return Fail(11, "angled Gizmo world scale is invalid");
    }
    std::array<float, 3> angled_screen_lengths{};
    for (std::size_t index = 0; index < Axes.size(); ++index)
    {
        if (!GE::Editor::ComputeTranslationGizmoScreenLength(angled_projections[index], Extent, common_world_length,
                                                             angled_screen_lengths[index]))
        {
            return Fail(12, "angled axis screen length is invalid");
        }
    }
    const auto [shortest, longest] = std::minmax_element(angled_screen_lengths.begin(), angled_screen_lengths.end());
    if (*longest - *shortest < 5.0F)
    {
        return Fail(13, "common Gizmo scale removed axis-angle foreshortening");
    }

    const std::size_t original_vertices = gizmo.vertices.size();
    const std::size_t original_indices = gizmo.indices.size();
    if (GE::Editor::AppendTranslationGizmoAxisGeometry(x_projection, {}, gizmo_world_length, 5.0F, 6.0F, AxisColor,
                                                       gizmo) ||
        gizmo.vertices.size() != original_vertices || gizmo.indices.size() != original_indices)
    {
        return Fail(14, "invalid geometry request mutated the output");
    }

    std::cout << "[EditorGizmoSmoke] Passed\n";
    return 0;
}
