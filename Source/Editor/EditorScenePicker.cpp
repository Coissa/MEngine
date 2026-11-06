#include "EditorScenePicker.h"

#include <Core/Math/geometry.h>
#include <Core/Math/vector.h>
#include <Runtime/GameObject.h>
#include <Runtime/SceneMath.h>
#include <Runtime/World.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace GE::Editor
{
    namespace
    {
        constexpr float ScaleEpsilon = 0.000001F;
        constexpr float DirectionEpsilon = 0.000001F;

    } // namespace

    bool EditorMeshBounds::IsValid() const noexcept
    {
        return GE::Math::Aabb3{minimum, maximum}.IsValid();
    }

    bool EditorScenePicker::RegisterMeshBounds(GE::RenderCore::MeshHandle mesh, EditorMeshBounds bounds) noexcept
    {
        if (!mesh.IsValid() || !bounds.IsValid() || FindBounds(mesh) != nullptr)
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

    std::optional<GE::Runtime::GameObjectId> EditorScenePicker::Pick(const GE::Runtime::World& world,
                                                                     const EditorCamera& camera,
                                                                     const std::array<float, 2>& viewport_uv,
                                                                     GE::RenderCore::RenderExtent extent) const noexcept
    {
        EditorRay world_ray;
        if (!camera.BuildRay(viewport_uv, extent, world_ray))
        {
            return std::nullopt;
        }

        const GE::Runtime::TransformComponent& camera_transform = camera.GetTransform();
        const GE::Runtime::CameraComponent& projection = camera.GetProjection();
        const GE::Runtime::SceneCameraBasis camera_basis =
            GE::Runtime::MakeSceneCameraBasis(camera_transform.rotation_degrees);
        const float ray_forward = GE::Math::Dot(world_ray.direction, camera_basis.forward);
        if (!std::isfinite(ray_forward) || ray_forward <= DirectionEpsilon)
        {
            return std::nullopt;
        }
        const float minimum_t = projection.near_plane / ray_forward;
        const float maximum_t = projection.far_plane / ray_forward;

        std::optional<GE::Runtime::GameObjectId> result;
        float closest_t = (std::numeric_limits<float>::max)();
        for (const GE::Runtime::GameObjectHandle handle : world.GetGameObjectHandles())
        {
            const GE::Runtime::GameObject* object = world.FindGameObject(handle);
            const GE::Runtime::MeshRendererComponent* mesh = object == nullptr ? nullptr : object->GetMeshRenderer();
            if (mesh == nullptr || !mesh->visible || !mesh->mesh.IsValid() || !mesh->material.IsValid())
            {
                continue;
            }
            const EditorMeshBounds* bounds = FindBounds(mesh->mesh);
            if (bounds == nullptr)
            {
                continue;
            }

            GE::Math::Mat4 inverse;
            if (!GE::Math::TryInverseAffine(object->GetWorldMatrix(), inverse))
                continue;
            const GE::Math::Vec3 local_origin = GE::Math::TransformPoint(world_ray.origin, inverse);
            const GE::Math::Vec3 local_direction = GE::Math::TransformVector(world_ray.direction, inverse);

            float intersection_t = 0.0F;
            if (GE::Math::IntersectRayAabb({local_origin, local_direction}, {bounds->minimum, bounds->maximum},
                                           minimum_t, maximum_t, intersection_t) &&
                intersection_t < closest_t)
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

    const EditorMeshBounds* EditorScenePicker::FindBounds(GE::RenderCore::MeshHandle mesh) const noexcept
    {
        const auto found =
            std::find_if(m_entries.begin(), m_entries.end(), [mesh](const Entry& entry) { return entry.mesh == mesh; });
        return found == m_entries.end() ? nullptr : &found->bounds;
    }
} // namespace GE::Editor
