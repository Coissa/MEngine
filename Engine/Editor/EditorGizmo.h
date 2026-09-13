#pragma once

#include <Engine/Core/Math/matrix4.h>
#include <Engine/RenderCore/GizmoRenderData.h>
#include <Engine/RenderCore/RenderExtent.h>
#include <Engine/Runtime/Component.h>

namespace GE::Editor
{
    enum class EditorGizmoAxis
    {
        None,
        X,
        Y,
        Z
    };

    struct TranslationGizmoAxisProjection final
    {
        GE::Math::Vec2 origin_pixels;
        GE::Math::Vec2 screen_direction;
        float pixels_per_world_unit = 0.0F;
        GE::Math::Vec4 origin_clip;
        GE::Math::Vec4 unit_endpoint_clip;
    };

    [[nodiscard]] bool BuildTranslationGizmoAxisProjection(
        GE::Math::Vec3 world_origin,
        EditorGizmoAxis axis,
        const GE::Math::Mat4& view_projection,
        GE::RenderCore::RenderExtent extent,
        TranslationGizmoAxisProjection& out_projection) noexcept;

    [[nodiscard]] bool ComputeTranslationGizmoWorldLength(
        const GE::Math::Vec4& origin_clip,
        const GE::Math::Mat4& projection,
        GE::RenderCore::RenderExtent extent,
        float target_screen_length,
        float& out_world_length) noexcept;

    [[nodiscard]] bool ComputeTranslationGizmoScreenLength(
        const TranslationGizmoAxisProjection& projection,
        GE::RenderCore::RenderExtent extent,
        float world_length,
        float& out_screen_length) noexcept;

    [[nodiscard]] bool AppendTranslationGizmoAxisGeometry(
        const TranslationGizmoAxisProjection& projection,
        GE::RenderCore::RenderExtent extent,
        float world_length,
        float thickness_pixels,
        float endpoint_radius_pixels,
        std::uint32_t color,
        GE::RenderCore::GizmoRenderData& out_gizmo) noexcept;

    [[nodiscard]] GE::Runtime::TransformComponent ApplyTranslationGizmoDrag(
        const GE::Runtime::TransformComponent& initial_transform,
        EditorGizmoAxis axis,
        const TranslationGizmoAxisProjection& projection,
        GE::Math::Vec2 mouse_delta) noexcept;
}
