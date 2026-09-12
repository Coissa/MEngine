#pragma once

#include <Engine/Core/Math/matrix4.h>
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
    };

    [[nodiscard]] bool BuildTranslationGizmoAxisProjection(
        GE::Math::Vec3 world_origin,
        EditorGizmoAxis axis,
        const GE::Math::Mat4& view_projection,
        GE::RenderCore::RenderExtent extent,
        TranslationGizmoAxisProjection& out_projection) noexcept;

    [[nodiscard]] GE::Runtime::TransformComponent ApplyTranslationGizmoDrag(
        const GE::Runtime::TransformComponent& initial_transform,
        EditorGizmoAxis axis,
        const TranslationGizmoAxisProjection& projection,
        GE::Math::Vec2 mouse_delta) noexcept;
}
