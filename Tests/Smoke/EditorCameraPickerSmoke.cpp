#include <filesystem>
#include <Editor/EditorCamera.h>
#include <Editor/EditorCommand.h>
#include <Editor/EditorSession.h>
#include <Editor/EditorScenePicker.h>
#include <Runtime/GameObject.h>
#include <Runtime/World.h>
#include "TestRuntimeRegistries.h"
#include <Runtime/WorldContext.h>

#include <cmath>
#include <iostream>

namespace
{
    [[nodiscard]] bool Near(float left, float right, float epsilon = 0.001F) noexcept
    {
        return std::abs(left - right) <= epsilon;
    }

    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[EditorCameraPickerSmoke] Failure: " << message << '\n';
        return code;
    }

    GE::Runtime::GameObject* AddMeshObject(GE::Runtime::World& world, const char* name, float z,
                                           std::uint32_t mesh_handle = 1)
    {
        GE::Runtime::GameObject* object = world.FindGameObject(world.CreateGameObject(name));
        if (object == nullptr)
        {
            return nullptr;
        }
        object->GetTransform().translation = {0.0F, 0.0F, z};
        GE::Runtime::MeshRendererComponent* mesh = object->AddMeshRenderer();
        if (mesh == nullptr)
        {
            return nullptr;
        }
        mesh->mesh = {mesh_handle};
        mesh->material = {1};
        return object;
    }
} // namespace

int main()
{
    constexpr GE::RenderCore::RenderExtent Extent{1000, 500};
    constexpr GE::Editor::EditorMeshBounds UnitBounds{.minimum = {-1.0F, -1.0F, -1.0F}, .maximum = {1.0F, 1.0F, 1.0F}};

    GE::Editor::EditorCamera default_camera;
    GE::Editor::EditorRay center_ray;
    GE::Editor::EditorRay corner_ray;
    if (!default_camera.BuildRay({0.5F, 0.5F}, Extent, center_ray) || !Near(center_ray.origin[2], -7.0F) ||
        !Near(center_ray.direction[0], 0.0F) || !Near(center_ray.direction[1], 0.0F) ||
        !Near(center_ray.direction[2], 1.0F) || !default_camera.BuildRay({0.0F, 0.0F}, Extent, corner_ray) ||
        corner_ray.direction[0] >= 0.0F || corner_ray.direction[1] <= 0.0F || corner_ray.direction[2] <= 0.0F)
    {
        return Fail(1, "viewport ray construction is incorrect");
    }

    const auto before_move = default_camera.GetTransform().translation;
    default_camera.ApplyInput({.forward_axis = 1.0F, .right_axis = 1.0F}, 1.0F);
    const auto after_move = default_camera.GetTransform().translation;
    const float movement_length = std::sqrt((after_move[0] - before_move[0]) * (after_move[0] - before_move[0]) +
                                            (after_move[1] - before_move[1]) * (after_move[1] - before_move[1]) +
                                            (after_move[2] - before_move[2]) * (after_move[2] - before_move[2]));
    default_camera.ApplyInput({.mouse_delta_y = 10'000.0F, .wheel_delta = 10'000.0F}, 0.0F);
    if (!Near(movement_length, 5.0F) || !Near(default_camera.GetTransform().rotation_degrees[0], 89.0F) ||
        !Near(default_camera.GetMoveSpeed(), 100.0F))
    {
        return Fail(2, "camera input normalization or clamps failed");
    }

    GE::Tests::RuntimeRegistries registries;
    GE::Runtime::World camera_world{0, registries.components};
    GE::Runtime::GameObject* first_camera = camera_world.FindGameObject(camera_world.CreateGameObject("First Camera"));
    GE::Runtime::GameObject* primary_camera =
        camera_world.FindGameObject(camera_world.CreateGameObject("Primary Camera"));
    if (first_camera == nullptr || primary_camera == nullptr)
    {
        return Fail(3, "camera setup failed");
    }
    (void)first_camera->AddCamera();
    primary_camera->GetTransform().translation = {3.0F, 4.0F, 5.0F};
    primary_camera->AddCamera()->primary = true;
    default_camera.ResetFromWorld(camera_world);
    if (default_camera.GetTransform().translation != primary_camera->GetTransform().translation)
    {
        return Fail(4, "primary camera was not used to reset editor camera");
    }

    GE::Editor::EditorScenePicker picker;
    if (!picker.RegisterMeshBounds({1}, UnitBounds) || picker.RegisterMeshBounds({1}, UnitBounds) ||
        picker.RegisterMeshBounds({}, UnitBounds))
    {
        return Fail(5, "mesh bounds registration validation failed");
    }

    GE::Runtime::World picking_world{0, registries.components};
    GE::Runtime::GameObject* near_object = AddMeshObject(picking_world, "Near", -2.0F);
    GE::Runtime::GameObject* far_object = AddMeshObject(picking_world, "Far", 2.0F);
    if (near_object == nullptr || far_object == nullptr)
    {
        return Fail(6, "picking object setup failed");
    }
    GE::Editor::EditorCamera picking_camera;
    auto picked = picker.Pick(picking_world, picking_camera, {0.5F, 0.5F}, Extent);
    if (!picked.has_value() || *picked != near_object->GetId())
    {
        return Fail(7, "nearest object was not selected");
    }

    near_object->GetMeshRenderer()->visible = false;
    far_object->GetTransform().rotation_degrees = {20.0F, 35.0F, 10.0F};
    far_object->GetTransform().scale = {-1.5F, 2.0F, 0.5F};
    picked = picker.Pick(picking_world, picking_camera, {0.5F, 0.5F}, Extent);
    if (!picked.has_value() || *picked != far_object->GetId())
    {
        return Fail(8, "rotated negative-scale object was not selected");
    }

    far_object->GetTransform().scale[0] = 0.0F;
    if (picker.Pick(picking_world, picking_camera, {0.5F, 0.5F}, Extent))
    {
        return Fail(9, "zero-scale object participated in picking");
    }

    far_object->GetTransform().scale = {1.0F, 1.0F, 1.0F};
    far_object->GetMeshRenderer()->mesh = {2};
    if (picker.Pick(picking_world, picking_camera, {0.5F, 0.5F}, Extent))
    {
        return Fail(10, "object without registered bounds was selected");
    }

    GE::Runtime::World clipped_world{0, registries.components};
    GE::Runtime::GameObject* clipped_camera_object =
        clipped_world.FindGameObject(clipped_world.CreateGameObject("Clipped Camera"));
    GE::Runtime::GameObject* clipped_mesh = AddMeshObject(clipped_world, "Beyond Far", 10.0F);
    if (clipped_camera_object == nullptr || clipped_mesh == nullptr)
    {
        return Fail(11, "clipping setup failed");
    }
    clipped_camera_object->GetTransform().translation = {0.0F, 0.0F, 0.0F};
    GE::Runtime::CameraComponent* clipped_projection = clipped_camera_object->AddCamera();
    clipped_projection->primary = true;
    clipped_projection->far_plane = 5.0F;
    picking_camera.ResetFromWorld(clipped_world);
    if (picker.Pick(clipped_world, picking_camera, {0.5F, 0.5F}, Extent))
    {
        return Fail(12, "object beyond the far plane was selected");
    }

    GE::Editor::EditorSession editor{std::filesystem::path(MENGINE_TEST_ASSET_ROOT) / "Scenes/Default.mscene"};
    const GE::Runtime::EngineServices services{registries.reflection, registries.serialization, registries.components,
                                               registries.assets};
    if (!editor.RegisterMeshBounds({1}, UnitBounds) || !editor.Initialize(services))
    {
        return Fail(13, "editor setup failed");
    }
    GE::Runtime::World* editor_world = editor.GetEditorWorldContext()->GetWorld();
    editor.PickEditorObject({0.4F, 0.5F}, Extent);
    const GE::Runtime::GameObjectId editor_selection = editor.GetSelectedObjectId();
    if (!editor_selection.IsValid() || editor_world->FindGameObjectById(editor_selection) == nullptr)
    {
        return Fail(14, "editor viewport selection failed");
    }

    if (!editor.StartPlaySession() || editor.GetSelectedObjectId() != editor_selection)
    {
        return Fail(15, "PIE selection initialization failed");
    }
    const auto editor_camera_before_pie_input = editor.GetEditorCamera().GetTransform().translation;
    editor.PickEditorObject({0.6F, 0.5F}, Extent);
    editor.UpdateEditorCamera({.forward_axis = 1.0F}, 1.0F);
    if (editor.GetSelectedObjectId() != editor_selection ||
        editor.GetEditorCamera().GetTransform().translation != editor_camera_before_pie_input)
    {
        return Fail(16, "PIE accepted editor viewport input");
    }

    GE::RenderCore::RenderFrame pie_frame{.output_extent = Extent};
    if (!editor.BuildRenderFrame(pie_frame) || pie_frame.render_items.size() != 2)
    {
        return Fail(17, "PIE did not use its renderable game view");
    }
    editor.StopPlaySession();
    if (editor.GetSelectedObjectId() != editor_selection)
    {
        return Fail(18, "editor selection was not restored after PIE");
    }

    for (const GE::Runtime::GameObjectHandle handle : editor_world->GetGameObjectHandles())
    {
        GE::Runtime::GameObject* object = editor_world->FindGameObject(handle);
        if (object != nullptr && object->GetCamera() != nullptr)
        {
            (void)object->RemoveCamera();
        }
    }
    GE::RenderCore::RenderFrame editor_frame{.output_extent = Extent};
    if (!editor.BuildRenderFrame(editor_frame) || editor_frame.render_items.size() != 2)
    {
        return Fail(19, "camera-less editor world did not render with editor camera");
    }

    GE::Runtime::GameObject* selected = editor_world->FindGameObjectById(editor_selection);
    if (selected == nullptr)
    {
        return Fail(20, "selected editor object disappeared early");
    }
    GE::Editor::EditorCommand remove{.type = GE::Editor::EditorCommandType::DeleteObject,
                                     .target = selected->GetHandle()};
    if (!editor.QueueCommand(std::move(remove)))
    {
        return Fail(21, "delete command was rejected");
    }
    editor.Tick({0.0, 0.0});
    if (editor.GetSelectedObjectId().IsValid())
    {
        return Fail(22, "deleting the selected object kept stale selection");
    }

    std::string error;
    if (!editor.NewScene(error) || editor.GetSelectedObjectId().IsValid() ||
        !Near(editor.GetEditorCamera().GetTransform().translation[2], -7.0F))
    {
        return Fail(23, "new scene did not reset selection and editor camera");
    }

    editor.Shutdown();
    std::cout << "[EditorCameraPickerSmoke] Passed\n";
    return 0;
}
