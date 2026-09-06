#include "EditorScenePicker.h"

#include <Engine/Runtime/GameObject.h>
#include <Engine/Runtime/SceneMath.h>
#include <Engine/Runtime/World.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace GE::Editor
{
    namespace
    {
        constexpr float ScaleEpsilon = 0.000001F;
        constexpr float DirectionEpsilon = 0.000001F;

        [[nodiscard]] bool IsFinite3(
            const std::array<float, 3>& value) noexcept
        {
            return std::isfinite(value[0]) && std::isfinite(value[1])
                && std::isfinite(value[2]);
        }

        [[nodiscard]] float Dot(
            const std::array<float, 3>& left,
            const std::array<float, 3>& right) noexcept
        {
            return left[0] * right[0] + left[1] * right[1]
                + left[2] * right[2];
        }

        [[nodiscard]] bool IntersectLocalBounds(
            const std::array<float, 3>& origin,
            const std::array<float, 3>& direction,
            const EditorMeshBounds& bounds,
            float minimum_t,
            float maximum_t,
            float& out_t) noexcept
        {
            float enter = minimum_t;
            float exit = maximum_t;
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                // Keep this test scale-invariant. A very small but non-zero
                // local direction can be produced by a large object scale and
                // still reach the box within the camera's clipping interval.
                if (direction[axis] == 0.0F)
                {
                    if (origin[axis] < bounds.minimum[axis]
                        || origin[axis] > bounds.maximum[axis])
                    {
                        return false;
                    }
                    continue;
                }

                float first = (bounds.minimum[axis] - origin[axis])
                    / direction[axis];
                float second = (bounds.maximum[axis] - origin[axis])
                    / direction[axis];
                if (first > second)
                {
                    std::swap(first, second);
                }
                enter = (std::max)(enter, first);
                exit = (std::min)(exit, second);
                if (enter > exit)
                {
                    return false;
                }
            }
            out_t = enter;
            return std::isfinite(out_t);
        }
    }

    bool EditorMeshBounds::IsValid() const noexcept
    {
        return IsFinite3(minimum) && IsFinite3(maximum)
            && minimum[0] <= maximum[0]
            && minimum[1] <= maximum[1]
            && minimum[2] <= maximum[2];
    }

    bool EditorScenePicker::RegisterMeshBounds(
        GE::RenderCore::MeshHandle mesh,
        EditorMeshBounds bounds) noexcept
    {
        if (!mesh.IsValid() || !bounds.IsValid()
            || FindBounds(mesh) != nullptr)
        {
            return false;
        }
        try
        {
            m_entries.push_back({mesh, bounds});
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::optional<GE::Runtime::GameObjectId> EditorScenePicker::Pick(
        const GE::Runtime::World& world,
        const EditorCamera& camera,
        const std::array<float, 2>& viewport_uv,
        GE::RenderCore::RenderExtent extent) const noexcept
    {
        EditorRay world_ray;
        if (!camera.BuildRay(viewport_uv, extent, world_ray))
        {
            return std::nullopt;
        }

        const GE::Runtime::TransformComponent& camera_transform =
            camera.GetTransform();
        const GE::Runtime::CameraComponent& projection =
            camera.GetProjection();
        const GE::Runtime::SceneCameraBasis camera_basis =
            GE::Runtime::MakeSceneCameraBasis(
                camera_transform.rotation_degrees);
        const float ray_forward = Dot(world_ray.direction, camera_basis.forward);
        if (!std::isfinite(ray_forward) || ray_forward <= DirectionEpsilon)
        {
            return std::nullopt;
        }
        const float minimum_t = projection.near_plane / ray_forward;
        const float maximum_t = projection.far_plane / ray_forward;

        std::optional<GE::Runtime::GameObjectId> result;
        float closest_t = (std::numeric_limits<float>::max)();
        for (const GE::Runtime::GameObjectHandle handle
            : world.GetGameObjectHandles())
        {
            const GE::Runtime::GameObject* object = world.FindGameObject(handle);
            const GE::Runtime::MeshRendererComponent* mesh =
                object == nullptr ? nullptr : object->GetMeshRenderer();
            if (mesh == nullptr || !mesh->visible
                || !mesh->mesh.IsValid() || !mesh->material.IsValid())
            {
                continue;
            }
            const EditorMeshBounds* bounds = FindBounds(mesh->mesh);
            if (bounds == nullptr)
            {
                continue;
            }

            const GE::Runtime::TransformComponent& transform =
                object->GetTransform();
            if (!IsFinite3(transform.translation)
                || !IsFinite3(transform.rotation_degrees)
                || !IsFinite3(transform.scale)
                || std::abs(transform.scale[0]) <= ScaleEpsilon
                || std::abs(transform.scale[1]) <= ScaleEpsilon
                || std::abs(transform.scale[2]) <= ScaleEpsilon)
            {
                continue;
            }

            const GE::Runtime::SceneCameraBasis object_basis =
                GE::Runtime::MakeSceneCameraBasis(
                    transform.rotation_degrees);
            const std::array<float, 3> relative_origin{
                world_ray.origin[0] - transform.translation[0],
                world_ray.origin[1] - transform.translation[1],
                world_ray.origin[2] - transform.translation[2]};
            std::array<float, 3> local_origin{
                Dot(relative_origin, object_basis.right),
                Dot(relative_origin, object_basis.up),
                Dot(relative_origin, object_basis.forward)};
            std::array<float, 3> local_direction{
                Dot(world_ray.direction, object_basis.right),
                Dot(world_ray.direction, object_basis.up),
                Dot(world_ray.direction, object_basis.forward)};
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                local_origin[axis] /= transform.scale[axis];
                local_direction[axis] /= transform.scale[axis];
            }

            float intersection_t = 0.0F;
            if (IntersectLocalBounds(
                    local_origin,
                    local_direction,
                    *bounds,
                    minimum_t,
                    maximum_t,
                    intersection_t)
                && intersection_t < closest_t)
            {
                closest_t = intersection_t;
                result = object->GetId();
            }
        }
        return result;
    }

    std::size_t EditorScenePicker::GetMeshBoundsCount() const noexcept
    {
        return m_entries.size();
    }

    const EditorMeshBounds* EditorScenePicker::FindBounds(
        GE::RenderCore::MeshHandle mesh) const noexcept
    {
        const auto found = std::find_if(
            m_entries.begin(),
            m_entries.end(),
            [mesh](const Entry& entry)
            {
                return entry.mesh == mesh;
            });
        return found == m_entries.end() ? nullptr : &found->bounds;
    }
}
