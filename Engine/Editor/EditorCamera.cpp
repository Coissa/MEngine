#include "EditorCamera.h"

#include <Engine/Runtime/GameObject.h>
#include <Engine/Runtime/SceneMath.h>
#include <Engine/Runtime/World.h>
#include <Engine/Core/Math/angle.h>
#include <Engine/Core/Math/vector.h>

#include <algorithm>
#include <cmath>

namespace GE::Editor
{
    namespace
    {
        [[nodiscard]] bool IsFiniteInput(
            const EditorCameraInput& input) noexcept
        {
            return std::isfinite(input.forward_axis)
                && std::isfinite(input.right_axis)
                && std::isfinite(input.up_axis)
                && std::isfinite(input.mouse_delta_x)
                && std::isfinite(input.mouse_delta_y)
                && std::isfinite(input.wheel_delta);
        }

    }

    EditorCamera::EditorCamera() noexcept
    {
        m_transform.translation = {0.0F, 0.0F, -7.0F};
        m_projection.vertical_field_of_view_degrees = 60.0F;
        m_projection.near_plane = 0.1F;
        m_projection.far_plane = 1'000.0F;
        m_projection.primary = false;
    }

    void EditorCamera::ResetFromWorld(
        const GE::Runtime::World& world) noexcept
    {
        const GE::Runtime::GameObject* first_valid = nullptr;
        const GE::Runtime::GameObject* primary = nullptr;
        for (const GE::Runtime::GameObjectHandle handle
            : world.GetGameObjectHandles())
        {
            const GE::Runtime::GameObject* object = world.FindGameObject(handle);
            const GE::Runtime::CameraComponent* camera =
                object == nullptr ? nullptr : object->GetCamera();
            if (camera == nullptr
                || !GE::Runtime::IsValidPerspectiveCamera(*camera))
            {
                continue;
            }
            if (first_valid == nullptr)
            {
                first_valid = object;
            }
            if (camera->primary)
            {
                primary = object;
                break;
            }
        }

        const GE::Runtime::GameObject* source =
            primary == nullptr ? first_valid : primary;
        if (source == nullptr)
        {
            *this = EditorCamera{};
            return;
        }

        m_transform = source->GetTransform();
        m_projection = *source->GetCamera();
        m_projection.primary = false;
        m_move_speed = 5.0F;
    }

    void EditorCamera::ApplyInput(
        const EditorCameraInput& input,
        float delta_seconds) noexcept
    {
        if (!IsFiniteInput(input) || !std::isfinite(delta_seconds)
            || delta_seconds < 0.0F)
        {
            return;
        }

        m_transform.rotation_degrees[0] = std::clamp(
            m_transform.rotation_degrees[0]
                + input.mouse_delta_y * m_mouse_sensitivity,
            -89.0F,
            89.0F);
        m_transform.rotation_degrees[1] = std::remainder(
            m_transform.rotation_degrees[1]
                + input.mouse_delta_x * m_mouse_sensitivity,
            360.0F);

        if (input.wheel_delta != 0.0F)
        {
            m_move_speed = std::clamp(
                m_move_speed * std::pow(1.2F, input.wheel_delta),
                0.1F,
                100.0F);
        }

        const GE::Runtime::SceneCameraBasis basis =
            GE::Runtime::MakeSceneCameraBasis(
                m_transform.rotation_degrees);
        GE::Math::Vec3 movement{
            basis.forward[0] * input.forward_axis
                + basis.right[0] * input.right_axis,
            basis.forward[1] * input.forward_axis
                + basis.right[1] * input.right_axis + input.up_axis,
            basis.forward[2] * input.forward_axis
                + basis.right[2] * input.right_axis};
        if (!GE::Math::Normalize(movement))
        {
            return;
        }

        const float distance = m_move_speed * delta_seconds;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            m_transform.translation[axis] += movement[axis] * distance;
        }
    }

    bool EditorCamera::BuildRenderView(
        GE::RenderCore::RenderExtent extent,
        GE::RenderCore::RenderView& out_view) const noexcept
    {
        if (!extent.HasArea()
            || !GE::Runtime::IsValidPerspectiveCamera(m_projection))
        {
            return false;
        }
        const float aspect = static_cast<float>(extent.width)
            / static_cast<float>(extent.height);
        out_view.view = GE::Runtime::MakeSceneViewMatrix(m_transform);
        out_view.projection =
            GE::Runtime::MakeSceneProjectionMatrix(m_projection, aspect);
        return true;
    }

    bool EditorCamera::BuildRay(
        const std::array<float, 2>& viewport_uv,
        GE::RenderCore::RenderExtent extent,
        EditorRay& out_ray) const noexcept
    {
        if (!extent.HasArea()
            || !std::isfinite(viewport_uv[0])
            || !std::isfinite(viewport_uv[1])
            || viewport_uv[0] < 0.0F || viewport_uv[0] > 1.0F
            || viewport_uv[1] < 0.0F || viewport_uv[1] > 1.0F
            || !GE::Runtime::IsValidPerspectiveCamera(m_projection))
        {
            return false;
        }

        const float aspect = static_cast<float>(extent.width)
            / static_cast<float>(extent.height);
        const float tangent = std::tan(
            GE::Math::ToRadians(GE::Math::Degrees{
                m_projection.vertical_field_of_view_degrees}).Value() * 0.5F);
        GE::Math::Vec3 camera_direction{
            (2.0F * viewport_uv[0] - 1.0F) * aspect * tangent,
            (1.0F - 2.0F * viewport_uv[1]) * tangent,
            1.0F};
        if (!GE::Math::Normalize(camera_direction))
        {
            return false;
        }

        const GE::Runtime::SceneCameraBasis basis =
            GE::Runtime::MakeSceneCameraBasis(
                m_transform.rotation_degrees);
        out_ray.origin = m_transform.translation;
        out_ray.direction = {
            basis.right[0] * camera_direction[0]
                + basis.up[0] * camera_direction[1]
                + basis.forward[0] * camera_direction[2],
            basis.right[1] * camera_direction[0]
                + basis.up[1] * camera_direction[1]
                + basis.forward[1] * camera_direction[2],
            basis.right[2] * camera_direction[0]
                + basis.up[2] * camera_direction[1]
                + basis.forward[2] * camera_direction[2]};
        return GE::Math::Normalize(out_ray.direction);
    }

    const GE::Runtime::TransformComponent&
    EditorCamera::GetTransform() const noexcept
    {
        return m_transform;
    }

    const GE::Runtime::CameraComponent&
    EditorCamera::GetProjection() const noexcept
    {
        return m_projection;
    }

    float EditorCamera::GetMoveSpeed() const noexcept
    {
        return m_move_speed;
    }
}
