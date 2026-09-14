#include "WorldRenderExtractor.h"

#include "GameObject.h"
#include "SceneMath.h"
#include "World.h"

#include <exception>

namespace GE::Runtime
{
    namespace
    {
        [[nodiscard]] RenderCore::RenderExtent GetRenderExtent(
            const RenderCore::RenderFrame& frame) noexcept
        {
            return frame.scene_extent.HasArea()
                ? frame.scene_extent : frame.output_extent;
        }

        [[nodiscard]] bool ExtractWorldView(
            const World& world,
            RenderCore::RenderFrame& out_frame) noexcept
        {
            const RenderCore::RenderExtent render_extent =
                GetRenderExtent(out_frame);
            if (!render_extent.HasArea())
            {
                return true;
            }

            const GameObject* selected_camera_object = nullptr;
            const CameraComponent* selected_camera = nullptr;
            for (const GameObjectHandle handle : world.GetGameObjectHandles())
            {
                const GameObject* object = world.FindGameObject(handle);
                const CameraComponent* camera =
                    object == nullptr ? nullptr : object->GetCamera();
                if (camera != nullptr
                    && (selected_camera == nullptr || camera->primary))
                {
                    selected_camera_object = object;
                    selected_camera = camera;
                    if (camera->primary)
                    {
                        break;
                    }
                }
            }
            if (selected_camera == nullptr
                || !IsValidPerspectiveCamera(*selected_camera))
            {
                return false;
            }

            const float aspect = static_cast<float>(render_extent.width)
                / static_cast<float>(render_extent.height);
            out_frame.main_view.view =
                MakeSceneViewMatrix(selected_camera_object->GetTransform());
            out_frame.main_view.projection =
                MakeSceneProjectionMatrix(*selected_camera, aspect);
            const Math::Vec3& camera_position =
                selected_camera_object->GetTransform().translation;
            out_frame.main_view.camera_world_position = {
                camera_position[0], camera_position[1], camera_position[2], 1.0F};
            return true;
        }

        [[nodiscard]] bool ExtractWorldItems(
            const World& world,
            RenderCore::RenderFrame& out_frame)
        {
            out_frame.render_items.clear();
            if (!GetRenderExtent(out_frame).HasArea())
            {
                return true;
            }

            const std::vector<GameObjectHandle> handles =
                world.GetGameObjectHandles();
            out_frame.render_items.reserve(handles.size());
            for (const GameObjectHandle handle : handles)
            {
                const GameObject* object = world.FindGameObject(handle);
                const MeshRendererComponent* mesh =
                    object == nullptr ? nullptr : object->GetMeshRenderer();
                if (mesh == nullptr || !mesh->visible
                    || !mesh->mesh.IsValid() || !mesh->material.IsValid())
                {
                    continue;
                }
                out_frame.render_items.push_back({
                    .mesh = mesh->mesh,
                    .material = mesh->material,
                    .local_to_world = MakeSceneLocalToWorldMatrix(
                        object->GetTransform())});
            }
            return true;
        }
    }

    bool WorldRenderExtractor::ExtractView(
        const World& world,
        RenderCore::RenderFrame& out_frame) noexcept
    {
        try
        {
            return ExtractWorldView(world, out_frame);
        }
        catch (...)
        {
            return false;
        }
    }

    bool WorldRenderExtractor::ExtractRenderItems(
        const World& world,
        RenderCore::RenderFrame& out_frame) noexcept
    {
        try
        {
            return ExtractWorldItems(world, out_frame);
        }
        catch (...)
        {
            out_frame.render_items.clear();
            return false;
        }
    }

    bool WorldRenderExtractor::Extract(
        const World& world,
        RenderCore::RenderFrame& out_frame) noexcept
    {
        out_frame.render_items.clear();
        return ExtractView(world, out_frame)
            && ExtractRenderItems(world, out_frame);
    }
}
