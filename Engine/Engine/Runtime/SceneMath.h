#pragma once

#include <Engine/Core/Math/vector.h>
#include <Engine/RenderCore/RenderView.h>

namespace GE::Runtime
{
    class CameraComponent;
    class TransformComponent;

    using SceneVector3 = Math::Vec3;

    struct SceneCameraBasis final
    {
        SceneVector3 right{1.0F, 0.0F, 0.0F};
        SceneVector3 up{0.0F, 1.0F, 0.0F};
        SceneVector3 forward{0.0F, 0.0F, 1.0F};
    };

    [[nodiscard]] bool IsValidPerspectiveCamera(
        const CameraComponent& camera) noexcept;

    [[nodiscard]] RenderCore::RenderMatrix4 MakeSceneRotationMatrix(
        const SceneVector3& rotation_degrees) noexcept;

    [[nodiscard]] RenderCore::RenderMatrix4 MakeSceneLocalToWorldMatrix(
        const TransformComponent& transform) noexcept;

    [[nodiscard]] RenderCore::RenderMatrix4 MakeSceneViewMatrix(
        const TransformComponent& transform) noexcept;

    [[nodiscard]] RenderCore::RenderMatrix4 MakeSceneProjectionMatrix(
        const CameraComponent& camera,
        float aspect) noexcept;

    [[nodiscard]] SceneCameraBasis MakeSceneCameraBasis(
        const SceneVector3& rotation_degrees) noexcept;
}
