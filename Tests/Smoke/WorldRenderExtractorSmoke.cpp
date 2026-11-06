#include <Runtime/GameObject.h>
#include <Runtime/World.h>
#include "TestRuntimeRegistries.h"

#include <cmath>
#include <iostream>

namespace
{
    [[nodiscard]] bool Near(float left, float right) noexcept
    {
        return std::abs(left - right) < 0.0001F;
    }

    [[nodiscard]] int Fail(int code, const char* message)
    {
        std::cerr << "[WorldRenderExtractorSmoke] " << message << '\n';
        return code;
    }
} // namespace

int main()
{
    GE::Tests::RuntimeRegistries registries;
    GE::Runtime::World world{0, registries.components};
    GE::Runtime::GameObject* camera = world.FindGameObject(world.CreateGameObject("Camera"));
    GE::Runtime::GameObject* visible = world.FindGameObject(world.CreateGameObject("Visible"));
    GE::Runtime::GameObject* hidden = world.FindGameObject(world.CreateGameObject("Hidden"));
    if (camera == nullptr || visible == nullptr || hidden == nullptr)
    {
        return Fail(1, "object setup failed");
    }

    camera->GetTransform().translation = {0.0F, 0.0F, -5.0F};
    GE::Runtime::CameraComponent* camera_component = camera->AddCamera();
    camera_component->primary = true;
    camera_component->vertical_field_of_view_degrees = 60.0F;
    camera_component->near_plane = 0.1F;
    camera_component->far_plane = 100.0F;

    visible->GetTransform().translation = {2.0F, 3.0F, 4.0F};
    visible->GetTransform().scale = {2.0F, 2.0F, 2.0F};
    GE::Runtime::MeshRendererComponent* visible_mesh = visible->AddMeshRenderer();
    visible_mesh->mesh = {12};
    visible_mesh->material = {34};

    GE::Runtime::MeshRendererComponent* hidden_mesh = hidden->AddMeshRenderer();
    hidden_mesh->mesh = {56};
    hidden_mesh->material = {78};
    hidden_mesh->visible = false;

    GE::RenderCore::RenderFrame frame{
        .frame_number = 7, .output_extent = {.width = 1280, .height = 720}, .real_elapsed_seconds = 1.25};
    if (!GE::Runtime::ExtractWorldRenderFrame(world, frame))
    {
        return Fail(2, "extraction failed");
    }
    if (frame.frame_number != 7 || frame.real_elapsed_seconds != 1.25 || frame.render_items.size() != 1 ||
        frame.render_items[0].mesh.value != 12 || frame.render_items[0].material.value != 34 ||
        frame.render_items[0].object_id != visible->GetId().value || frame.main_view.camera_id != camera->GetId().value)
    {
        return Fail(3, "value snapshot contents are invalid");
    }
    if (!Near(frame.main_view.view[14], 5.0F) || !Near(frame.main_view.camera_world_position[0], 0.0F) ||
        !Near(frame.main_view.camera_world_position[1], 0.0F) ||
        !Near(frame.main_view.camera_world_position[2], -5.0F) ||
        !Near(frame.main_view.camera_world_position[3], 1.0F) || !Near(frame.render_items[0].local_to_world[0], 2.0F) ||
        !Near(frame.render_items[0].local_to_world[12], 2.0F) ||
        !Near(frame.render_items[0].local_to_world[13], 3.0F) || !Near(frame.render_items[0].local_to_world[14], 4.0F))
    {
        return Fail(4, "camera or object matrices are invalid");
    }

    camera_component->near_plane = -1.0F;
    if (GE::Runtime::ExtractWorldRenderFrame(world, frame))
    {
        return Fail(5, "invalid camera was accepted");
    }

    GE::Runtime::World no_camera{0, registries.components};
    if (GE::Runtime::ExtractWorldRenderFrame(no_camera, frame))
    {
        return Fail(6, "camera-less World was accepted");
    }

    std::cout << "[WorldRenderExtractorSmoke] Passed\n";
    return 0;
}
