#include <algorithm>
#include "EditorCamera.h"

#include <Runtime/GameObject.h>
#include <Runtime/SceneMath.h>
#include <Runtime/World.h>
#include <Core/Math/angle.h>
#include <Core/Math/vector.h>

#include <cmath>

namespace GE::Editor
{
    namespace
    {
        [[nodiscard]] bool IsFiniteInput(const EditorCameraInput& input) noexcept
        {
            return std::isfinite(input.forward_axis) && std::isfinite(input.right_axis) &&
                   std::isfinite(input.up_axis) && std::isfinite(input.mouse_delta_x) &&
                   std::isfinite(input.mouse_delta_y) && std::isfinite(input.wheel_delta);
        }

    } // namespace

    EditorCamera::EditorCamera() noexcept
    {
        m_transform.translation = {0.0F, 0.0F, -7.0F};
        m_projection.vertical_field_of_view_degrees = 60.0F;
        m_projection.near_plane = 0.1F;
        m_projection.far_plane = 1'000.0F;
        m_projection.primary = false;
    }

    void EditorCamera::ResetFromWorld(const GE::Runtime::World& world) noexcept
    {
        const GE::Runtime::GameObject* first_valid = nullptr;
        const GE::Runtime::GameObject* primary = nullptr;
        for (const GE::Runtime::GameObjectHandle handle : world.GetGameObjectHandles())
        {
            const GE::Runtime::GameObject* object = world.FindGameObject(handle);
            const GE::Runtime::CameraComponent* camera = object == nullptr ? nullptr : object->GetCamera();
            if (camera == nullptr || !GE::Runtime::IsValidPerspectiveCamera(*camera))
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

        const GE::Runtime::GameObject* source = primary == nullptr ? first_valid : primary;
        if (source == nullptr)
        {
            *this = EditorCamera{};
            return;
        }

        m_transform = source->GetTransform();
        if (source->GetParent())
        {
            auto rotation = source->GetWorldMatrix();
            m_transform.translation = GE::Math::TransformPoint({}, rotation);
            m_transform.scale = {1, 1, 1};
            for (size_t row = 0; row < 3; ++row)
            {
                GE::Math::Vec3 axis{rotation.At(row, 0), rotation.At(row, 1), rotation.At(row, 2)};
                if (GE::Math::Normalize(axis))
                    for (size_t col = 0; col < 3; ++col)
                        rotation.At(row, col) = axis[col];
            }
            const float pitch = std::asin(std::clamp(-rotation.At(0, 2), -1.0f, 1.0f));
            constexpr float degrees = 57.29577951308232f;
            if (std::abs(std::cos(pitch)) > 1.0e-5f)
                m_transform.rotation_degrees = {std::atan2(rotation.At(1, 2), rotation.At(2, 2)) * degrees,
                                                pitch * degrees,
                                                std::atan2(rotation.At(0, 1), rotation.At(0, 0)) * degrees};
            else
                m_transform.rotation_degrees = {std::atan2(-rotation.At(2, 1), rotation.At(1, 1)) * degrees,
                                                pitch * degrees, 0};
        }
        m_projection = *source->GetCamera();
        m_projection.primary = false;
        m_move_speed = 5.0F;
    }

    void EditorCamera::FrameBounds(Math::Vec3 bounds_min, Math::Vec3 bounds_max) noexcept
    {
        const Math::Vec3 center = (bounds_min + bounds_max) * 0.5F;
        const float radius = Math::Length(bounds_max - bounds_min) * 0.5F;
        const float distance = std::max(2.0F, radius * 2.6F);
        m_transform.translation = center + Math::Vec3{0.0F, 0.0F, -distance};
        m_transform.rotation_degrees = {};
        m_move_speed = std::max(0.5F, radius);
        m_projection.far_plane = std::max(1000.0F, distance + radius * 4.0F);
    }

    void EditorCamera::ApplyInput(const EditorCameraInput& input, float delta_seconds) noexcept
    {
        if (!IsFiniteInput(input) || !std::isfinite(delta_seconds) || delta_seconds < 0.0F)
        {
            return;
        }

        m_transform.rotation_degrees[0] =
            std::clamp(m_transform.rotation_degrees[0] + input.mouse_delta_y * m_mouse_sensitivity, -89.0F, 89.0F);
        m_transform.rotation_degrees[1] =
            std::remainder(m_transform.rotation_degrees[1] + input.mouse_delta_x * m_mouse_sensitivity, 360.0F);

        if (input.wheel_delta != 0.0F)
        {
            m_move_speed = std::clamp(m_move_speed * std::pow(1.2F, input.wheel_delta), 0.1F, 100.0F);
        }

        const GE::Runtime::SceneCameraBasis basis = GE::Runtime::MakeSceneCameraBasis(m_transform.rotation_degrees);
        GE::Math::Vec3 movement{basis.forward[0] * input.forward_axis + basis.right[0] * input.right_axis,
                                basis.forward[1] * input.forward_axis + basis.right[1] * input.right_axis +
                                    input.up_axis,
                                basis.forward[2] * input.forward_axis + basis.right[2] * input.right_axis};
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

    bool EditorCamera::BuildRenderView(GE::RenderCore::RenderExtent extent,
                                       GE::RenderCore::RenderView& out_view) const noexcept
    {
        if (!extent.HasArea() || !GE::Runtime::IsValidPerspectiveCamera(m_projection))
        {
            return false;
        }
        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        out_view.view = GE::Runtime::MakeSceneViewMatrix(m_transform);
        out_view.projection = GE::Runtime::MakeSceneProjectionMatrix(m_projection, aspect);
        out_view.camera_world_position = {m_transform.translation[0], m_transform.translation[1],
                                          m_transform.translation[2], 1.0F};
        out_view.camera_id = 0;
        out_view.valid = true;
        return true;
    }

    bool EditorCamera::BuildRay(const std::array<float, 2>& viewport_uv, GE::RenderCore::RenderExtent extent,
                                EditorRay& out_ray) const noexcept
    {
        if (!extent.HasArea() || !std::isfinite(viewport_uv[0]) || !std::isfinite(viewport_uv[1]) ||
            viewport_uv[0] < 0.0F || viewport_uv[0] > 1.0F || viewport_uv[1] < 0.0F || viewport_uv[1] > 1.0F ||
            !GE::Runtime::IsValidPerspectiveCamera(m_projection))
        {
            return false;
        }

        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
        const float tangent = std::tan(
            GE::Math::ToRadians(GE::Math::Degrees{m_projection.vertical_field_of_view_degrees}).Value() * 0.5F);
        GE::Math::Vec3 camera_direction{(2.0F * viewport_uv[0] - 1.0F) * aspect * tangent,
                                        (1.0F - 2.0F * viewport_uv[1]) * tangent, 1.0F};
        if (!GE::Math::Normalize(camera_direction))
        {
            return false;
        }

        const GE::Runtime::SceneCameraBasis basis = GE::Runtime::MakeSceneCameraBasis(m_transform.rotation_degrees);
        out_ray.origin = m_transform.translation;
        out_ray.direction = {basis.right[0] * camera_direction[0] + basis.up[0] * camera_direction[1] +
                                 basis.forward[0] * camera_direction[2],
                             basis.right[1] * camera_direction[0] + basis.up[1] * camera_direction[1] +
                                 basis.forward[1] * camera_direction[2],
                             basis.right[2] * camera_direction[0] + basis.up[2] * camera_direction[1] +
                                 basis.forward[2] * camera_direction[2]};
        return GE::Math::Normalize(out_ray.direction);
    }

    const GE::Runtime::TransformComponent& EditorCamera::GetTransform() const noexcept
    {
        return m_transform;
    }

    const GE::Runtime::CameraComponent& EditorCamera::GetProjection() const noexcept
    {
        return m_projection;
    }

    float EditorCamera::GetMoveSpeed() const noexcept
    {
        return m_move_speed;
    }
} // namespace GE::Editor
