#include "EditorGizmo.h"

#include <cmath>
#include <cstddef>

namespace GE::Editor
{
    namespace
    {
        [[nodiscard]] GE::Math::Vec3 AxisVector(
            EditorGizmoAxis axis) noexcept
        {
            switch (axis)
            {
            case EditorGizmoAxis::X:
                return {1.0F, 0.0F, 0.0F};
            case EditorGizmoAxis::Y:
                return {0.0F, 1.0F, 0.0F};
            case EditorGizmoAxis::Z:
                return {0.0F, 0.0F, 1.0F};
            default:
                return {};
            }
        }

        [[nodiscard]] bool ProjectPoint(
            GE::Math::Vec3 world,
            const GE::Math::Mat4& view_projection,
            GE::RenderCore::RenderExtent extent,
            GE::Math::Vec2& out_pixels) noexcept
        {
            const GE::Math::Vec4 clip = GE::Math::Transform(
                {world[0], world[1], world[2], 1.0F}, view_projection);
            if (!GE::Math::IsFinite(clip) || clip[3] <= 0.0001F)
            {
                return false;
            }
            const float inverse_w = 1.0F / clip[3];
            const float ndc_x = clip[0] * inverse_w;
            const float ndc_y = clip[1] * inverse_w;
            const float ndc_z = clip[2] * inverse_w;
            if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y)
                || !std::isfinite(ndc_z) || ndc_z < 0.0F || ndc_z > 1.0F)
            {
                return false;
            }
            out_pixels = {
                (ndc_x * 0.5F + 0.5F) * static_cast<float>(extent.width),
                (0.5F - ndc_y * 0.5F) * static_cast<float>(extent.height)};
            return GE::Math::IsFinite(out_pixels);
        }
    }

    bool BuildTranslationGizmoAxisProjection(
        GE::Math::Vec3 world_origin,
        EditorGizmoAxis axis,
        const GE::Math::Mat4& view_projection,
        GE::RenderCore::RenderExtent extent,
        TranslationGizmoAxisProjection& out_projection) noexcept
    {
        out_projection = {};
        if (axis == EditorGizmoAxis::None || !extent.HasArea())
        {
            return false;
        }

        GE::Math::Vec2 origin;
        GE::Math::Vec2 endpoint;
        if (!ProjectPoint(world_origin, view_projection, extent, origin)
            || !ProjectPoint(
                world_origin + AxisVector(axis),
                view_projection,
                extent,
                endpoint))
        {
            return false;
        }

        const float delta_x = endpoint[0] - origin[0];
        const float delta_y = endpoint[1] - origin[1];
        const float length = std::sqrt(
            delta_x * delta_x + delta_y * delta_y);
        if (!std::isfinite(length) || length <= 0.01F)
        {
            return false;
        }

        out_projection.origin_pixels = origin;
        out_projection.screen_direction = {
            delta_x / length, delta_y / length};
        out_projection.pixels_per_world_unit = length;
        return true;
    }

    GE::Runtime::TransformComponent ApplyTranslationGizmoDrag(
        const GE::Runtime::TransformComponent& initial_transform,
        EditorGizmoAxis axis,
        const TranslationGizmoAxisProjection& projection,
        GE::Math::Vec2 mouse_delta) noexcept
    {
        GE::Runtime::TransformComponent result = initial_transform;
        if (axis == EditorGizmoAxis::None
            || !std::isfinite(projection.pixels_per_world_unit)
            || projection.pixels_per_world_unit <= 0.01F
            || !GE::Math::IsFinite(mouse_delta))
        {
            return result;
        }

        const float projected_pixels =
            mouse_delta[0] * projection.screen_direction[0]
            + mouse_delta[1] * projection.screen_direction[1];
        const float world_delta =
            projected_pixels / projection.pixels_per_world_unit;
        const std::size_t component = axis == EditorGizmoAxis::X
            ? 0 : axis == EditorGizmoAxis::Y ? 1 : 2;
        result.translation[component] += world_delta;
        return result;
    }
}
