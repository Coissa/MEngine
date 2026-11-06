#include "SceneMath.h"

#include "Component.h"

#include <Core/Math/angle.h>
#include <Core/Math/matrix4.h>

#include <cmath>

namespace GE::Runtime
{
    bool IsValidPerspectiveCamera(const CameraComponent& camera) noexcept
    {
        return std::isfinite(camera.vertical_field_of_view_degrees) && std::isfinite(camera.near_plane) &&
               std::isfinite(camera.far_plane) && camera.vertical_field_of_view_degrees > 0.0F &&
               camera.vertical_field_of_view_degrees < 180.0F && camera.near_plane > 0.0F &&
               camera.far_plane > camera.near_plane;
    }

    RenderCore::RenderMatrix4 MakeSceneRotationMatrix(const SceneVector3& rotation_degrees) noexcept
    {
        return Math::EulerXYZMatrix(rotation_degrees);
    }

    RenderCore::RenderMatrix4 MakeSceneLocalToWorldMatrix(const TransformComponent& transform) noexcept
    {
        return Math::ScaleMatrix(transform.scale) * MakeSceneRotationMatrix(transform.rotation_degrees) *
               Math::TranslationMatrix(transform.translation);
    }

    RenderCore::RenderMatrix4 MakeSceneViewMatrix(const TransformComponent& transform) noexcept
    {
        const Math::Mat4 rotation = MakeSceneRotationMatrix(transform.rotation_degrees);
        const Math::Vec3 inverse_translation{-transform.translation[0], -transform.translation[1],
                                             -transform.translation[2]};
        return Math::TranslationMatrix(inverse_translation) * Math::Transpose(rotation);
    }

    RenderCore::RenderMatrix4 MakeSceneProjectionMatrix(const CameraComponent& camera, float aspect) noexcept
    {
        return Math::PerspectiveFovLH_ZO(Math::ToRadians(Math::Degrees{camera.vertical_field_of_view_degrees}), aspect,
                                         camera.near_plane, camera.far_plane);
    }

    SceneCameraBasis MakeSceneCameraBasis(const SceneVector3& rotation_degrees) noexcept
    {
        const Math::Mat4 rotation = MakeSceneRotationMatrix(rotation_degrees);
        return {.right = Math::TransformVector({1, 0, 0}, rotation),
                .up = Math::TransformVector({0, 1, 0}, rotation),
                .forward = Math::TransformVector({0, 0, 1}, rotation)};
    }
} // namespace GE::Runtime
